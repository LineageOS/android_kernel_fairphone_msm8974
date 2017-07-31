/*
 * drivers/leds/leds-lm3644.c
 * General device driver for TI LM3644, FLASH LED Driver
 *
 * Copyright (C) 2014 Texas Instruments
 *
 * Contact: Daniel Jeong <gshark.jeong@gmail.com>
 *			Ldd-Mlp <ldd-mlp@list.ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/platform_data/leds-lm3644.h>
#include <linux/of_gpio.h>
#include "msm_cci.h"
#include "msm_camera_i2c.h"
#include "msm_camera_regmap_i2c.h"
#include "msm_camera_dt_util.h"
#include "detect/fp_cam_detect.h"
#include "lm3644_pm8941_power.h"


struct msm_camera_i2c_reg_conf;

/* registers definitions */
#define REG_ENABLE		0x01
#define REG_FLASH_LED0_BR	0x03
#define REG_FLASH_LED1_BR	0x04
#define REG_TORCH_LED0_BR	0x05
#define REG_TORCH_LED1_BR	0x06
#define REG_FLASH_TOUT		0x08
#define REG_FLAG0		0x0a
#define REG_FLAG1		0x0b

enum lm3644_devid {
	ID_FLASH0 = 0x0,
	ID_FLASH1,
	ID_TORCH0,
	ID_TORCH1,
	ID_MAX
};

enum lm3644_mode {
	MODE_STDBY = 0x0,
	MODE_IR,
	MODE_TORCH,
	MODE_FLASH,
	MODE_MAX
};

enum lm3644_devfile {
	DFILE_FLASH0_ENABLE = 0,
	DFILE_FLASH0_ONOFF,
	DFILE_FLASH0_SOURCE,
	DFILE_FLASH0_TIMEOUT,
	DFILE_FLASH1_ENABLE,
	DFILE_FLASH1_ONOFF,
	DFILE_TORCH0_ENABLE,
	DFILE_TORCH0_ONOFF,
	DFILE_TORCH0_SOURCE,
	DFILE_TORCH1_ENABLE,
	DFILE_TORCH1_ONOFF,
	DFILE_MAX
};

#undef CDBG
#if 1
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif


struct lm3644 {
	/* put i2c_client at beginning of struct so our
	   regmap implementation can find it */
	struct msm_camera_i2c_client i2c_client;
	uint32_t i2c_slaveaddr;
	uint32_t cci_master;

	unsigned is_powered;
	struct timer_list power_down_timer;

	struct device *dev;

	u8 brightness[ID_MAX];
	struct work_struct work[ID_MAX];
	struct led_classdev cdev[ID_MAX];

	struct lm3644_platform_data *pdata;
	struct regmap *regmap;
	struct mutex lock;
};

enum lm3644_cmd_id {
	CMD_ENABLE = 0,
	CMD_DISABLE,
	CMD_ON,
	CMD_OFF,
	CMD_IRMODE,
	CMD_OVERRIDE,
	CMD_MAX
};

/* device files to control registers */
struct lm3644_commands {
	char *str;
	int size;
};

struct lm3644_commands cmds[CMD_MAX] = {
	[CMD_ENABLE] = {"enable", 6},
	[CMD_DISABLE] = {"disable", 7},
	[CMD_ON] = {"on", 2},
	[CMD_OFF] = {"off", 3},
	[CMD_IRMODE] = {"irmode", 6},
	[CMD_OVERRIDE] = {"override", 8},
};

static size_t lm3644_ctrl(struct device *dev,
			  const char *buf, enum lm3644_devid id,
			  enum lm3644_devfile dfid, size_t size);


#define LM3644_POWER_TIMER_DELAY_MS 1000

static void lm3644_power_down_timer_func(unsigned long data);

/* to be called during driver initialization */
static void lm3644_power_init(struct lm3644 *pchip) {
	setup_timer(&pchip->power_down_timer, lm3644_power_down_timer_func,
			(unsigned long) pchip);
}

/* XXX: Currently we are looking for the brightness values
 * stored in the driver; maybe we want to check the enable
 * registers instead */
static int lm3644_should_power_down(struct lm3644 *pchip) {
	int i;
	for(i = 0; i < ID_MAX ; i++) {
		if (pchip->brightness[i] > 0)
			return 0;
	}
	return 1;
}

