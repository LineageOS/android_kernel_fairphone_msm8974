#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <asm/mach-types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include<../drivers/staging/android/timed_output.h>

#define DRV2603_VIBRATOR_EN		86
#define DRV2603_VIBRATOR_PWM		85
#define MAX_TIMEOUT			10000	/* 10s */

static struct vibrator{
	 struct wake_lock wklock;
	 struct hrtimer timer;
         struct mutex lock;
         struct work_struct work;
}vibdata;

static void drv2603_vibrator_off(void)
{
	gpio_direction_output (DRV2603_VIBRATOR_EN, 0);
	wake_unlock(&vibdata.wklock);
}
void drv2603_motor_enable(struct timed_output_dev *sdev, int value)
{
	mutex_lock(&vibdata.lock);
	hrtimer_cancel(&vibdata.timer);
	cancel_work_sync(&vibdata.work);
	if (value)
	{
		wake_lock(&vibdata.wklock);
		gpio_direction_output(DRV2603_VIBRATOR_EN, 1);
		if (value > 0)
		{
			if(value > MAX_TIMEOUT)
				value= MAX_TIMEOUT;
			hrtimer_start(&vibdata.timer,
                                     ns_to_ktime((u64)value* NSEC_PER_MSEC),
                                     HRTIMER_MODE_REL);
		 }
	}
	 else
		drv2603_vibrator_off();

         mutex_unlock(&vibdata.lock);
}

int drv2603_get_time(struct timed_output_dev *sdev)
{
	if(hrtimer_active(&vibdata.timer))
	{
		ktime_t r = hrtimer_get_remaining(&vibdata.timer);
		return ktime_to_ms(r);
	}
	return 0;
}

struct timed_output_dev drv2603_motot_driver={
	.name ="vibrator",
	.enable= drv2603_motor_enable,
	.get_time= drv2603_get_time,
};

static enum hrtimer_restart drv2603_vibrator_timer_func(struct hrtimer * timer)
{
	schedule_work(&vibdata.work);
	 return HRTIMER_NORESTART;
}

static void drv2603_vibrator_work(struct work_struct *work)
{
	drv2603_vibrator_off();
}

static int __init drv2603_motor_init(void)
{
	int ret =0;

	INIT_WORK(&vibdata.work, drv2603_vibrator_work);
	hrtimer_init(&vibdata.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibdata.timer.function= drv2603_vibrator_timer_func;

	ret =gpio_request(DRV2603_VIBRATOR_EN, "vibrator-en");
	if (ret< 0)
	{
		printk("vibratorrequest en IO err!:%d\n",ret);
		return ret;
	}
	ret = gpio_request(DRV2603_VIBRATOR_PWM, "vibrator-pwm");
	if (ret< 0)
	{
		printk("vibratorrequest pwm IO err!:%d\n",ret);
		return ret;
	}
	gpio_direction_output(DRV2603_VIBRATOR_EN, 0);
	wake_lock_init(&vibdata.wklock,WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&vibdata.lock);
	ret=timed_output_dev_register(&drv2603_motot_driver);
	if (ret< 0)
		goto err_to_dev_reg;
	return 0;
err_to_dev_reg:
	mutex_destroy(&vibdata.lock);
	wake_lock_destroy(&vibdata.wklock);
	gpio_free(DRV2603_VIBRATOR_EN);
        gpio_free(DRV2603_VIBRATOR_PWM);
	printk("vibrator   err!:%d\n",ret);
        return ret;
}

static void drv2603_motor_exit(void)
{
	mutex_destroy(&vibdata.lock);
	wake_lock_destroy(&vibdata.wklock);
	gpio_free(DRV2603_VIBRATOR_EN);
	gpio_free(DRV2603_VIBRATOR_PWM);
	printk("vibrator  exit!\n");
	timed_output_dev_register(&drv2603_motot_driver);
}

module_init(drv2603_motor_init);
module_exit(drv2603_motor_exit);

MODULE_AUTHOR("Godfather");
MODULE_DESCRIPTION("Drv2603 Vibrator driver");
MODULE_LICENSE("GPL");
