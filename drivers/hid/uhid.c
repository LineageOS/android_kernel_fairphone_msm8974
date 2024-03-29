/*
 * User-space I/O driver support for HID subsystem
 * Copyright (c) 2012 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hid.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uhid.h>
#include <linux/wait.h>

#define UHID_NAME	"uhid"
#define UHID_BUFSIZE	32

static DEFINE_MUTEX(uhid_open_mutex);

struct uhid_device {
	struct mutex devlock;
	bool running;

	__u8 *rd_data;
	uint rd_size;

	struct hid_device *hid;
	struct uhid_event input_buf;

	wait_queue_head_t waitq;
	spinlock_t qlock;
	__u8 head;
	__u8 tail;
	struct uhid_event *outq[UHID_BUFSIZE];

	/* blocking GET_REPORT support; state changes protected by qlock */
	struct mutex report_lock;
	wait_queue_head_t report_wait;
	bool report_running;
	u32 report_id;
	struct uhid_event report_buf;
	struct work_struct worker;
};

static struct miscdevice uhid_misc;

static void uhid_device_add_worker(struct work_struct *work)
{
	struct uhid_device *uhid = container_of(work, struct uhid_device, worker);
	int ret;

	ret = hid_add_device(uhid->hid);
	if (ret) {
		hid_err(uhid->hid, "Cannot register HID device: error %d\n", ret);

		hid_destroy_device(uhid->hid);
		uhid->hid = NULL;
		uhid->running = false;
	}
}

static void uhid_queue(struct uhid_device *uhid, struct uhid_event *ev)
{
	__u8 newhead;

	newhead = (uhid->head + 1) % UHID_BUFSIZE;

	if (newhead != uhid->tail) {
		uhid->outq[uhid->head] = ev;
		uhid->head = newhead;
		wake_up_interruptible(&uhid->waitq);
	} else {
		hid_warn(uhid->hid, "Output queue is full\n");
		kfree(ev);
	}
}

static int uhid_queue_event(struct uhid_device *uhid, __u32 event)
{
	unsigned long flags;
	struct uhid_event *ev;

	ev = kzalloc(sizeof(*ev), GFP_KERNEL);
	if (!ev)
		return -ENOMEM;

	ev->type = event;

	spin_lock_irqsave(&uhid->qlock, flags);
	uhid_queue(uhid, ev);
	spin_unlock_irqrestore(&uhid->qlock, flags);

	return 0;
}

static int uhid_hid_start(struct hid_device *hid)
{
	struct uhid_device *uhid = hid->driver_data;

	return uhid_queue_event(uhid, UHID_START);
}

static void uhid_hid_stop(struct hid_device *hid)
{
	struct uhid_device *uhid = hid->driver_data;

	hid->claimed = 0;
	uhid_queue_event(uhid, UHID_STOP);
}

static int uhid_hid_open(struct hid_device *hid)
{
	struct uhid_device *uhid = hid->driver_data;
	int retval = 0;

	mutex_lock(&uhid_open_mutex);
	if (!hid->open++) {
		retval = uhid_queue_event(uhid, UHID_OPEN);
		if (retval)
			hid->open--;
	}
	mutex_unlock(&uhid_open_mutex);
	return retval;
}

static void uhid_hid_close(struct hid_device *hid)
{
	struct uhid_device *uhid = hid->driver_data;

	mutex_lock(&uhid_open_mutex);
	if (!--hid->open)
		uhid_queue_event(uhid, UHID_CLOSE);
	mutex_unlock(&uhid_open_mutex);
}

static int uhid_hid_parse(struct hid_device *hid)
{
	struct uhid_device *uhid = hid->driver_data;

	return hid_parse_report(hid, uhid->rd_data, uhid->rd_size);
}

