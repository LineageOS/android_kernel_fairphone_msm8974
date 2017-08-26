#ifndef MSM_CAMERA_REGMAP_I2C_H
#define MSM_CAMERA_REGMAP_I2C_H 1

#include <mach/camera2.h>
#include "msm_camera_i2c.h"
#include "msm_cci.h"
#include <linux/regmap.h>
#include <linux/platform_device.h>


struct regmap *regmap_init_msm_camera_i2c(struct platform_device *pdev,
			       const struct regmap_config *config);

struct regmap *devm_regmap_init_msm_camera_i2c(struct platform_device *pdev,
				    const struct regmap_config *config);

#endif 