void lm3644_schedule_powerdown_timer_func(struct lm3644 *pchip) {
	if (pchip->is_powered) {
		mod_timer(&pchip->power_down_timer, jiffies +
			msecs_to_jiffies(LM3644_POWER_TIMER_DELAY_MS));
	}
}

static void lm3644_power_down_timer_func(unsigned long data) {
	struct lm3644 *pchip = (void*) data;
	if (lm3644_should_power_down(pchip)) {
		lm3644_pm8941_power_down();
		pchip->is_powered = 0;
	} else {
		lm3644_pm8941_power_pet_watchdog();
	}
	lm3644_schedule_powerdown_timer_func(pchip);
}

static int lm3644_power_up(struct lm3644 *pchip)
{
	int ret = 0;
	mutex_lock(&pchip->lock);
	del_timer_sync(&pchip->power_down_timer);
	if (!pchip->is_powered) {
		if( (ret = lm3644_pm8941_power_up()) )
			goto error;
		pchip->is_powered = 1;
	}
	/* we can extend the time now for LM3644_POWER_TIMER_DELAY_MS */
error:
	lm3644_schedule_powerdown_timer_func(pchip);
	mutex_unlock(&pchip->lock);
	return ret;
}

static void lm3644_read_flag(struct lm3644 *pchip)
{
	int rval;
	unsigned int flag0, flag1;

	rval = regmap_read(pchip->regmap, REG_FLAG0, &flag0);
	rval |= regmap_read(pchip->regmap, REG_FLAG1, &flag1);

	if (rval < 0)
		dev_err(pchip->dev, "i2c access fail (rval=%d).\n", rval) ;

	dev_info(pchip->dev, "[flag1] 0x%x, [flag0] 0x%x\n",
		 flag1 & 0x1f, flag0);
}

static void lm3644_torch_enable_disable(struct lm3644 *pchip, int flash_id)
{
	unsigned enabled = (pchip->brightness[flash_id] > 0);
	unsigned flash_enable_bit = flash_id - 1;

	/* enable torch mode */
	regmap_update_bits(pchip->regmap, REG_ENABLE, 0x0c, enabled?0x08:0);
	/* flash0 or flash1 enable */
	regmap_update_bits(pchip->regmap, REG_ENABLE, flash_enable_bit,
		enabled?flash_enable_bit:0);
}

/* torch0 brightness control */
static void lm3644_deferred_torch0_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work, struct lm3644, work[ID_TORCH0]);

	if (regmap_update_bits(pchip->regmap, REG_TORCH_LED0_BR, 0x7f,
			pchip->brightness[ID_TORCH0]))
		dev_err(pchip->dev, "i2c access fail.\n");

	lm3644_torch_enable_disable(pchip, ID_TORCH0);
	lm3644_read_flag(pchip);
}

static void lm3644_torch0_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH0]);

	pchip->brightness[ID_TORCH0] = brightness;
	schedule_work_on(0, &pchip->work[ID_TORCH0]);
}

/* torch1 brightness control */
static void lm3644_deferred_torch1_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work, struct lm3644, work[ID_TORCH1]);
	if ( (lm3644_power_up(pchip) < 0) )
		return;

	if (regmap_update_bits(pchip->regmap, REG_TORCH_LED1_BR, 0x7f,
			       pchip->brightness[ID_TORCH1]))
		dev_err(pchip->dev, "i2c access fail.\n");

	lm3644_torch_enable_disable(pchip, ID_TORCH1);
	lm3644_read_flag(pchip);
}

static void lm3644_torch1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_TORCH1]);

	pchip->brightness[ID_TORCH1] = brightness;
	schedule_work_on(0, &pchip->work[ID_TORCH1]);
}

static void lm3644_flash_enable_disable(struct lm3644 *pchip, int flash_id)
{
	unsigned enabled = (pchip->brightness[flash_id] > 0);
	unsigned flash_enable_bit = flash_id + 1;

	/* flash strobe (we can always set this one, could be also done in power up?)*/
	regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20, 0x20);
	/* enable flash mode */
	regmap_update_bits(pchip->regmap, REG_ENABLE, 0x0c, 0);
	/* flash0 or flash1 enable */
	regmap_update_bits(pchip->regmap, REG_ENABLE, flash_enable_bit,
		enabled?flash_enable_bit:0);
}