static int uhid_hid_get_raw(struct hid_device *hid, unsigned char rnum,
			    __u8 *buf, size_t count, unsigned char rtype)
{
	struct uhid_device *uhid = hid->driver_data;
	__u8 report_type;
	struct uhid_event *ev;
	unsigned long flags;
	int ret;
	size_t uninitialized_var(len);
	struct uhid_feature_answer_req *req;

	if (!uhid->running)
		return -EIO;

	switch (rtype) {
	case HID_FEATURE_REPORT:
		report_type = UHID_FEATURE_REPORT;
		break;
	case HID_OUTPUT_REPORT:
		report_type = UHID_OUTPUT_REPORT;
		break;
	case HID_INPUT_REPORT:
		report_type = UHID_INPUT_REPORT;
		break;
	default:
		return -EINVAL;
	}

	ret = mutex_lock_interruptible(&uhid->report_lock);
	if (ret)
		return ret;

	ev = kzalloc(sizeof(*ev), GFP_KERNEL);
	if (!ev) {
		ret = -ENOMEM;
		goto unlock;
	}

	spin_lock_irqsave(&uhid->qlock, flags);
	ev->type = UHID_FEATURE;
	ev->u.feature.id = ++uhid->report_id;
	ev->u.feature.rnum = rnum;
	ev->u.feature.rtype = report_type;

	uhid->report_running = true;
	uhid_queue(uhid, ev);
	spin_unlock_irqrestore(&uhid->qlock, flags);

	ret = wait_event_interruptible_timeout(uhid->report_wait,
				!uhid->report_running || !uhid->running,
				5 * HZ);

	if (!ret || !uhid->running) {
		ret = -EIO;
	} else if (ret < 0) {
		ret = -ERESTARTSYS;
	} else {
		spin_lock_irqsave(&uhid->qlock, flags);
		req = &uhid->report_buf.u.feature_answer;

		if (req->err) {
			ret = -EIO;
		} else {
			ret = 0;
			len = min(count,
				min_t(size_t, req->size, UHID_DATA_MAX));
			memcpy(buf, req->data, len);
		}

		spin_unlock_irqrestore(&uhid->qlock, flags);
	}

	uhid->report_running = false;

unlock:
	mutex_unlock(&uhid->report_lock);
	return ret ? ret : len;
}

static int uhid_hid_output_raw(struct hid_device *hid, __u8 *buf, size_t count,
			       unsigned char report_type)
{
	struct uhid_device *uhid = hid->driver_data;
	__u8 rtype;
	unsigned long flags;
	struct uhid_event *ev;

	switch (report_type) {
	case HID_FEATURE_REPORT:
		rtype = UHID_FEATURE_REPORT;
		break;
	case HID_OUTPUT_REPORT:
		rtype = UHID_OUTPUT_REPORT;
		break;
	default:
		return -EINVAL;
	}

	if (count < 1 || count > UHID_DATA_MAX)
		return -EINVAL;

	ev = kzalloc(sizeof(*ev), GFP_KERNEL);
	if (!ev)
		return -ENOMEM;

	ev->type = UHID_OUTPUT;
	ev->u.output.size = count;
	ev->u.output.rtype = rtype;
	memcpy(ev->u.output.data, buf, count);

	spin_lock_irqsave(&uhid->qlock, flags);
	uhid_queue(uhid, ev);
	spin_unlock_irqrestore(&uhid->qlock, flags);

	return count;
}

static struct hid_ll_driver uhid_hid_driver = {
	.start = uhid_hid_start,
	.stop = uhid_hid_stop,
	.open = uhid_hid_open,
	.close = uhid_hid_close,
	.parse = uhid_hid_parse,
};

static int uhid_dev_create(struct uhid_device *uhid,
			   const struct uhid_event *ev)
{
	struct hid_device *hid;
	int ret;

	if (uhid->running)
		return -EALREADY;

	uhid->rd_size = ev->u.create.rd_size;
	if (uhid->rd_size <= 0 || uhid->rd_size > HID_MAX_DESCRIPTOR_SIZE)
		return -EINVAL;

	uhid->rd_data = kmalloc(uhid->rd_size, GFP_KERNEL);
	if (!uhid->rd_data)
		return -ENOMEM;

	if (copy_from_user(uhid->rd_data, ev->u.create.rd_data,
			   uhid->rd_size)) {
		ret = -EFAULT;
		goto err_free;
	}

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_free;
	}

	strncpy(hid->name, ev->u.create.name, 127);
	hid->name[127] = 0;
	strncpy(hid->phys, ev->u.create.phys, 63);
	hid->phys[63] = 0;
	strncpy(hid->uniq, ev->u.create.uniq, 63);
	hid->uniq[63] = 0;

	hid->ll_driver = &uhid_hid_driver;
	hid->hid_get_raw_report = uhid_hid_get_raw;
	hid->hid_output_raw_report = uhid_hid_output_raw;
	hid->bus = ev->u.create.bus;
	hid->vendor = ev->u.create.vendor;
	hid->product = ev->u.create.product;
	hid->version = ev->u.create.version;
	hid->country = ev->u.create.country;
	hid->driver_data = uhid;
	hid->dev.parent = uhid_misc.this_device;

	uhid->hid = hid;
	uhid->running = true;

	ret = hid_add_device(hid);
	if (ret) {
		hid_err(hid, "Cannot register HID device\n");
		goto err_hid;
	}

	return 0;

