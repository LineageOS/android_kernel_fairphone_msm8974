#include "ilitek_ts.h"
#ifdef GESTURE
static int system_resume_ilitek = 1;
#endif
u8 TOUCH_DRIVER_DEBUG_LOG_LEVEL = CONFIG_TOUCH_DRIVER_DEFAULT_LOG_LEVEL;

static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag = 0;
static int update_wait_flag = 0;
extern int ilitek_upgrade_firmware(void);

#ifdef TOOL
struct dev_data {
	dev_t devno;
	struct cdev cdev;
	struct class *class;
};

extern struct dev_data dev_ilitek;
extern struct proc_dir_entry * proc_ilitek;
extern int create_tool_node(void);
extern int remove_tool_node(void);
#endif

extern struct kobject *android_touch_kobj;
extern void ilitek_touch_node_deinit(void);
extern void ilitek_touch_node_init(void);

extern int ilitek_into_glove_mode(bool status);
extern int ilitek_into_fingersense_mode(bool status);
extern int ilitek_into_hall_halfmode(bool status);

 int touch_key_hold_press = 0;
 int touch_key_code[] = {KEY_MENU,KEY_HOMEPAGE,KEY_BACK,KEY_VOLUMEDOWN,KEY_VOLUMEUP};
 int touch_key_press[] = {0, 0, 0, 0, 0};
 unsigned long touch_time=0;
 int driver_information[] = {DERVER_VERSION_MAJOR,DERVER_VERSION_MINOR,CUSTOMER_ID,MODULE_ID,PLATFORM_ID,PLATFORM_MODULE,ENGINEER_ID};
 char Report_Flag;
 volatile char int_Flag;
 volatile char update_Flag;
 int update_timeout;

 char EXCHANG_XY = 0;
 char REVERT_X = 0;
 char REVERT_Y = 0;
 char DBG_FLAG,DBG_COR;
struct i2c_data i2c;

int ILITEK_RESET_PIN = 0;//must set
int ILITEK_IRQ_PIN = 0;//must set
#define ILITEK_JDI_MODULE 8
#define X_CHANNEL_NUM 15
#define Y_CHANNEL_NUM 27
#define MAX_X 1079
#define MAX_Y 1919
#define MAX_TOUCH_POINT 10
#define JUDGE_ZERO 0
static struct regulator *syn_power_vbus = NULL;
static struct regulator *syn_power_vdd = NULL;
static void ilitek_set_input_param(struct input_dev*, int max_tp, int max_x, int max_y);
static int ilitek_i2c_process_and_report(void);
static int ilitek_i2c_probe(struct i2c_client*, const struct i2c_device_id*);
static int ilitek_i2c_remove(struct i2c_client*);
#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ilitek_i2c_early_suspend(struct early_suspend *h);
static void ilitek_i2c_late_resume(struct early_suspend *h);
#endif
static irqreturn_t ilitek_i2c_isr(int, void*);
static int Request_IRQ(void);
static int ilitek_request_init_reset(void);
static int ilitek_init(void);
static void ilitek_exit(void);
static bool is_ilitek_powered_on(void);

#ifdef DEBUG_NETLINK
static struct sock *netlink_sock;
bool debug_flag = false;
static void udp_reply(int pid,int seq,void *payload,int size)
{
	struct sk_buff	*skb;
	struct nlmsghdr	*nlh;
	int		len = NLMSG_SPACE(size);
	void		*data;
	int ret;
	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;
	printk("ilitek udp_reply\n");
	nlh= nlmsg_put(skb, pid, seq, 0, size, 0);
	nlh->nlmsg_flags = 0;
	data=NLMSG_DATA(nlh);
	memcpy(data, payload, size);
	NETLINK_CB(skb).portid = 0;         /* from kernel */
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	ret=netlink_unicast(netlink_sock, skb, pid, MSG_DONTWAIT);
	if (ret <0)
	{
		printk("ilitek send failed\n");
		return;
	}
	return;

}

/* Receive messages from netlink socket. */
u_int	uid, pid = 100, seq = 23/*, sid*/;
static void udp_receive(struct sk_buff  *skb)
{
	void			*data;
	uint8_t buf[64] = {0};
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)skb->data;
	pid  = 100;//NETLINK_CREDS(skb)->pid;
	uid  = NETLINK_CREDS(skb)->uid;
	//sid  = NETLINK_CB(skb).sid;
	seq  = 23;//nlh->nlmsg_seq;
	data = NLMSG_DATA(nlh);
	//printk("recv skb from user space uid:%d pid:%d seq:%d,sid:%d\n",uid,pid,seq,sid);
	if(!strcmp(data,"Hello World!"))
	{
		printk("recv skb from user space uid:%d pid:%d seq:%d\n",uid,pid,seq);
		printk("data is :%s\n",(char *)data);
		udp_reply(pid,seq,data,sizeof("Hello World!"));
		//udp_reply(pid,seq,data);
	}
	else
	{
		memcpy(buf,data,64);
	}
	//kfree(data);
	return ;
}
#endif

static const struct i2c_device_id ilitek_i2c_id[] ={
	{ILITEK_I2C_DRIVER_NAME, 0}, {}
};
MODULE_DEVICE_TABLE(i2c, ilitek_i2c_id);
#ifdef CONFIG_OF
static struct of_device_id ilitek_match_table[] = {
	{ .compatible = "ilitek,2120",},
	{ .compatible = "ilitek,2139",},
	{ .compatible = "ilitek,2839",},
	{ .compatible = "ilitek,2113",},
	{ .compatible = "ilitek,2115",},
	{ .compatible = "ilitek,2116",},
	{ .compatible = "ilitek,2656",},
	{ .compatible = "ilitek,2645",},
	{ .compatible = "ilitek,2302",},
	{ .compatible = "ilitek,2303",},
	{ .compatible = "ilitek,2102",},
	{ .compatible = "ilitek,2111",},
};
#endif
static struct i2c_driver ili2120_ts_driver = {
	.probe = ilitek_i2c_probe,
	.remove = ilitek_i2c_remove,
	.driver = {
		.name = "ilitek_i2c",
		.owner = THIS_MODULE,
		#ifdef CONFIG_OF
		.of_match_table = ilitek_match_table,
		#endif
	},
	.id_table = ilitek_i2c_id,
};

void ilitek_reset(int reset_gpio)
{
	tp_log_debug("Enter ilitek_reset\n");
	gpio_direction_output(reset_gpio,1);
	mdelay(5);
	gpio_direction_output(reset_gpio,0);
	mdelay(5);
	gpio_direction_output(reset_gpio,1);
	mdelay(10);
	return;
}

int ilitek_poll_int(void)
{
	return gpio_get_value(i2c.irq_gpio);
}

static int ilitek_request_init_reset(void)
{
	s32 ret=0;
	ilitek_reset(i2c.reset_gpio);
	return ret;
}