/* flash0 brightness control */
static void lm3644_deferred_flash0_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work, struct lm3644, work[ID_FLASH0]);

	if ( lm3644_power_up(pchip) < 0 )
		return;

	if (regmap_update_bits(pchip->regmap,
			       REG_FLASH_LED0_BR, 0xff,
			       pchip->brightness[ID_FLASH0]))
		dev_err(pchip->dev, "i2c access fail.\n");

	lm3644_flash_enable_disable(pchip, ID_FLASH0);
	lm3644_read_flag(pchip);
}

static void lm3644_flash0_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH0]);

	pchip->brightness[ID_FLASH0] = brightness;
	schedule_work_on(0, &pchip->work[ID_FLASH0]);
}

/* flash1 brightness control */
static void lm3644_deferred_flash1_brightness_set(struct work_struct *work)
{
	struct lm3644 *pchip = container_of(work, struct lm3644, work[ID_FLASH1]);

	if ( lm3644_power_up(pchip) < 0 )
		return;

	if (regmap_update_bits(pchip->regmap,
			       REG_FLASH_LED1_BR, 0x7f,
			       pchip->brightness[ID_FLASH1]))
		dev_err(pchip->dev, "i2c access fail.\n");

	lm3644_flash_enable_disable(pchip, ID_FLASH1);
	lm3644_read_flag(pchip);
}

static void lm3644_flash1_brightness_set(struct led_classdev *cdev,
					 enum led_brightness brightness)
{
	struct lm3644 *pchip =
	    container_of(cdev, struct lm3644, cdev[ID_FLASH1]);
	pchip->brightness[ID_FLASH1] = brightness;
	schedule_work_on(0, &pchip->work[ID_FLASH1]);
}

struct lm3644_devices {
	struct led_classdev cdev;
	work_func_t func;
};

static struct lm3644_devices lm3644_leds[ID_MAX] = {
	[ID_FLASH0] = {
		       .cdev.name = "flash0",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = 0x7f,
		       .cdev.brightness_set = lm3644_flash0_brightness_set,
		       .cdev.default_trigger = "flash0",
		       .func = lm3644_deferred_flash0_brightness_set},
	[ID_FLASH1] = {
		       .cdev.name = "flash1",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = 0x7f,
		       .cdev.brightness_set = lm3644_flash1_brightness_set,
		       .cdev.default_trigger = "flash1",
		       .func = lm3644_deferred_flash1_brightness_set},
	[ID_TORCH0] = {
		       .cdev.name = "torch0",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = 0x7f,
		       .cdev.brightness_set = lm3644_torch0_brightness_set,
		       .cdev.default_trigger = "torch0",
		       .func = lm3644_deferred_torch0_brightness_set},
	[ID_TORCH1] = {
		       .cdev.name = "torch1",
		       .cdev.brightness = 0,
		       .cdev.max_brightness = 0x7f,
		       .cdev.brightness_set = lm3644_torch1_brightness_set,
		       .cdev.default_trigger = "torch0",
		       .func = lm3644_deferred_torch1_brightness_set},
};

static void lm3644_led_unregister(struct lm3644 *pchip, enum lm3644_devid id)
{
	int icnt;

	for (icnt = id; icnt > 0; icnt--)
		led_classdev_unregister(&pchip->cdev[icnt - 1]);
}

static int lm3644_led_register(struct lm3644 *pchip)
{
	int icnt, rval;

	for (icnt = 0; icnt < ID_MAX; icnt++) {
		INIT_WORK(&pchip->work[icnt], lm3644_leds[icnt].func);
		pchip->cdev[icnt].name = lm3644_leds[icnt].cdev.name;
		pchip->cdev[icnt].max_brightness =
		    lm3644_leds[icnt].cdev.max_brightness;
		pchip->cdev[icnt].brightness =
		    lm3644_leds[icnt].cdev.brightness;
		pchip->cdev[icnt].brightness_set =
		    lm3644_leds[icnt].cdev.brightness_set;
		pchip->cdev[icnt].default_trigger =
		    lm3644_leds[icnt].cdev.default_trigger;
		rval = led_classdev_register((struct device *)
					     pchip->dev, &pchip->cdev[icnt]);
		if (rval < 0) {
			lm3644_led_unregister(pchip, icnt);
			return rval;
		}
	}
	return 0;
}