err_hid:
	hid_destroy_device(hid);
	uhid->hid = NULL;
	uhid->running = false;
err_free:
	kfree(uhid->rd_data);
	return ret;
}

static int uhid_dev_create2(struct uhid_device *uhid,
			    const struct uhid_event *ev)
{
	struct hid_device *hid;
	size_t rd_size, len;
	void *rd_data;
	int ret;

	if (uhid->running)
		return -EALREADY;

	rd_size = ev->u.create2.rd_size;
	if (rd_size <= 0 || rd_size > HID_MAX_DESCRIPTOR_SIZE)
		return -EINVAL;

	rd_data = kmemdup(ev->u.create2.rd_data, rd_size, GFP_KERNEL);
	if (!rd_data)
		return -ENOMEM;

	uhid->rd_size = rd_size;
	uhid->rd_data = rd_data;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_free;
	}

	len = min(sizeof(hid->name), sizeof(ev->u.create2.name)) - 1;
	strncpy(hid->name, ev->u.create2.name, len);
	len = min(sizeof(hid->phys), sizeof(ev->u.create2.phys)) - 1;
	strncpy(hid->phys, ev->u.create2.phys, len);
	len = min(sizeof(hid->uniq), sizeof(ev->u.create2.uniq)) - 1;
	strncpy(hid->uniq, ev->u.create2.uniq, len);

	hid->ll_driver = &uhid_hid_driver;
	hid->bus = ev->u.create2.bus;
	hid->vendor = ev->u.create2.vendor;
	hid->product = ev->u.create2.product;
	hid->version = ev->u.create2.version;
	hid->country = ev->u.create2.country;
	hid->driver_data = uhid;
	hid->dev.parent = uhid_misc.this_device;

	uhid->hid = hid;
	uhid->running = true;

	/* Adding of a HID device is done through a worker, to allow HID drivers
	 * which use feature requests during .probe to work, without they would
	 * be blocked on devlock, which is held by uhid_char_write.
	 */
	schedule_work(&uhid->worker);

	return 0;

err_free:
	kfree(uhid->rd_data);
	uhid->rd_data = NULL;
	uhid->rd_size = 0;
	return ret;
}

static int uhid_dev_destroy(struct uhid_device *uhid)
{
	if (!uhid->running)
		return -EINVAL;

	uhid->running = false;
	wake_up_interruptible(&uhid->report_wait);

	cancel_work_sync(&uhid->worker);

	hid_destroy_device(uhid->hid);
	kfree(uhid->rd_data);

	return 0;
}

static int uhid_dev_input(struct uhid_device *uhid, struct uhid_event *ev)
{
	if (!uhid->running)
		return -EINVAL;

	hid_input_report(uhid->hid, HID_INPUT_REPORT, ev->u.input.data,
			 min_t(size_t, ev->u.input.size, UHID_DATA_MAX), 0);

	return 0;
}

static int uhid_dev_input2(struct uhid_device *uhid, struct uhid_event *ev)
{
	if (!uhid->running)
		return -EINVAL;

	hid_input_report(uhid->hid, HID_INPUT_REPORT, ev->u.input2.data,
			 min_t(size_t, ev->u.input2.size, UHID_DATA_MAX), 0);

	return 0;
}

static int uhid_dev_feature_answer(struct uhid_device *uhid,
				   struct uhid_event *ev)
{
	unsigned long flags;

	if (!uhid->running)
		return -EINVAL;

	spin_lock_irqsave(&uhid->qlock, flags);

	/* id for old report; drop it silently */
	if (uhid->report_id != ev->u.feature_answer.id)
		goto unlock;
	if (!uhid->report_running)
		goto unlock;

	memcpy(&uhid->report_buf, ev, sizeof(*ev));
	uhid->report_running = false;
	wake_up_interruptible(&uhid->report_wait);

unlock:
	spin_unlock_irqrestore(&uhid->qlock, flags);
	return 0;
}

static int uhid_char_open(struct inode *inode, struct file *file)
{
	struct uhid_device *uhid;

	uhid = kzalloc(sizeof(*uhid), GFP_KERNEL);
	if (!uhid)
		return -ENOMEM;

	mutex_init(&uhid->devlock);
	mutex_init(&uhid->report_lock);
	spin_lock_init(&uhid->qlock);
	init_waitqueue_head(&uhid->waitq);
	init_waitqueue_head(&uhid->report_wait);
	uhid->running = false;
	INIT_WORK(&uhid->worker, uhid_device_add_worker);

	file->private_data = uhid;
	nonseekable_open(inode, file);

	return 0;
}

