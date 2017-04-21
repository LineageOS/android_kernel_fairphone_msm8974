#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include "detect/fp_cam_detect.h"

#define REG_FLASH_MAX_CURRENT 0xD341
#define REG_MODULE_ENABLE     0xD346
#define REG_LED1_CURRENT      0xD342
#define REG_LED2_CURRENT      0xD343
#define REG_SEC_ACCESS        0xD3D0
#define REG_TEST3             0xD3E4
#define REG_FAULT_DETECT      0xD351
#define REG_STROBE_CONTROL    0xD347
#define REG_VPH_PWR_DROOP     0xD35A
#define REG_WATCHDOG_PET      0xD356
#define REG_TMR_CONTROL       0xD348
#define REG_WATCHDOG_TIMEOUT  0xD349

static struct spmi_device *_flash_controller_spmi_dev;

int lm3644_pm8941_power_init(struct spmi_device* spmi_dev) {
	struct resource *led_resource = spmi_get_resource(spmi_dev, NULL, IORESOURCE_MEM, 0);
	u16 base = led_resource->start;

	if (base != 0xD300)
		return 0;

	switch (fp_cam_module) {
		case FP_NO_CAM_MODULE:
			return -EPROBE_DEFER;
		case FP_CAM_MODULE_2:
			break;
		default:
			return 0;
	}

	_flash_controller_spmi_dev = spmi_dev;
	return 1;
}

static int qpnp_reg_write(struct spmi_device* spmi_dev, u16 addr, u8 val)
{
	int rc = spmi_ext_register_writel(spmi_dev->ctrl, spmi_dev->sid, addr, &val, 1);
	if (rc)
		dev_err(&spmi_dev->dev,
			"Unable to write to addr=%x, rc(%d)\n", addr, rc);
	return rc;
}

void lm3644_pm8941_power_pet_watchdog(void) {
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;
	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
	}
	pr_debug("%s: Petting the watchdog for LM3644\n", __func__);
	qpnp_reg_write(spmi_dev, REG_WATCHDOG_PET, 0x80);
}

int lm3644_pm8941_power_down(void)
{
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;
	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
		return -EINVAL;
	}
	pr_debug("%s: Disabling Power for LM3644\n", __func__);
	return qpnp_reg_write(spmi_dev,REG_MODULE_ENABLE, 0x00);
}

int lm3644_pm8941_power_up(void)
{
	int rc;
	struct resource *led_resource;
	u16 base;
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;
	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
		return -EINVAL;
	}
	led_resource = spmi_get_resource(spmi_dev, NULL, IORESOURCE_MEM, 0);
	base = led_resource->start;

	/* enable watchdog timer */
	if( (rc = qpnp_reg_write(spmi_dev, REG_TMR_CONTROL, 0x03)) )
		goto error;
	/* set watchdog timer timout to 2 Seconds */
	if( (rc = qpnp_reg_write(spmi_dev, base + REG_WATCHDOG_TIMEOUT, 0x00)) )
		goto error;
	/* Enable module */
	if( (rc = qpnp_reg_write(spmi_dev, REG_MODULE_ENABLE, 0xE0)) )
		goto error;
	/* FLASH_MAX_CURRENT */
	if( (rc = qpnp_reg_write(spmi_dev, REG_FLASH_MAX_CURRENT, 0x4F)) )
		goto error;
	if( (rc = qpnp_reg_write(spmi_dev, REG_LED1_CURRENT, 0x4F)) )
		goto error;
	if( (rc = qpnp_reg_write(spmi_dev, REG_LED2_CURRENT, 0x4F)) )
		goto error;
	/* 4 SEC_ACCESS, needed to disable timers. */
	if( (rc = qpnp_reg_write(spmi_dev, REG_SEC_ACCESS, 0xC0)) )
		goto error;
	/* disable safety timers. */
	if( (rc = qpnp_reg_write(spmi_dev, REG_TEST3, 0x00)) )
		goto error;
	/* Disable Fault detect */
	if( (rc = qpnp_reg_write(spmi_dev, REG_FAULT_DETECT, 0x00)) )
		goto error;
		/* Disable Power droop */
	if( (rc = qpnp_reg_write(spmi_dev, REG_VPH_PWR_DROOP, 0x00)) )
		goto error;
	/* Enable SW stropbe */
	if( (rc = qpnp_reg_write(spmi_dev, REG_STROBE_CONTROL, 0xC0)) )
		goto error;
	pr_debug("%s: Enabled Power for LM3644\n", __func__);
	return rc;
error:
	dev_err(&spmi_dev->dev,
		"Power up lm3644 failed\n");
	return rc;
}