struct lm3644_files {
	enum lm3644_devid id;
	struct device_attribute attr;
};

static size_t lm3644_ctrl(struct device *dev,
			  const char *buf, enum lm3644_devid id,
			  enum lm3644_devfile dfid, size_t size)
{
	struct lm3644 *pchip;
	enum lm3644_cmd_id icnt;
	int tout, rval;
	int rc;

	if (!dev) {
		pr_err("%s: Illegal argument: dev must not be null.\n", __func__);
		return -EINVAL;
	}
	if (!dev->parent) {
		pr_err("%s: Missing parent device.\n", __func__);
		return -ENXIO;
	}
	pchip = dev_get_drvdata(dev->parent);

	if ( (rc = lm3644_power_up(pchip)) < 0) {
		return size;
	}

	mutex_lock(&pchip->lock);
	for (icnt = 0; icnt < CMD_MAX; icnt++) {
		if (strncmp(buf, cmds[icnt].str, cmds[icnt].size) == 0)
			break;
	}

	if (rval < 0)
		goto lm3644_ctrl_power_error;

	switch (dfid) {
		/* led 0 enable */
	case DFILE_FLASH0_ENABLE:
	case DFILE_TORCH0_ENABLE:
		if (icnt == CMD_ENABLE)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1,
					       0x1);
		else if (icnt == CMD_DISABLE)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x1,
					       0x0);
		break;
		/* led 1 enable, flash override */
	case DFILE_FLASH1_ENABLE:
		if (icnt == CMD_ENABLE) {
			rval = regmap_update_bits(pchip->regmap,
						  REG_FLASH_LED0_BR, 0x80, 0x0);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x2);
		} else if (icnt == CMD_DISABLE) {
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x0);
		} else if (icnt == CMD_OVERRIDE) {
			rval = regmap_update_bits(pchip->regmap,
						  REG_FLASH_LED0_BR, 0x80,
						  0x80);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x2);
		}
		break;
		/* led 1 enable, torch override */
	case DFILE_TORCH1_ENABLE:
		if (icnt == CMD_ENABLE) {
			rval = regmap_update_bits(pchip->regmap,
						  REG_TORCH_LED0_BR, 0x80, 0x0);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x2);
		} else if (icnt == CMD_DISABLE) {
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x0);
		} else if (icnt == CMD_OVERRIDE) {
			rval = regmap_update_bits(pchip->regmap,
						  REG_TORCH_LED0_BR, 0x80,
						  0x80);
			rval |=
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x2,
					       0x2);
		}
		break;
		/* mode control flash/ir */
	case DFILE_FLASH0_ONOFF:
	case DFILE_FLASH1_ONOFF:
		if (icnt == CMD_ON)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc,
					       0xc);
		else if (icnt == CMD_OFF)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc,
					       0x0);
		else if (icnt == CMD_IRMODE)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc,
					       0x4);
		break;
		/* mode control torch */
	case DFILE_TORCH0_ONOFF:
	case DFILE_TORCH1_ONOFF:
		if (icnt == CMD_ON)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc,
					       0x8);
		else if (icnt == CMD_OFF)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0xc,
					       0x0);
		break;
		/* strobe pin control */
	case DFILE_FLASH0_SOURCE:
		if (icnt == CMD_ON)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20,
					       0x20);
		else if (icnt == CMD_OFF)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x20,
					       0x0);
		break;
	case DFILE_TORCH0_SOURCE:
		if (icnt == CMD_ON)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10,
					       0x10);
		else if (icnt == CMD_OFF)
			rval =
			    regmap_update_bits(pchip->regmap, REG_ENABLE, 0x10,
					       0x0);
		break;
		/* flash time out */
	case DFILE_FLASH0_TIMEOUT:
		rval = kstrtouint((const char *)buf, 10, &tout);
		if (rval < 0)
			break;
		rval = regmap_update_bits(pchip->regmap,
					  REG_FLASH_TOUT, 0x0f, tout);
		break;
	default:
		dev_err(pchip->dev, "error : undefined dev file\n");
		break;
	}
	lm3644_read_flag(pchip);