static int uhid_char_release(struct inode *inode, struct file *file)
{
	struct uhid_device *uhid = file->private_data;
	unsigned int i;

	uhid_dev_destroy(uhid);

	for (i = 0; i < UHID_BUFSIZE; ++i)
		kfree(uhid->outq[i]);

	kfree(uhid);

	return 0;
}

static ssize_t uhid_char_read(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct uhid_device *uhid = file->private_data;
	int ret;
	unsigned long flags;
	size_t len;

	/* they need at least the "type" member of uhid_event */
	if (count < sizeof(__u32))
		return -EINVAL;

try_again:
	if (file->f_flags & O_NONBLOCK) {
		if (uhid->head == uhid->tail)
			return -EAGAIN;
	} else {
		ret = wait_event_interruptible(uhid->waitq,
						uhid->head != uhid->tail);
		if (ret)
			return ret;
	}

	ret = mutex_lock_interruptible(&uhid->devlock);
	if (ret)
		return ret;

	if (uhid->head == uhid->tail) {
		mutex_unlock(&uhid->devlock);
		goto try_again;
	} else {
		len = min(count, sizeof(**uhid->outq));
		if (copy_to_user(buffer, uhid->outq[uhid->tail], len)) {
			ret = -EFAULT;
		} else {
			kfree(uhid->outq[uhid->tail]);
			uhid->outq[uhid->tail] = NULL;

			spin_lock_irqsave(&uhid->qlock, flags);
			uhid->tail = (uhid->tail + 1) % UHID_BUFSIZE;
			spin_unlock_irqrestore(&uhid->qlock, flags);
		}
	}

	mutex_unlock(&uhid->devlock);
	return ret ? ret : len;
}

static ssize_t uhid_char_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct uhid_device *uhid = file->private_data;
	int ret;
	size_t len;

	/* we need at least the "type" member of uhid_event */
	if (count < sizeof(__u32))
		return -EINVAL;

	ret = mutex_lock_interruptible(&uhid->devlock);
	if (ret)
		return ret;

	memset(&uhid->input_buf, 0, sizeof(uhid->input_buf));
	len = min(count, sizeof(uhid->input_buf));
	if (copy_from_user(&uhid->input_buf, buffer, len)) {
		ret = -EFAULT;
		goto unlock;
	}

	switch (uhid->input_buf.type) {
	case UHID_CREATE:
		ret = uhid_dev_create(uhid, &uhid->input_buf);
		break;
	case UHID_CREATE2:
		ret = uhid_dev_create2(uhid, &uhid->input_buf);
		break;
	case UHID_DESTROY:
		ret = uhid_dev_destroy(uhid);
		break;
	case UHID_INPUT:
		ret = uhid_dev_input(uhid, &uhid->input_buf);
		break;
	case UHID_INPUT2:
		ret = uhid_dev_input2(uhid, &uhid->input_buf);
		break;
	case UHID_FEATURE_ANSWER:
		ret = uhid_dev_feature_answer(uhid, &uhid->input_buf);
		break;
	default:
		ret = -EOPNOTSUPP;
	}

unlock:
	mutex_unlock(&uhid->devlock);

	/* return "count" not "len" to not confuse the caller */
	return ret ? ret : count;
}

static unsigned int uhid_char_poll(struct file *file, poll_table *wait)
{
	struct uhid_device *uhid = file->private_data;

	poll_wait(file, &uhid->waitq, wait);

	if (uhid->head != uhid->tail)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations uhid_fops = {
	.owner		= THIS_MODULE,
	.open		= uhid_char_open,
	.release	= uhid_char_release,
	.read		= uhid_char_read,
	.write		= uhid_char_write,
	.poll		= uhid_char_poll,
	.llseek		= no_llseek,
};

static struct miscdevice uhid_misc = {
	.fops		= &uhid_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= UHID_NAME,
};

static int __init uhid_init(void)
{
	return misc_register(&uhid_misc);
}

static void __exit uhid_exit(void)
{
	misc_deregister(&uhid_misc);
}

module_init(uhid_init);
module_exit(uhid_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Herrmann <dh.herrmann@gmail.com>");
MODULE_DESCRIPTION("User-space I/O driver support for HID subsystem");
