#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
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
#include <linux/regulator/consumer.h>
#include<../drivers/staging/android/timed_output.h>
#include <linux/slab.h>

#define DRV2603_VIBRATOR_EN		86
#define DRV2603_VIBRATOR_PWM		85
#define MAX_TIMEOUT			10000	/* 10s */
#define DRV2603_VTG_MIN_UV      2850000
#define DRV2603_VTG_MAX_UV      2850000
#define DRV2603_VTG_CURR_UA     150000

struct drv2603_data {
	struct platform_device *pdev;
	struct regulator *drv2603_vcc;
};
struct drv2603_data *drv2603;

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

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
        return (regulator_count_voltages(reg) > 0) ?
                regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int drv2603_pdev_power_on(struct drv2603_data *hbtp, bool on)
{
        int ret, error;

        if (!hbtp->drv2603_vcc) {
                pr_err("%s: regulator is not available\n", __func__);
                return -EINVAL;
        }

        if (!on)
                goto reg_off;

        ret = reg_set_optimum_mode_check(hbtp->drv2603_vcc, DRV2603_VTG_CURR_UA);
        if (ret < 0) {
                pr_err("%s: Regulator drv2603_vcc set_opt failed rc=%d\n",
                        __func__, ret);
                return -EINVAL;
        }

        ret = regulator_enable(hbtp->drv2603_vcc);
        if (ret) {
                pr_err("%s: Regulator drv2603_vcc enable failed rc=%d\n",
                        __func__, ret);
                error = -EINVAL;
                goto error_reg_en_drv2603_vcc;
        }

        return 0;
error_reg_en_drv2603_vcc:
        reg_set_optimum_mode_check(hbtp->drv2603_vcc, 0);
        return error;

reg_off:
        reg_set_optimum_mode_check(hbtp->drv2603_vcc, 0);
        regulator_disable(hbtp->drv2603_vcc);
        return 0;
}

#ifdef CONFIG_PM

static int ti_drv2603_suspend(struct platform_device *pdev, pm_message_t state)
{
        hrtimer_cancel(&vibdata.timer);
        cancel_work_sync(&vibdata.work);

	/* turn regulators off */	
	drv2603_pdev_power_on(drv2603,false);
	
	return 0;
}

static int ti_drv2603_resume(struct platform_device *pdev)
{
	int rc;

        /* turn regulators on */
        rc = drv2603_pdev_power_on(drv2603, true);
        if (rc < 0) {
		pr_err("%s: drv2603 unable to turn regulators on rc=%d\n",
                        __func__, rc);
                return rc;
        }
	
	return 0;
}

#endif /* CONFIG_PM */
	
static int  ti_drv2603_probe(struct platform_device *pdev)
{
	int rc=0;
	int ret =0;

	struct regulator *drv2603_vcc;
 
        printk("ti_drv2603 probe\n");
	drv2603_vcc = regulator_get(&pdev->dev, "vdd-drv2603");
	if (IS_ERR(drv2603_vcc)) {
		rc = PTR_ERR(drv2603_vcc);
		pr_err("%s: Regulator get failed vcc_drv2603 rc=%d\n",
			__func__, rc);
		return -EINVAL;
	}
	
	 if (regulator_count_voltages(drv2603_vcc) > 0) {
                ret = regulator_set_voltage(drv2603_vcc,
                                DRV2603_VTG_MIN_UV, DRV2603_VTG_MAX_UV);
                if (ret) {
                        pr_err("%s: regulator set_vtg failed rc=%d\n",
                                __func__, ret);
                        ret = -EINVAL;
                        goto error_set_vtg_drv2603_vcc;
                }
        }
	
	drv2603->drv2603_vcc = drv2603_vcc;
	drv2603->pdev= pdev;
	drv2603_pdev_power_on(drv2603,true);
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
	gpio_direction_input(DRV2603_VIBRATOR_PWM);
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

error_set_vtg_drv2603_vcc:
        regulator_put(drv2603_vcc);
        return ret;
}

static int ti_drv2603_remove(struct platform_device *pdev)
{
	if (drv2603->drv2603_vcc) {
		drv2603_pdev_power_on(drv2603, false);
                regulator_put(drv2603->drv2603_vcc);
        }
	return 0;
}

static const struct of_device_id drv2603_of_id_table[] = {
        {.compatible = "drv2603"},
	{ },
};

static struct platform_driver ti_drv2603_driver = {
	.probe		= ti_drv2603_probe,
	.remove		= ti_drv2603_remove,
	.driver		= {
		.name	= "drv2603",
		.owner	= THIS_MODULE,
		.of_match_table = drv2603_of_id_table,
	},
#ifdef CONFIG_PM
        .suspend = ti_drv2603_suspend,
        .resume =  ti_drv2603_resume,
#endif
};

static int  ti_drv2603_init(void)
{
	drv2603 = kzalloc(sizeof(struct drv2603_data), GFP_KERNEL);
        if (!drv2603)
		return -ENOMEM;
	return platform_driver_register(&ti_drv2603_driver);
}

static void  ti_drv2603_exit(void)
{
	mutex_destroy(&vibdata.lock);
	wake_lock_destroy(&vibdata.wklock);
	gpio_free(DRV2603_VIBRATOR_EN);
	gpio_free(DRV2603_VIBRATOR_PWM);
	timed_output_dev_register(&drv2603_motot_driver);
	platform_driver_unregister(&ti_drv2603_driver);
	kfree(drv2603);
}

module_init(ti_drv2603_init);
module_exit(ti_drv2603_exit);

MODULE_AUTHOR("Godfather.cxl");
MODULE_DESCRIPTION("Drv2603 Vibrator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ti_drv2603");
