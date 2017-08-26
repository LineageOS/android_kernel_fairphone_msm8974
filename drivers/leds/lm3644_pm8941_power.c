#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/regulator/consumer.h>
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

#define FLASH_RAMP_UP_DELAY_US 1000
#define FLASH_RAMP_DN_DELAY_US 2160

static struct spmi_device *_flash_controller_spmi_dev;
static struct regulator *flash_boost_reg;
static struct regulator *flash_wa_reg;

#define MSM_LED_POWER_TIMER_DELAY_MS 1000

static struct timer_list lm3644_pm8941_power_timer;
static void lm3644_pm8941_power_pet_watchdog(void);
static DEFINE_MUTEX(power_lock);
static int ref_count=0;

void lm3644_init(void);
void lm3644_deinit(void);

static void lm3644_pm8941_power_schedule_timer_func(void)
{
	if (ref_count > 0) {
		mod_timer(&lm3644_pm8941_power_timer, jiffies +
				msecs_to_jiffies(MSM_LED_POWER_TIMER_DELAY_MS));
	}
}


static void lm3644_pm8941_power_timer_func(unsigned long data)
{
	if (ref_count > 0) {
		lm3644_pm8941_power_pet_watchdog();
		lm3644_pm8941_power_schedule_timer_func();
	}
}


static void lm3644_pm8941_power_timer_init(void)
{
	setup_timer(&lm3644_pm8941_power_timer, lm3644_pm8941_power_timer_func,
			(unsigned long) &lm3644_pm8941_power_timer);
}


/*
 * We cannot get hold of the voltage regulators at the init stage
 * So we call this function when ever we need thema and return a error if we are
 * unable to get them.
 */
int lm3644_pm8941_get_regulators(void) {
	int rc;

	struct spmi_device *spmi_dev = _flash_controller_spmi_dev;
	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
	}
	if ( !flash_wa_reg ) {
		flash_wa_reg = devm_regulator_get(&spmi_dev->dev, "flash-wa");
		if (IS_ERR_OR_NULL (flash_wa_reg)) {
			rc = PTR_ERR(flash_wa_reg);
			dev_warn(&spmi_dev->dev,
					"cannot get voltage regulator flash_wa_reg (%d)\n", rc);
			flash_wa_reg = NULL;
			return rc;
		}
	}

	if (!flash_boost_reg) {
		flash_boost_reg = regulator_get(&spmi_dev->dev, "flash-boost");
		if (IS_ERR_OR_NULL (flash_boost_reg)) {
			rc = PTR_ERR(flash_boost_reg);
			dev_warn(&spmi_dev->dev,
					"cannot get voltage regulator flash_boost_reg (%d)\n", rc);
			flash_boost_reg = NULL;
			return rc;
		}
	}

	return 0;
}


int lm3644_pm8941_power_init(struct spmi_device* spmi_dev)
{
	struct resource *led_resource =
		spmi_get_resource(spmi_dev, NULL, IORESOURCE_MEM, 0);
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

	lm3644_pm8941_power_timer_init();
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


static void lm3644_pm8941_power_pet_watchdog(void) {
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;
	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
	}
	pr_debug("%s: Petting the watchdog for LM3644\n", __func__);
	qpnp_reg_write(spmi_dev, REG_WATCHDOG_PET, 0x80);
}


int lm3644_pm8941_power_down(void)
{
	int rc = 0;
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;

	mutex_lock(&power_lock);

	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
		rc = -EINVAL;
		goto out;
	}

	pr_err("%s: ref count: %d ", __func__, ref_count);

	if(ref_count > 1) {
		pr_debug("%s: power is still needed", __func__);
		goto out;
	}

	if(ref_count == 0) {
		pr_debug("%s: Ref count is 0? Power-up failed earlier?", __func__);
		rc = -EINVAL;
		goto out;
	}

	if ((rc = lm3644_pm8941_get_regulators())) {
		goto out;
	}

	lm3644_deinit();
	pr_debug("%s: Disabling Power for LM3644\n", __func__);
	qpnp_reg_write(spmi_dev, REG_MODULE_ENABLE, 0x00);

	usleep(FLASH_RAMP_UP_DELAY_US);

	if ( (rc = regulator_disable(flash_boost_reg))) {
		dev_err(&spmi_dev->dev, "Could not disable flash_boost_reg (%d)", rc);
	}
	if ( (rc = regulator_disable(flash_wa_reg))) {
		dev_err(&spmi_dev->dev, "Could not disable flash_wa_reg (%d)", rc);
	}

	if(rc== 0) {
		pr_debug("%s: Power disabled",__func__);
	}
out:
	if (rc == 0) {
		pr_debug("%s: decreasing refcount", __func__);
		ref_count--;
	} else {
		pr_err("%s something went wrong..., not decreasing refcount", __func__);
	}
	mutex_unlock(&power_lock);
	return rc;
}


int lm3644_pm8941_power_up(void)
{
	int rc=0, rc1;
	struct spmi_device * spmi_dev = _flash_controller_spmi_dev;
	mutex_lock(&power_lock);
	pr_debug("%s: ref_count: %d\n", __func__, ref_count);
	if(ref_count > 0) {
		goto out;
	}

	if (_flash_controller_spmi_dev == NULL) {
		pr_err("Error in %s : _flash_controller_spmi_dev is NULL?", __func__);
		rc = -EINVAL;
		goto error;
	}

	if ((rc = lm3644_pm8941_get_regulators()))
		goto error;

	if ((rc = regulator_enable(flash_wa_reg))) {
		dev_err(&spmi_dev->dev, "Could not enable flash_we_reg (%d)", rc);
		goto error;
	}
	if ((rc = regulator_enable(flash_boost_reg))) {
		dev_err(&spmi_dev->dev, "Could not enable flash_boost_reg (%d)", rc);
		if ( (rc1 = regulator_disable(flash_wa_reg)) ) {
			dev_err(&spmi_dev->dev, "Could not disable flash_wa_reg (%d)", rc1);
		}
		goto error;
	}

	if( (rc = qpnp_reg_write(spmi_dev, REG_MODULE_ENABLE, 0x00)) )
		goto error;
	/* enable watchdog timer */
	if( (rc = qpnp_reg_write(spmi_dev, REG_TMR_CONTROL, 0x03)) )
		goto error;
	if( (rc = qpnp_reg_write(spmi_dev, REG_WATCHDOG_TIMEOUT, 0x1f)) )
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
	/* SEC_ACCESS, needed to disable timers. */
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
	usleep(FLASH_RAMP_UP_DELAY_US);
	/* Enable SW stropbe */
	if( (rc = qpnp_reg_write(spmi_dev, REG_STROBE_CONTROL, 0xC0)) )
		goto error;

	if(rc==0) {
		pr_debug("%s: power enabled \n", __func__);
	}
	lm3644_init();
out:
error:
	if (rc==0) {
		ref_count++;
		lm3644_pm8941_power_schedule_timer_func();
	} else {
		pr_err("%s, Power up lm3644 failed\n", __func__);
	}
	mutex_unlock(&power_lock);
	return rc;
}