static int Request_IRQ(void)
{
	int ret = 0;
	//ret = request_irq(i2c.client->irq, ilitek_i2c_isr, /*IRQF_TRIGGER_FALLING*/IRQF_TRIGGER_LOW | IRQF_ONESHOT, "ilitek_i2c_irq", &i2c);
	ret = request_threaded_irq(i2c.client->irq, NULL,ilitek_i2c_isr, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,"ilitek_i2c_irq", &i2c);
	return ret;
}

static void ilitek_set_input_param(struct input_dev *input,	int max_tp, int max_x, int max_y)
{
	int i;
	__set_bit(INPUT_PROP_DIRECT, input->propbit);
	input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
		input_set_abs_params(input, ABS_MT_POSITION_X, 0, max_x, 0, 0);
		input_set_abs_params(input, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
		input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
		input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
#ifdef TOUCH_PROTOCOL_B
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
		input_mt_init_slots(input, max_tp, INPUT_MT_DIRECT);
	#else
		input_mt_init_slots(input, max_tp);
	#endif
#else
		input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
#endif
#ifdef REPORT_PRESSURE
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif
	for(i=0; i<sizeof(touch_key_code) / sizeof(touch_key_code[0]); i++){
			if(touch_key_code[i] <= 0){
					continue;
		}
			set_bit(touch_key_code[i] & KEY_MAX, input->keybit);
	}
	for(i = 0; i < i2c.keycount; i++)
	{
		set_bit(i2c.keyinfo[i].id & KEY_MAX, input->keybit);
	}
	input->name = ILITEK_I2C_DRIVER_NAME;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &(i2c.client)->dev;
}

int ilitek_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs, int cnt)
{
	int ret, count=ILITEK_I2C_RETRY_COUNT;

	while(count >= 0){
		count-= 1;
		mutex_lock(&i2c.mutex);
		ret = i2c_transfer(client->adapter, msgs, cnt);
		mutex_unlock(&i2c.mutex);
		if(ret < 0){
			tp_log_err("%s:%d,  error, ret %d\n", __func__,__LINE__,ret);
			msleep(50);
			continue;
		}
		break;
	}
	return ret;
}

int ilitek_i2c_write(struct i2c_client *client, uint8_t * cmd, int length)
{
	int ret;
	struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = length, .buf = cmd,}
	};

	ret = ilitek_i2c_transfer(client, msgs, 1);
	if(ret < 0)
	{
		tp_log_err("%s, i2c write error, ret %d\n", __func__, ret);
	}
	return ret;
}

int ilitek_i2c_read(struct i2c_client *client, uint8_t *data, int length)
{
	int ret;

	struct i2c_msg msgs_ret[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = data,}
	};


	ret = ilitek_i2c_transfer(client, msgs_ret, 1);
	if(ret < 0){
		tp_log_err("%s, i2c read error, ret %d\n", __func__, ret);
	}

	return ret;
}

int ilitek_i2c_write_and_read(struct i2c_client *client, uint8_t *cmd,
		int write_len, int delay, uint8_t *data, int read_len)
{
	int ret = 0;
	struct i2c_msg msgs_send[] = {
		{.addr = client->addr, .flags = 0, .len = write_len, .buf = cmd,},
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};
	struct i2c_msg msgs_receive[] = {
		{.addr = client->addr, .flags = I2C_M_RD, .len = read_len, .buf = data,}
	};
	if (read_len == 0) {
		if (write_len > 0) {
			ret = ilitek_i2c_transfer(client, msgs_send, 1);
			if(ret < 0)
			{
				tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
			}
		}
		if(delay > 1)
			msleep(delay);
	}
	else if (write_len == 0) {
		if(read_len > 0){
			ret = ilitek_i2c_transfer(client, msgs_receive, 1);
			if(ret < 0)
			{
				tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
			}
		}
	}
	else if (delay > 0) {
		if (write_len > 0) {
			ret = ilitek_i2c_transfer(client, msgs_send, 1);
			if(ret < 0)
			{
				tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
			}
		}
		if(delay > 1)
			msleep(delay);
		if(read_len > 0){
			ret = ilitek_i2c_transfer(client, msgs_receive, 1);
			if(ret < 0)
			{
				tp_log_err("%s, i2c read error, ret = %d\n", __func__, ret);
			}
		}
	}
	else {
		ret = ilitek_i2c_transfer(client, msgs_send, 2);
		if(ret < 0)
		{
			tp_log_err("%s, i2c write error, ret = %d\n", __func__, ret);
		}
	}
	return ret;
}

