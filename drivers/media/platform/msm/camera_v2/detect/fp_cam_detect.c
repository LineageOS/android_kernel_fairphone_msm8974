/* Copyright (c) 2017, Fairphone. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/bug.h>

#include "fp_cam_detect.h"

enum fp_cam_module_id fp_cam_module = FP_NO_CAM_MODULE;

struct fp_cam_module_desc {
	enum fp_cam_module_id module_id;
	const char *eeprom_name;
};

/* EEPROM names as defined in device tree. */
struct fp_cam_module_desc fp_cam_module_descs[] = {
	{FP_CAM_MODULE_1, "sunny_q8v18a"},
	{FP_CAM_MODULE_2, "ov12870_gt24p64a"},
};

/**
 * Set name of EEPROM for camera module detection of Fairphone 2.
 * @eeprom_name: Name of the loaded EEPROM device as defined in device tree.
 */
void fp_cam_module_set(const char *eeprom_name)
{
	int i;

	if (fp_cam_module != FP_NO_CAM_MODULE)
		pr_warn("%s: Camera module is set already: %d\n", __func__,
				fp_cam_module);

	for (i = 0; i < ARRAY_SIZE(fp_cam_module_descs); i++) {
		if (!strcmp(fp_cam_module_descs[i].eeprom_name, eeprom_name)) {
			fp_cam_module = fp_cam_module_descs[i].module_id;
			pr_info("%s: Camera module detected: %d (%s)\n", __func__,
					fp_cam_module, eeprom_name);
			return;
		}
	}
}
