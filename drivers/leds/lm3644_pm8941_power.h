#ifndef DRIVERS_LEDS_LM3644_PM8941_POWER_H
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/spmi.h>

int  lm3644_pm8941_power_init(struct spmi_device* spmi_dev);
void lm3644_pm8941_power_pet_watchdog(void);
int  lm3644_pm8941_power_down(void);
int  lm3644_pm8941_power_up(void);

#endif