static int ilitek_i2c_process_and_report(void)
{
	int i, j = 0, len = 0, ret, x = 0, y = 0, tp_status = 0;
#ifdef REPORT_PRESSURE
	int pressure = 0;
#endif
#ifndef TOUCH_PROTOCOL_B
	int release_counter = 0;
#endif
	unsigned char buf[128]={0};
	if(i2c.report_status == 0){
		return 1;
	}
	buf[0] = ILITEK_TP_CMD_READ_DATA;
	ret = ilitek_i2c_write_and_read(i2c.client, buf, 1, 0, buf, 53);
	len = buf[0];
	if (ret < 0) {
		tp_log_info("ilitek ILITEK_TP_CMD_READ_DATA error return & release\n");
#ifdef TOUCH_PROTOCOL_B
		for(i = 0; i < i2c.max_tp; i++)
		{
			if(i2c.touchinfo[i].flag == 1)
			{
				tp_log_info("ilitek release point i = %d\n", i);

				input_mt_slot(i2c.input_dev, i);
				input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
			}
			i2c.touchinfo[i].flag = 0;
		}
#else
		if(i2c.touch_flag == 1)
		{
			i2c.touch_flag = 0;
			tp_log_debug("Release, ID=%02X, X=%04d, Y=%04d\n", buf[0] & 0x3F, x, y);

			input_report_key(i2c.input_dev, BTN_TOUCH,	0);
			input_mt_sync(i2c.input_dev);
		}
#endif
		return ret;
	}
	else {
		ret = 0;
	}
#ifdef DEBUG_NETLINK
		if (debug_flag) {
			udp_reply(pid,seq,buf,sizeof(buf));
		}
		if (buf[1] == 0x5F) {
			return 0;
		}
#endif
	len = buf[0];
	tp_log_debug("ilitek len = 0x%x buf[0] = 0x%x, buf[1] = 0x%x, buf[2] = 0x%x\n", len, buf[0], buf[1], buf[2]);
	if (len > 20) {
		tp_log_info("ilitek len > 20  return & release\n");
		input_report_key(i2c.input_dev, BTN_TOUCH,0);
#ifdef TOUCH_PROTOCOL_B
		for(i = 0; i < i2c.max_tp; i++)
		{
			if(i2c.touchinfo[i].flag == 1)
			{
				tp_log_debug("ilitek release point i = %d\n", i);

				input_mt_slot(i2c.input_dev, i);
				input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
			}
			i2c.touchinfo[i].flag = 0;
		}
#else
		if(i2c.touch_flag == 1)
		{
			i2c.touch_flag = 0;
			tp_log_debug("Release, ID=%02X, X=%04d, Y=%04d\n", buf[0] & 0x3F, x, y);

			input_mt_sync(i2c.input_dev);
		}
#endif
		return ret;
	}
#ifdef GESTURE
	if (system_resume_ilitek == 0) {
		tp_log_info("gesture wake up 0x%x, 0x%x, 0x%x\n", buf[0], buf[1], buf[2]);
		if (buf[2] == 0x60) {
			tp_log_info("gesture wake up this is c\n");
			input_report_key(i2c.input_dev, KEY_C, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_C, 0);
			input_sync(i2c.input_dev);
		}
		if (buf[2] == 0x62) {
			tp_log_info("gesture wake up this is e\n");
			input_report_key(i2c.input_dev, KEY_E, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_E, 0);
			input_sync(i2c.input_dev);
		}
		if (buf[2] == 0x64) {
			tp_log_info("gesture wake up this is m\n");
			input_report_key(i2c.input_dev, KEY_M, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_M, 0);
			input_sync(i2c.input_dev);
		}
		if (buf[2] == 0x66) {
			tp_log_info("gesture wake up this is w\n");
			input_report_key(i2c.input_dev, KEY_W, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_W, 0);
			input_sync(i2c.input_dev);
		}
		if (buf[2] == 0x68) {
			tp_log_info("gesture wake up this is o\n");
			input_report_key(i2c.input_dev, KEY_O, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_O, 0);
			input_sync(i2c.input_dev);
		}
		if (buf[2] == 0x22) {
			tp_log_info("gesture wake up this is double click\n");
			#if 0
			input_report_key(i2c.input_dev, KEY_O, 1);
			input_sync(i2c.input_dev);
			input_report_key(i2c.input_dev, KEY_O, 0);
			input_sync(i2c.input_dev);
			#endif
		}
		input_report_key(i2c.input_dev, KEY_POWER, 1);
		input_sync(i2c.input_dev);
		input_report_key(i2c.input_dev, KEY_POWER, 0);
		input_sync(i2c.input_dev);
		return 0;
	}
#endif
	for(i = 0; i < i2c.max_tp; i++){
		tp_status = buf[i*5+3] >> 7;
		x = (((int)(buf[i*5+3] & 0x3F) << 8) + buf[i*5+4]);
		y = (((int)(buf[i*5+5] & 0x3F) << 8) + buf[i*5+6]);
#ifdef REPORT_PRESSURE
		pressure = buf[i * 5 + 7];
#endif
		if (tp_status) {
			tp_log_debug("ilitek TOUCH_POINT  i = %d  \n", i);
			if(i2c.keyflag == 0){
				for(j = 0; j < i2c.keycount; j++){
					if((x >= i2c.keyinfo[j].x && x <= i2c.keyinfo[j].x + i2c.key_xlen) && (y >= i2c.keyinfo[j].y && y <= i2c.keyinfo[j].y + i2c.key_ylen)){
						input_report_key(i2c.input_dev,  i2c.keyinfo[j].id, 1);
						i2c.keyinfo[j].status = 1;
						touch_key_hold_press = 1;
						i2c.touchinfo[j].flag = 0;
						i2c.keyflag = 1;
						tp_log_debug("Key, Keydown ID=%d, X=%d, Y=%d, key_status=%d,keyflag=%d\n", i2c.keyinfo[j].id ,x ,y , i2c.keyinfo[j].status,i2c.keyflag);
						break;
					}
				}
			}
			tp_log_debug("Point, ID=%02X, X=%04d, Y=%04d,touch_key_hold_press=%d\n",buf[0]  & 0x3F, x,y,touch_key_hold_press);
			if (touch_key_hold_press == 0) {
				if (x > i2c.max_x || y > i2c.max_y) {
					tp_log_info("Point (x > i2c.max_x || y > i2c.max_y) , ID=%02X, X=%04d, Y=%04d,release_flag[%d]=%d,tp_status=%d,keyflag=%d\n",i, x,y,i,i2c.touchinfo[j].flag,tp_status,i2c.keyflag);
				}
				else {
					i2c.keyflag = 1;
#ifdef TOUCH_PROTOCOL_B
					input_report_key(i2c.input_dev, BTN_TOUCH,  1);
					input_mt_slot(i2c.input_dev, i);
					input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, true);
					i2c.touchinfo[i].flag = 1;
#endif
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_X, x);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_POSITION_Y, y);
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 1);
#ifdef REPORT_PRESSURE
					input_event(i2c.input_dev, EV_ABS, ABS_MT_PRESSURE, pressure);
#endif
#ifndef TOUCH_PROTOCOL_B
					input_event(i2c.input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
					input_report_key(i2c.input_dev, BTN_TOUCH, 1);
					input_mt_sync(i2c.input_dev);
					i2c.touch_flag = 1;
#endif
				}
			}
			if(touch_key_hold_press == 1){
				for(j = 0; j < i2c.keycount; j++){
					if((i2c.keyinfo[j].status == 1) && (x < i2c.keyinfo[j].x || x > i2c.keyinfo[j].x + i2c.key_xlen || y < i2c.keyinfo[j].y || y > i2c.keyinfo[j].y + i2c.key_ylen)){
						input_report_key(i2c.input_dev,  i2c.keyinfo[j].id, 0);
						i2c.keyinfo[j].status = 0;
						touch_key_hold_press = 0;
						i2c.touchinfo[j].flag = 0;
						i2c.keyflag = 0;
						tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n", i2c.keyinfo[j].id ,x ,y , i2c.keyinfo[j].status);
						break;
					}
				}
			}
			ret = 0;
		}
		else {
			tp_log_debug("ilitek RELEASE_POINT  i = %d  \n", i);
#ifdef TOUCH_PROTOCOL_B
			if(i2c.touchinfo[i].flag == 1)
			{
				tp_log_debug("ilitek release point okokok i = %d\n", i);

				input_mt_slot(i2c.input_dev, i);
				input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
				i2c.touchinfo[i].flag = 0;
			}
#else
			release_counter++;
#endif
		}
	}
	if(len == 0)
	{
		i2c.keyflag = 0;
#ifdef TOUCH_PROTOCOL_B
		input_report_key(i2c.input_dev, BTN_TOUCH,  0);
		for(i = 0; i < i2c.max_tp; i++)
		{
			if(i2c.touchinfo[i].flag == 1)
			{
				tp_log_debug("ilitek release point i = %d\n", i);

				input_mt_slot(i2c.input_dev, i);
				input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
			}
			i2c.touchinfo[i].flag = 0;
		}
#else
		if(i2c.touch_flag == 1)
		{
			i2c.touch_flag = 0;
			tp_log_debug("Release, ID=%02X, X=%04d, Y=%04d\n", buf[0] & 0x3F, x, y);

			input_report_key(i2c.input_dev, BTN_TOUCH,  0);
			input_mt_sync(i2c.input_dev);
		}
#endif
	if(touch_key_hold_press == 1){
		for(j = 0; j < i2c.keycount; j++){
			if((i2c.keyinfo[j].status == 1) && (x < i2c.keyinfo[j].x || x > i2c.keyinfo[j].x + i2c.key_xlen || y < i2c.keyinfo[j].y || y > i2c.keyinfo[j].y + i2c.key_ylen)){
				input_report_key(i2c.input_dev,  i2c.keyinfo[j].id, 0);
				i2c.keyinfo[j].status = 0;
				touch_key_hold_press = 0;
				i2c.touchinfo[j].flag = 0;
				i2c.keyflag = 0;
				tp_log_debug("Key, Keyout ID=%d, X=%d, Y=%d, key_status=%d\n", i2c.keyinfo[j].id ,x ,y , i2c.keyinfo[j].status);
				break;
			}
		}
	}
	}
	tp_log_debug("%s,ret=%d\n",__func__,ret);

	input_sync(i2c.input_dev);
	return ret;
}

