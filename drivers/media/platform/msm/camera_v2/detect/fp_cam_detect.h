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

#ifndef _FP_CAM_DETECT_H
#define _FP_CAM_DETECT_H

enum fp_cam_module_id {
	FP_NO_CAM_MODULE,
	FP_CAM_MODULE_1,
	FP_CAM_MODULE_2,
};

/**
 * Indicates which camera module has been detected.
 * FP_NO_CAM_MODULE in case module has not been set yet.
 */
extern enum fp_cam_module_id fp_cam_module;

/**
 * Set name of EEPROM for camera module detection of Fairphone 2.
 * @eeprom_name: Name of the loaded EEPROM device as defined in device tree.
 */
void fp_cam_module_set(const char *eeprom_name);

#endif