lm3644_ctrl_power_error:
	mutex_unlock(&pchip->lock);
	return size;
}

/* flash enable control */
static ssize_t lm3644_flash0_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ENABLE, size);
}

static ssize_t lm3644_flash1_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ENABLE, size);
}

/* flash onoff control */
static ssize_t lm3644_flash0_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_ONOFF, size);
}

static ssize_t lm3644_flash1_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH1, DFILE_FLASH1_ONOFF, size);
}

/* flash timeout control */
static ssize_t lm3644_flash0_timeout_store(struct device *dev,
					   struct device_attribute *devAttr,
					   const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_TIMEOUT, size);
}

/* flash source control */
static ssize_t lm3644_flash0_source_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_FLASH0_SOURCE, size);
}

/* torch enable control */
static ssize_t lm3644_torch0_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_FLASH0, DFILE_TORCH0_ENABLE, size);
}

static ssize_t lm3644_torch1_enable_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ENABLE, size);
}

/* torch onoff control */
static ssize_t lm3644_torch0_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_ONOFF, size);
}

static ssize_t lm3644_torch1_onoff_store(struct device *dev,
					 struct device_attribute *devAttr,
					 const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH1, DFILE_TORCH1_ONOFF, size);
}

/* torch source control */
static ssize_t lm3644_torch0_source_store(struct device *dev,
					  struct device_attribute *devAttr,
					  const char *buf, size_t size)
{
	return lm3644_ctrl(dev, buf, ID_TORCH0, DFILE_TORCH0_SOURCE, size);
}

#define lm3644_attr(_name, _show, _store)\
{\
	.attr = {\
		.name = _name,\
		.mode = 0644,\
	},\
	.show = _show,\
	.store = _store,\
}

static struct lm3644_files lm3644_devfiles[DFILE_MAX] = {
	[DFILE_FLASH0_ENABLE] = {
				 .id = ID_FLASH0,
				 .attr =
				 lm3644_attr("enable", NULL,
					     lm3644_flash0_enable_store),
				 },
	[DFILE_FLASH0_ONOFF] = {
				.id = ID_FLASH0,
				.attr =
				lm3644_attr("onoff", NULL,
					    lm3644_flash0_onoff_store),
				},
	[DFILE_FLASH0_SOURCE] = {
				 .id = ID_FLASH0,
				 .attr =
				 lm3644_attr("source", NULL,
					     lm3644_flash0_source_store),
				 },
	[DFILE_FLASH0_TIMEOUT] = {
				  .id = ID_FLASH0,
				  .attr =
				  lm3644_attr("timeout", NULL,
					      lm3644_flash0_timeout_store),
				  },
	[DFILE_FLASH1_ENABLE] = {
				 .id = ID_FLASH1,
				 .attr =
				 lm3644_attr("enable", NULL,
					     lm3644_flash1_enable_store),
				 },
	[DFILE_FLASH1_ONOFF] = {
				.id = ID_FLASH1,
				.attr =
				lm3644_attr("onoff", NULL,
					    lm3644_flash1_onoff_store),
				},
	[DFILE_TORCH0_ENABLE] = {
				 .id = ID_TORCH0,
				 .attr =
				 lm3644_attr("enable", NULL,
					     lm3644_torch0_enable_store),
				 },
	[DFILE_TORCH0_ONOFF] = {
				.id = ID_TORCH0,
				.attr =
				lm3644_attr("onoff", NULL,
					    lm3644_torch0_onoff_store),
				},
	[DFILE_TORCH0_SOURCE] = {
				 .id = ID_TORCH0,
				 .attr =
				 lm3644_attr("source", NULL,
					     lm3644_torch0_source_store),
				 },
	[DFILE_TORCH1_ENABLE] = {
				 .id = ID_TORCH1,
				 .attr =
				 lm3644_attr("enable", NULL,
					     lm3644_torch1_enable_store),
				 },
	[DFILE_TORCH1_ONOFF] = {
				.id = ID_TORCH1,
				.attr =
				lm3644_attr("onoff", NULL,
					    lm3644_torch1_onoff_store),
				}
};