static irqreturn_t ilitek_i2c_isr(int irq, void *dev_id)
{
	tp_log_info("ENTER %s\n",__func__);

	if (!is_ilitek_powered_on())
	{
		return IRQ_HANDLED;
	}

	if(i2c.firmware_updating) {
		tp_log_info("%s: tp fw is updating,return\n", __func__);
		return IRQ_HANDLED;
	}
	if(i2c.irq_status ==1){
		disable_irq_nosync(i2c.client->irq);
		tp_log_debug("ENTER disable irq no sync%s\n",__func__);
		i2c.irq_status = 0;
	}
	if(update_Flag == 1){
		int_Flag = 1;
	}
	else{
		tpd_flag = 1;
		wake_up_interruptible(&waiter);
	}

	return IRQ_HANDLED;
}

static int ilitek_i2c_touchevent_thread(void *arg)
{
	int ret=0;
	struct sched_param param = { .sched_priority = 4};
	sched_setscheduler(current, SCHED_RR, &param);
	tp_log_info("%s, enter\n", __func__);

	while(1){
		tp_log_debug("%s, enter tpd_flag = %d\n", __func__, tpd_flag);
		if(kthread_should_stop()){
			tp_log_info("%s, stop\n", __func__);
			break;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, tpd_flag != 0);
		tpd_flag = 0;
		set_current_state(TASK_RUNNING);
		if(ilitek_i2c_process_and_report() < 0){
			tp_log_err("%s, process error\n", __func__);
		}
		if (i2c.function_ctrl == 0) {
			if (i2c.glove_status) {
				ilitek_into_glove_mode(true);
			}
			if (i2c.ilitek_roi_enabled) {
				ilitek_into_fingersense_mode(true);
			}
			if (i2c.hall_status) {
				ilitek_into_hall_halfmode(true);
			}
			i2c.function_ctrl = 1;
		}
		ilitek_i2c_irq_enable();
	}
	tp_log_info("%s, exit\n", __func__);
	return ret;
}
#ifdef ILI_UPDATE_FW
static int ilitek_i2c_update_thread(void *arg)
{

	int ret=0;
	tp_log_info("%s, enter\n", __func__);

	if(kthread_should_stop()){
		tp_log_info("%s, stop\n", __func__);
		return -1;
	}

	//disable_irq(i2c.client->irq);

	update_wait_flag = 1;
	i2c.firmware_updating = true;
	ret = ilitek_upgrade_firmware();
	if(ret==2) {
		tp_log_info("ilitek update end\n");
	}
	else if(ret==3) {
		tp_log_info("ilitek i2c communication error\n");
	}

	i2c.firmware_updating = false;
	ret=ilitek_i2c_read_tp_info();
	if(ret < 0)
	{
		return ret;
	}

	//enable_irq(i2c.client->irq);
	update_wait_flag = 0;
	tp_log_info("%s, IRQ: 0x%X\n", __func__, (i2c.client->irq));
	if(i2c.client->irq != 0 ){
		if(Request_IRQ()){
			tp_log_err("%s, request irq, error\n", __func__);
		}else{
			i2c.valid_irq_request = 1;
			i2c.irq_status = 1;
			tp_log_err("%s, request irq, success\n", __func__);
		}
	}
	ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_x, i2c.max_y);
	ret = input_register_device(i2c.input_dev);
	if(ret){
		tp_log_info("%s, register input device, error\n", __func__);
		return ret;
	}
	tp_log_info("%s, register input device, success\n", __func__);
	return ret;
}
#endif

#if defined(CONFIG_FB)
static void ilitek_ts_suspend(struct i2c_data *ts)
{
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
	tp_log_info("%s\n", __func__);
}

static void ilitek_ts_resume(struct i2c_data *ts)
{
	ilitek_i2c_resume(i2c.client);
	tp_log_info("%s\n", __func__);
}

static int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct i2c_data *ts =
		container_of(self, struct i2c_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts && ts->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			tp_log_info("fb_notifier_callback ilitek_ts_resume\n");
			ilitek_ts_resume(ts);
		}
		else if (*blank == FB_BLANK_POWERDOWN) {
			tp_log_info("fb_notifier_callback ilitek_ts_suspend\n");
			ilitek_ts_suspend(ts);
		}
	}
	return 0;
}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ilitek_i2c_early_suspend(struct early_suspend *h)
{
	ilitek_i2c_suspend(i2c.client, PMSG_SUSPEND);
	tp_log_info("%s\n", __func__);
}

static void ilitek_i2c_late_resume(struct early_suspend *h)
{
	ilitek_i2c_resume(i2c.client);
	tp_log_info("%s\n", __func__);
}
#endif

void ilitek_i2c_irq_enable(void)
{
	if (i2c.irq_status == 0){
		i2c.irq_status = 1;
		enable_irq(i2c.client->irq);
		tp_log_debug("ilitek_i2c_irq_enable ok.\n");
	}
	else {
		tp_log_debug("no enable\n");
	}
}

void ilitek_i2c_irq_disable(void)
{
	if (i2c.irq_status == 1){
		i2c.irq_status = 0;
		disable_irq(i2c.client->irq);
		tp_log_debug("ilitek_i2c_irq_disable ok.\n");
	}
	else {
		tp_log_debug("no disable\n");
	}
}

static unsigned char read_flash_data(unsigned int address) {
	int temp = 0, ret = 0;
	int i = 0;
	unsigned char buf;
	temp = (address << 8) + REG_FLASH_CMD_MEMORY_READ;
	ret = outwrite(REG_FLASH_CMD, temp, 4);
	ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
	for (i = 0; i < 50; i++) {
		ret = inwrite(REG_CHK_FLAG);
		if (ret & 0x01) {
			msleep(2);
		}
		else {
			break;
		}
	}
	buf = (unsigned char)(inwrite(FLASH_READ_DATA));
	return buf;
}

