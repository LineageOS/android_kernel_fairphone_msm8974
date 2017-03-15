/*
 * Register map access API - MSM CCI I2C support
 *
 * Copyright 2011 Wolfson Microelectronics plc
 * Copyright 2017 Fairphone BV
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 * Author: Dirk Vogt <dirk@fairphone.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include "msm_camera_regmap_i2c.h"

static struct msm_camera_i2c_client *to_msm_camera_i2c_client(const struct device *dev) {
	struct platform_device *pdev = to_platform_device(dev);
	/* i2c_client is at begin of drv_data */
	struct msm_camera_i2c_client *i2c_client = platform_get_drvdata(pdev);
	return i2c_client;
}

static int regmap_msm_camera_i2c_write(struct device *dev, const void *data, size_t count)
{
	struct msm_camera_i2c_client *i2c_client =  to_msm_camera_i2c_client(dev);
	int ret;
	int i;

	if (i2c_client == NULL) {
		return -EIO;
	}
	
	if ((count % 2)) {
		printk("Count should always be divisble by two;");
		return -EIO;
	}
	msm_sensor_cci_i2c_util(i2c_client, MSM_CCI_INIT);

	for (i = 0 ; i < count/2 ; i+=2) {
		uint8_t addr = ((uint8_t *)data)[i];
		uint8_t val  = ((uint8_t *)data)[i+1];
		printk("writing  0x%x -> (0x%x)", val, addr);
		ret = msm_camera_cci_i2c_write_seq(i2c_client, addr, &val, count);
		if(ret != 0)
			break;
	}
	msm_sensor_cci_i2c_util(i2c_client, MSM_CCI_RELEASE);

	return ret;
}

/* XXX we don't support this */
static int regmap_msm_camera_i2c_gather_write(struct device *dev,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	return -ENOTSUPP;
}

static int regmap_msm_camera_i2c_read(struct device *dev,
			   const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	int ret;

	struct msm_camera_i2c_client *i2c_client =  to_msm_camera_i2c_client(dev);
	if (reg_size != 1 || val_size != 1) {
		printk("We only support byte wide reads\n");
		return -ENOTSUPP;
	}
	msm_sensor_cci_i2c_util(i2c_client, MSM_CCI_INIT);
	ret = msm_camera_cci_i2c_read_seq(i2c_client, *((uint8_t*)reg), (uint8_t*) val, val_size);
	msm_sensor_cci_i2c_util(i2c_client, MSM_CCI_RELEASE);

	return ret;
}

static struct regmap_bus regmap_msm_camera_i2c = {
	.write = regmap_msm_camera_i2c_write,
	.gather_write = regmap_msm_camera_i2c_gather_write,
	.read = regmap_msm_camera_i2c_read,
};

struct regmap *regmap_init_msm_camera_i2c(struct platform_device *pdev,
			       const struct regmap_config *config)
{
	return regmap_init(&pdev->dev, &regmap_msm_camera_i2c, config);
}
EXPORT_SYMBOL_GPL(regmap_init_msm_camera_i2c);


struct regmap *devm_regmap_init_msm_camera_i2c(struct platform_device *pdev,
				    const struct regmap_config *config)
{
	return devm_regmap_init(&pdev->dev, &regmap_msm_camera_i2c, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_msm_camera_i2c);

MODULE_LICENSE("GPL");