static void lm3644_df_remove(struct lm3644 *pchip, enum lm3644_devfile dfid)
{
	enum lm3644_devfile icnt;

	for (icnt = dfid; icnt > 0; icnt--)
		device_remove_file(pchip->cdev[lm3644_devfiles[icnt - 1].id].
				   dev, &lm3644_devfiles[icnt - 1].attr);
}

static int lm3644_df_create(struct lm3644 *pchip)
{
	enum lm3644_devfile icnt;
	int rval;

	for (icnt = 0; icnt < DFILE_MAX; icnt++) {
		rval =
		    device_create_file(pchip->cdev[lm3644_devfiles[icnt].id].
				       dev, &lm3644_devfiles[icnt].attr);
		if (rval < 0) {
			lm3644_df_remove(pchip, icnt);
			return rval;
		}
	}
	return 0;
}

static const struct regmap_config lm3644_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static int lm3644_fp_cam_module_detect(void)
{
	switch (fp_cam_module) {
	case FP_NO_CAM_MODULE:
		return -EPROBE_DEFER;
	case FP_CAM_MODULE_1:
		return -ENODEV;
	case FP_CAM_MODULE_2:
		return 0;
	default:
		pr_err("%s: Invalid value from EEPROM: %d\n", __func__, fp_cam_module);
		return -EINVAL;
	}
}

static int lm3644_probe(struct platform_device *pdev)
{
	struct msm_camera_cci_client *cci_client = NULL;
	struct lm3644 *pchip;
	int rval;
	struct device_node *of_node = pdev->dev.of_node;

	rval = lm3644_fp_cam_module_detect();
	if (rval < 0)
		return rval;

	pchip = devm_kzalloc(&pdev->dev, sizeof(struct lm3644), GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;

	pchip->dev = &pdev->dev;

	lm3644_power_init(pchip);

	pchip->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	pchip->i2c_client.cci_client = devm_kzalloc(&pdev->dev, sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!pchip->i2c_client.cci_client) {
		return -ENOMEM;
	}

	rval = of_property_read_u32(of_node, "qcom,slave-addr",
	        &pchip->i2c_slaveaddr);
	if (rval < 0) {
	        pr_err("%s failed rc %d\n", __func__, rval);
	        return rval;
	}

	rval = of_property_read_u32(of_node, "qcom,cci-master",
	        &pchip->cci_master);
	if (rval < 0) {
	        pr_err("%s failed rc %d\n", __func__, rval);
	        return rval;
	}


	cci_client = pchip->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->sid = pchip->i2c_slaveaddr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = pchip->cci_master;

	dev_info(pchip->dev, "lm3644 leds  i2c_slave addr: 0x%x, cci_i2c_master: %u\n",
		cci_client->sid, cci_client->cci_i2c_master);


	pchip->regmap = devm_regmap_init_msm_camera_i2c(pdev, &lm3644_regmap);
	if (IS_ERR(pchip->regmap)) {
		rval = PTR_ERR(pchip->regmap);
		dev_err(&pdev->dev, "Failed to allocate register map: %d\n",
			rval);
		return rval;
	}

	mutex_init(&pchip->lock);
	platform_set_drvdata(pdev, pchip);

	/* led class register */
	rval = lm3644_led_register(pchip);
	if (rval < 0)
		return rval;

	/* create dev files */
	rval = lm3644_df_create(pchip);
	if (rval < 0) {
		lm3644_led_unregister(pchip, ID_MAX);
		return rval;
	}

	dev_info(pchip->dev, "lm3644 leds initialized\n");
	return 0;
}


static const struct of_device_id msm_actuator_dt_match[] = {
	{.compatible = "ti,lm3644", .data = NULL},
	{}
};

static struct platform_driver lm3644_platform_data_driver = {
	.driver = {
		   .name = LM3644_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = msm_actuator_dt_match,
		   },
	.probe = lm3644_probe,
};

static int __init lm3644_init_module(void)
{
	CDBG("%s:\n",__func__);
	return platform_driver_register(&lm3644_platform_data_driver);
}


module_init(lm3644_init_module);
MODULE_LICENSE("GPL v2");