int ilitek_i2c_read_tp_info( void)
{
	unsigned char buf[64] = {0};
	int i = 0;
	tp_log_debug("%s, driver version:%d.%d.%d\n", __func__, driver_information[0], driver_information[1], driver_information[2]);

	for (i = 0; i < 3; i++) {
		buf[0] = ILITEK_TP_CMD_READ_DATA;
		if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 10, buf, 3) < 0)
		{
			return -TRANSMIT_ERROR;
		}
		tp_log_info("%s, 0x10 cmd read data :%X %X %X\n", __func__, buf[0], buf[1], buf[2]);
		if (buf[1] >= FW_OK) {
			break;
		}
		else {
			msleep(3);
		}
	}
	if (i >= 3) {
		tp_log_info("%s, 0x10 cmd read data error:%X %X %X\n", __func__, buf[0], buf[1], buf[2]);
	}

	buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
	buf[1] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 2, 10, buf, 0) < 0)
	{
		return -TRANSMIT_ERROR;
	}
	buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 0, buf, 3) < 0)
	{
		return -TRANSMIT_ERROR;
	}
	tp_log_info("%s, firmware version:%d.%d.%d\n", __func__, buf[0], buf[1], buf[2]);
	i2c.firmware_ver[0] = 0;
	for(i = 1; i < 4; i++)
	{
		i2c.firmware_ver[i] = buf[i - 1];
		if (i2c.firmware_ver[i] == 0xFF) {
			tp_log_info("%s, firmware version:[%d] = 0xFF so set 0 \n", __func__, i);
			i2c.firmware_ver[1] = 0;
			i2c.firmware_ver[2] = 0;
			i2c.firmware_ver[3] = 0;
			break;
		}
	}

	buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
	buf[1] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 2, 10, buf, 0) < 0)
	{
		return -TRANSMIT_ERROR;
	}

	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 0, buf, 2) < 0)
	{
		return -TRANSMIT_ERROR;
	}
	i2c.protocol_ver = (((int)buf[0]) << 8) + buf[1];
	tp_log_info("%s, protocol version:%d.%d\n", __func__, buf[0], buf[1]);

	buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
	buf[1] = 0x20;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 2, 10, buf, 0) < 0)
	{
		return -TRANSMIT_ERROR;
	}
	buf[0] = 0x20;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 0, buf, 10) < 0)
	{
		return -TRANSMIT_ERROR;
	}

	i2c.max_x = buf[2];
	i2c.max_x+= ((int)buf[3]) * 256;

	if (JUDGE_ZERO == i2c.max_x)
	{
		i2c.max_x = MAX_X;
	}

	i2c.max_y = buf[4];
	i2c.max_y+= ((int)buf[5]) * 256;

	if (JUDGE_ZERO == i2c.max_y)
	{
		i2c.max_y = MAX_Y;
	}

	i2c.min_x = buf[0];
	i2c.min_y = buf[1];
	i2c.x_ch = buf[6];

	if (JUDGE_ZERO == i2c.x_ch)
	{
		i2c.x_ch = X_CHANNEL_NUM;
	}

	i2c.y_ch = buf[7];

	if (JUDGE_ZERO == i2c.y_ch)
	{
		i2c.y_ch = Y_CHANNEL_NUM;
	}

	i2c.max_tp = buf[8];

	if (JUDGE_ZERO == i2c.max_tp)
	{
		i2c.max_tp = MAX_TOUCH_POINT;
	}

	i2c.keycount = buf[9];

	buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
	buf[1] = 0xF8;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 2, 10, buf, 0) < 0)
	{
		return -TRANSMIT_ERROR;
	}
	buf[0] = 0xF8;
	if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 10, buf, 1) < 0)
	{
		return -1;
	}
	if (buf[0] != 0) {
		i2c.x_allnode_ch = buf[0];
	} else {
		i2c.x_allnode_ch = i2c.x_ch;
	}
	if (i2c.keycount > 0) {
		buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
		buf[1] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
		if(ilitek_i2c_write_and_read(i2c.client, buf, 2, 10, buf, 0) < 0)
		{
			return -TRANSMIT_ERROR;
		}
		buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
		if(ilitek_i2c_write_and_read(i2c.client, buf, 1, 10, buf, 29) < 0)
		{
			tp_log_err("%s, ilitek read ILITEK_TP_CMD_GET_KEY_INFORMATION err\n", __func__);
			return -1;
		}
		if (i2c.keycount > 5) {
			for (i = 0; i < ((i2c.keycount % 5) ? (((i2c.keycount - 5) / 5) + 1) : ((i2c.keycount - 5) / 5)); i++) {
				tp_log_info("%s, ilitek i = %d\n", __func__, i);
				if(ilitek_i2c_write_and_read(i2c.client, buf, 0, 10, buf + 29 + 25 * i, 25) < 0)
				{
					tp_log_err("%s, ilitek read ILITEK_TP_CMD_GET_KEY_INFORMATION err\n", __func__);
					return -1;
				}
			}
		}

		i2c.key_xlen = (buf[0] << 8) + buf[1];
		i2c.key_ylen = (buf[2] << 8) + buf[3];
		tp_log_info("%s, key_xlen: %d, key_ylen: %d\n", __func__, i2c.key_xlen, i2c.key_ylen);

		//print key information
		for(i = 0; i < i2c.keycount; i++){
			i2c.keyinfo[i].id = buf[i*5+4];
			i2c.keyinfo[i].x = (buf[i*5+5] << 8) + buf[i*5+6];
			i2c.keyinfo[i].y = (buf[i*5+7] << 8) + buf[i*5+8];
			i2c.keyinfo[i].status = 0;
			tp_log_info("%s, key_id: %d, key_x: %d, key_y: %d, key_status: %d\n", __func__, i2c.keyinfo[i].id, i2c.keyinfo[i].x, i2c.keyinfo[i].y, i2c.keyinfo[i].status);
		}
	}
	tp_log_info("%s, min_x: %d, max_x: %d, min_y: %d, max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d x_allnode_ch:%d\n"
			, __func__, i2c.min_x, i2c.max_x, i2c.min_y, i2c.max_y, i2c.x_ch, i2c.y_ch, i2c.max_tp, i2c.keycount,i2c.x_allnode_ch);
	return 0;
}

static int ilitek_request_io_port(struct i2c_client *client)
{
	int err = 0;
#ifdef CONFIG_OF
	struct device *dev = &(client->dev);
	struct device_node *np = dev->of_node;

	i2c.reset_gpio = of_get_named_gpio_flags(np, "ilitek,reset-gpio",0, &i2c.reset_gpio_flags);
	if (i2c.reset_gpio < 0)
	{
		tp_log_err("rst_gpio = %d\n",i2c.reset_gpio);
		return i2c.reset_gpio;
	}

	i2c.irq_gpio = of_get_named_gpio_flags(np, "ilitek,irq-gpio",0, &i2c.irq_gpio_flags);
	if (i2c.irq_gpio < 0)
	{
		tp_log_err("irq_gpio = %d\n",i2c.irq_gpio);
		return i2c.irq_gpio;
	}
	#if 0
	if (of_find_property(np, "chipone,vci-gpio", NULL)) {
		i2c.vci_gpio = of_get_named_gpio_flags(np, "chipone,vci-gpio", 0, NULL);
		err = gpio_request(i2c.vci_gpio, "vci-gpio");
		if (err){
			tp_log_err("Failed to request vci_gpio \n");
			return err;
		}

		err = gpio_direction_output(i2c.vci_gpio, 1);
		if (err) {
			tp_log_err("Failed to direction output vci_gpio GPIO err");
			return err;
		}
	}else {
		i2c.vci_gpio = -1;
	}
	#endif
#else
	i2c.reset_gpio = ILITEK_RESET_PIN;
	i2c.irq_gpio = ILITEK_IRQ_PIN;
	if (i2c.reset_gpio == 0)
	{
		tp_log_err("rst_gpio = %d\n",i2c.reset_gpio);
		return -1;
	}
	if (i2c.irq_gpio == 0)
	{
		tp_log_err("rst_gpio = %d\n",i2c.irq_gpio);
		return -1;
	}
#endif
	err= gpio_request(i2c.reset_gpio, "reset-gpio");
	if (err) {
		tp_log_err("Failed to request reset_gpio \n");
		return err;
	}
	err = gpio_direction_output(i2c.reset_gpio, 0);
	if (err) {
		tp_log_err("Failed to direction output rest GPIO err");
		return err;
	}
	msleep(20);
	gpio_set_value_cansleep(i2c.reset_gpio, 1);

	err = gpio_request(i2c.irq_gpio, "irq-gpio");
	if (err < 0)
	{
		tp_log_err("Failed to request irq_gpio\n");
		return err;
	}
	err = gpio_direction_input(i2c.irq_gpio);
	if (err) {
		tp_log_err("Failed to direction output rest GPIO err");
		return err;
	}
	i2c.client->irq  = gpio_to_irq(i2c.irq_gpio);

	return err;
}

static int ilitek_power_on(void)
{
#ifdef CONFIG_OF
	char const *power_pin_vdd = NULL;
	char const *power_pin_vbus = NULL;
	int rc = 0;
	struct i2c_client *client = i2c.client;
	struct device *dev = &(client->dev);
	rc = of_property_read_string(dev->of_node,"ilitek,vdd", &power_pin_vdd);
	if (rc) {
		tp_log_err("%s(%d): OF error vdd rc=%d\n", __func__, __LINE__, rc);
	}
	else {
		tp_log_info("%s, ilitek power_pin_vdd = %s\n", __func__, power_pin_vdd);
	}

	rc = of_property_read_string(dev->of_node,"ilitek,vbus", &power_pin_vbus);
	if (rc) {
		tp_log_err("%s(%d): OF error vbus rc=%d\n", __func__, __LINE__, rc);
	}
	else {
		tp_log_info("%s, ilitek power_pin_vbus = %s\n", __func__, power_pin_vbus);
	}

	rc = 0;
	if (rc == 0) {
		tp_log_info("ilitek test return\n");
		return 0;
	}
	if(power_pin_vdd)
	{
		i2c.vdd = regulator_get(dev, power_pin_vdd);
		if (IS_ERR(i2c.vdd)) {
			tp_log_err("%s: vdd_ilitek regulator get fail, rc=%d\n", __func__, rc);
			return  -EINVAL;
		}

		syn_power_vdd = i2c.vdd;

		rc = regulator_set_voltage(i2c.vdd, 2850000, 2850000);
		if(rc < 0){
			tp_log_err( "%s: vdd_ilitek regulator set fail, rc=%d\n", __func__, rc);
			return  -EINVAL;
		}

		rc = regulator_enable(i2c.vdd);
		if (rc < 0) {
			tp_log_err( "%s: vdd_ilitek regulator enable fail, rc=%d\n", __func__, rc);
			return -EINVAL;
		}
	}

	if(power_pin_vbus)
	{
		i2c.vcc_i2c = regulator_get(dev, power_pin_vbus);
		if (IS_ERR(i2c.vcc_i2c)) {
			tp_log_err( "%s: vbus_ilitek regulator get fail, rc=%d\n", __func__, rc);
			return -EINVAL;
		}

		syn_power_vbus = i2c.vcc_i2c;

		rc = regulator_set_voltage(i2c.vcc_i2c,1800000,1800000);
		if(rc < 0){
			tp_log_err( "%s: vbus_ilitek regulator set fail, rc=%d\n", __func__, rc);
			return -EINVAL;
		}

		rc = regulator_enable(i2c.vcc_i2c);
		if (rc < 0) {
			tp_log_err( "%s: vbus_ilitek regulator enable fail, rc=%d\n", __func__, rc);
			return -EINVAL;
		}
	}
#endif
	return 0;
}

int ilitek_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret = 0;
#ifdef GESTURE
	uint8_t cmd[2] = {0};
	struct i2c_msg msgs_cmd[] = {
		{.addr = client->addr, .flags = 0, .len = 2, .buf = cmd,},
	};
	system_resume_ilitek = 0;

	if (update_wait_flag == 1) {
		tp_log_info("%s, ilitek waiting update so return\n", __func__);
		return 0;
	}

	if(i2c.firmware_updating) {
		tp_log_info("%s: tp fw is updating,return\n", __func__);
		return 0;
	}

	if (!is_ilitek_powered_on())
	{
		return 0;
	}

	tp_log_info("Enter ilitek_i2c_suspend 0x01 0x00, 0x0A 0x01\n");
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
	if(ret < 0){
		tp_log_err("%s, 0x01 0x00 set tp suspend err, ret %d\n", __func__, ret);
	}
	msleep(10);
	cmd[0] = 0x0A;
	cmd[1] = 0x01;
	ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
	if(ret < 0){
		tp_log_err("%s, 0x0A 0x01 set tp suspend err, ret %d\n", __func__, ret);
	}
	enable_irq_wake(i2c.client->irq);
#else
	uint8_t cmd[2] = {0};
	struct i2c_msg msgs_cmd[] = {
		{.addr = client->addr, .flags = 0, .len = 2, .buf = cmd,},
	};

	if (update_wait_flag == 1) {
		tp_log_info("%s, ilitek waiting update so return\n", __func__);
		return 0;
	}

	tp_log_info("Enter %s i2c.valid_irq_request=%d i2c.reset_request_success=%d\n",__FUNCTION__,i2c.valid_irq_request,i2c.reset_request_success);
	if(i2c.valid_irq_request != 0){
		ilitek_i2c_irq_disable();
	}
	else{
		i2c.stop_polling = 1;
		tp_log_info("%s, stop i2c thread polling\n", __func__);
	}

	if(i2c.reset_request_success){
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
		if(ret < 0){
			tp_log_err("%s, 0x01 0x00 set tp suspend err, ret %d\n", __func__, ret);
		}
		msleep(10);
		cmd[0] = ILITEK_TP_CMD_SLEEP;
		cmd[1] = 0x00;
		ret = ilitek_i2c_transfer(client, msgs_cmd, 1);
		if(ret < 0){
			tp_log_err("%s, 0x02 0x00 set tp suspend err, ret %d\n", __func__, ret);
		}
	}
#endif

#ifdef TOUCH_PROTOCOL_B
	input_report_key(i2c.input_dev, BTN_TOUCH,	0);
	for(ret = 0; ret < i2c.max_tp; ret++)
	{
		if(i2c.touchinfo[ret].flag == 1)
		{
			tp_log_info("ilitek release point i = %d\n", ret);
			input_mt_slot(i2c.input_dev, ret);
			input_mt_report_slot_state(i2c.input_dev, MT_TOOL_FINGER, false);
		}
		i2c.touchinfo[ret].flag = 0;
	}
#else
	if(i2c.touch_flag == 1)
	{
		i2c.touch_flag = 0;
		tp_log_debug("Release, ID=%02X, X=%04d, Y=%04d\n", buf[0] & 0x3F, x, y);
		input_report_key(i2c.input_dev, BTN_TOUCH,	0);
		input_mt_sync(i2c.input_dev);
	}
#endif
	tp_log_debug("%s,ret=%d\n",__func__,ret);
	input_sync(i2c.input_dev);
	return 0;
}

int ilitek_i2c_resume(struct i2c_client *client)
{
#ifdef GESTURE
	system_resume_ilitek = 1;
#endif

	if (update_wait_flag == 1) {
		tp_log_info("%s,ilitek_i2c_resume  ilitek waiting update so return\n", __func__);
		return 0;
	}

	if(i2c.firmware_updating) {
		tp_log_info("%s: tp fw is updating,return\n", __func__);
		return 0;
	}
	tp_log_info("ENTER %s i2c.reset_request_success = %x i2c.valid_irq_request=%d\n", __FUNCTION__,i2c.reset_request_success,i2c.valid_irq_request);
	if(i2c.reset_request_success)
	{
		ilitek_reset(i2c.reset_gpio);
	}
	i2c.function_ctrl = 0;
	if(i2c.valid_irq_request != 0){
		ilitek_i2c_irq_enable();
	}
	else{
		i2c.stop_polling = 0;
		tp_log_info("%s, start i2c thread polling\n", __func__);
	}
	return 0;
}

int read_product_id(void)
{
	int product_id_len = 7;
	unsigned char buf[64] = {0};
	int ret = 0, tmp = 0;
	int i = 0;

	for (ret = 0; ret < 30; ret++ ) {
		tmp = ilitek_poll_int();
		tp_log_info("ilitek int status = %d\n", tmp);
		if (tmp == 0) {
			break;
		}
		else {
			mdelay(5);
		}
	}
	if (ret == 30) {
		tp_log_err("ilitek reset but int not pull low force upgrade\n");
		i2c.force_upgrade = true;
	}
	buf[0] = ILITEK_TP_CMD_READ_DATA;
	ret = ilitek_i2c_write_and_read(i2c.client, buf, 1, 10, buf, 3);
	if (buf[1] < FW_OK) {
		tp_log_err("ilitek reset int pull low but 0x10 read < 0x80 buf[1] = 0x%x force upgrade\n", buf[1]);
		i2c.force_upgrade = true;
	}
	if (ret < 0) {
		tp_log_info("%s, 0x10 cmd read data :%X %X %X\n", __func__, buf[0], buf[1], buf[2]);
		return -TRANSMIT_ERROR;
	}
	i2c.product_id =  kzalloc(sizeof(char) *(product_id_len + 1), GFP_KERNEL);
	if (NULL == i2c.product_id) {
		tp_log_err("kzalloc i2c.product_id  error\n");
	}
	else {
		ret = outwrite(ENTER_ICE_MODE, 0x0, 0);
		ret = outwrite(REG_FLASH_CMD, REG_FLASH_CMD_RELEASE_FROM_POWER_DOWN, 1);
		ret = outwrite(REG_PGM_NUM, REG_PGM_NUM_TRIGGER_KEY, 4);
		ret = outwrite(REG_TIMING_SET, REG_TIMING_SET_10MS, 1);
		ret = outwrite(REG_CHK_EN, REG_CHK_EN_PARTIAL_READ, 1);
		ret = outwrite(REG_READ_NUM, REG_READ_NUM_1, 2);
		for (i = PRODUCT_ID_STARTADDR; i <= PRODUCT_ID_ENDADDR; i++) {
			i2c.product_id[i - PRODUCT_ID_STARTADDR] = read_flash_data(i);
			tp_log_info("ilitek flash_buf[0x%X] = 0x%X\n", i, i2c.product_id[i - PRODUCT_ID_STARTADDR]);
		}
		i2c.product_id[product_id_len + 1] = '\0';
		buf[0] = (unsigned char)(EXIT_ICE_MODE & DATA_SHIFT_0);
		buf[1] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_8) >> 8);
		buf[2] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_16) >> 16);
		buf[3] = (unsigned char)((EXIT_ICE_MODE & DATA_SHIFT_24) >> 24);
		ilitek_i2c_write(i2c.client, buf, 4);
	}
	return 0;
}

static bool is_ilitek_powered_on(void)
{
	int en_gpio_val;

	if (gpio_is_valid(i2c.enable_gpio)) {
		en_gpio_val = gpio_get_value(i2c.enable_gpio);

		return en_gpio_val == 1;
	}

	return true;
}

static int ilitek_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int retval = 0;
	struct device *dev = &(client->dev);
	struct device_node *np = dev->of_node;
#ifdef ILI_UPDATE_FW
	struct task_struct *thread_update = NULL;
#endif

#ifdef DEBUG_NETLINK
		struct netlink_kernel_cfg cfg = {
			.groups = 0,
			.input	= udp_receive,
		};

#endif
	tp_log_info("Enter ilitek_i2c_probe \n");

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		tp_log_err("%s, I2C_FUNC_I2C not support\n", __func__);
		return -EIO;
	}
#ifdef TOOL
	memset(&dev_ilitek, 0, sizeof(struct dev_data));
#endif
	memset(&i2c, 0, sizeof(struct i2c_data));

	mutex_init(&i2c.mutex);
	mutex_init(&(i2c.roi_mutex));

	i2c.report_status = 1;
	i2c.firmware_updating = false;
	i2c.valid_i2c_register = 1;
	i2c.function_ctrl = 0;
	i2c.glove_status = 0;
	i2c.hall_status = 0;
	i2c.hall_x0 = 0;
	i2c.hall_x1 = 0;
	i2c.hall_y0 = 0;
	i2c.hall_y1 = 0;
	i2c.force_upgrade = false;
	i2c.client = client;
	i2c.enable_gpio =  of_get_named_gpio(np, "ilitek,power-enable-gpio", 0);

	retval = ilitek_power_on();
	if (retval) {
		tp_log_err("%s: power on fail, retval=%d\n", __func__, retval);
		goto err_regulator;
	}

	retval = ilitek_request_io_port(client);
	if (retval){
		tp_log_err("io error");
		goto err_read_tp_info;
	}

	retval = ilitek_request_init_reset();
	if(retval < 0){
		tp_log_err("ilitek request reset err\n");
		i2c.reset_request_success = 0;
	}
	else{
		tp_log_info("ilitek request reset success\n");
		i2c.reset_request_success = 1;
	}

	retval = read_product_id();

	retval = ilitek_i2c_read_tp_info();
	if(retval < 0){
		tp_log_err("ilitek read tp info fail\n");
		goto err_read_tp_info;
	}
	i2c.input_dev = input_allocate_device();
	if(i2c.input_dev == NULL){
		tp_log_err("%s, allocate input device, error\n", __func__);
		goto err_read_tp_info;
	}

	tp_log_info("%s, register input device, success\n", __func__);
	i2c.valid_input_register = 1;

#if defined(CONFIG_FB)
		i2c.fb_notif.notifier_call = fb_notifier_callback;
		retval = fb_register_client(&i2c.fb_notif);
		if (retval) {
			dev_err(&i2c.client->dev, "Unable to register fb_notifier: %d\n",retval);
			goto err_register_device;
		}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
		i2c.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		i2c.early_suspend.suspend = ilitek_i2c_early_suspend;
		i2c.early_suspend.resume = ilitek_i2c_late_resume;
		register_early_suspend(&i2c.early_suspend);
#endif
	i2c.thread = kthread_run(ilitek_i2c_touchevent_thread, NULL, "ilitek_i2c_thread");
	if(i2c.thread == (struct task_struct*)ERR_PTR){
		i2c.thread = NULL;
		tp_log_err("%s, kthread create, error\n", __func__);
		goto err_run_thread;
	}

#ifdef GESTURE
	system_resume_ilitek = 1;
	input_set_capability(i2c.input_dev, EV_KEY, KEY_POWER);
	input_set_capability(i2c.input_dev, EV_KEY, KEY_W);
	input_set_capability(i2c.input_dev, EV_KEY, KEY_O);
	input_set_capability(i2c.input_dev, EV_KEY, KEY_C);
	input_set_capability(i2c.input_dev, EV_KEY, KEY_E);
	input_set_capability(i2c.input_dev, EV_KEY, KEY_M);
#endif

#ifdef TOOL
	create_tool_node();
#endif
	ilitek_touch_node_init();

	Report_Flag=0;
#ifdef ILI_UPDATE_FW
	thread_update= kthread_run(ilitek_i2c_update_thread, NULL, "ilitek_i2c_updatethread");
	if(thread_update == (struct task_struct*)ERR_PTR){
		thread_update = NULL;
		tp_log_err("%s,thread_update kthread create, error\n", __func__);
	}
#else
	tp_log_info("%s, IRQ: 0x%X\n", __func__, (i2c.client->irq));
	if(i2c.client->irq != 0 ){
		if(Request_IRQ()){
			tp_log_err("%s, request irq, error\n", __func__);
			goto err_register_device;
		}else{
			i2c.valid_irq_request = 1;
			i2c.irq_status = 1;
			tp_log_err("%s, request irq, success\n", __func__);
		}
	}
	ilitek_set_input_param(i2c.input_dev, i2c.max_tp, i2c.max_x, i2c.max_y);
	retval = input_register_device(i2c.input_dev);
	if(retval){
		tp_log_info("%s, register input device, error\n", __func__);
		return retval;
	}
#endif
#ifdef DEBUG_NETLINK
		//netlink_sock = netlink_kernel_create(&init_net, 21, 0,udp_receive, NULL, THIS_MODULE);
		netlink_sock = netlink_kernel_create(&init_net, 21, &cfg);
#endif
	return retval;
err_run_thread:
	free_irq(i2c.irq,&i2c);
err_register_device:
	if (NULL != i2c.input_dev){
		input_unregister_device(i2c.input_dev);
		i2c.input_dev = NULL;
	}
err_read_tp_info:
	if(i2c.reset_gpio){
		gpio_free(i2c.reset_gpio);
	}
	if(i2c.irq_gpio){
		gpio_free(i2c.irq_gpio);
	}
	if(i2c.vci_gpio){
		gpio_free(i2c.vci_gpio);
	}
	if (NULL != i2c.product_id)
	{
		kfree(i2c.product_id);
		i2c.product_id = NULL;
	}
err_regulator:
	/*
	if (i2c.ilitek_full_raw_max_cap) {
		kfree(i2c.ilitek_full_raw_max_cap);
		i2c.ilitek_full_raw_max_cap = NULL;
	}

	if (i2c.ilitek_full_raw_min_cap) {
		kfree(i2c.ilitek_full_raw_min_cap);
		i2c.ilitek_full_raw_min_cap = NULL;
	}
	*/
	tp_log_err("%s: ilitek probe fail\n", __func__);
	return retval;
}

static int ilitek_i2c_remove(struct i2c_client *client)
{
	tp_log_info( "%s\n", __func__);
	i2c.stop_polling = 1;
#if defined(CONFIG_FB)
	if (fb_unregister_client(&i2c.fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&i2c.early_suspend);
#endif
	if(i2c.client->irq != 0){
		if(i2c.valid_irq_request != 0){
			free_irq(i2c.client->irq, &i2c);
			tp_log_info("%s, free irq\n", __func__);
		}

		if(i2c.thread != NULL){
			kthread_stop(i2c.thread);
			tp_log_info("%s, stop i2c thread\n", __func__);
		}

	}
	else{
		if(i2c.thread != NULL){
			kthread_stop(i2c.thread);
			tp_log_info("%s, stop i2c thread\n", __func__);
		}
	}
	if(i2c.valid_input_register != 0){
		input_unregister_device(i2c.input_dev);
		tp_log_info("%s, unregister i2c input device\n", __func__);
	}

	#ifdef TOOL
		remove_tool_node();
	#endif

	ilitek_touch_node_deinit();
	tp_log_info("%s\n", __func__);
	return 0;
}

static int ilitek_init(void)
{
	int ret = 0;
	tp_log_info("%s\n", __func__);
	ret = i2c_add_driver(&ili2120_ts_driver);
	if(ret != 0)
	{
		tp_log_err("%s, add i2c device, error\n", __func__);
		return ret;
	}
	return 0;
}

static void ilitek_exit(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&i2c.early_suspend);
#endif
	if(i2c.client->irq != 0){
		if(i2c.valid_irq_request != 0){
			free_irq(i2c.client->irq, &i2c);
			tp_log_info("%s, free irq\n", __func__);
		}

		if(i2c.thread != NULL){
			kthread_stop(i2c.thread);
			tp_log_info("%s, stop i2c thread\n", __func__);
		}
	}
	else{
		if(i2c.thread != NULL){
			kthread_stop(i2c.thread);
			tp_log_info("%s, stop i2c thread\n", __func__);
		}
	}
	if(i2c.valid_i2c_register != 0){
		i2c_del_driver(&ili2120_ts_driver);
		tp_log_info("%s, delete i2c driver\n", __func__);
	}
	if(i2c.valid_input_register != 0){
		input_unregister_device(i2c.input_dev);
		tp_log_info("%s, unregister i2c input device\n", __func__);
	}

	#ifdef TOOL
	remove_tool_node();
	#endif
	tp_log_info("%s\n", __func__);
}

module_init(ilitek_init);
module_exit(ilitek_exit);
