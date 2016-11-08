/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#define OV2685_SENSOR_NAME "ov2685"

#define CONFIG_MSMB_CAMERA_DEBUG
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(ov2685_mut);
static struct msm_sensor_ctrl_t ov2685_s_ctrl;

static struct msm_sensor_power_setting ov2685_power_setting[] = {
  {
    .seq_type = SENSOR_VREG,
    .seq_val = CAM_VANA,
    .config_val = 0,
    .delay = 1,
  },
  {
    .seq_type = SENSOR_GPIO,
    .seq_val = SENSOR_GPIO_VDIG,
    .config_val = GPIO_OUT_HIGH,
    .delay = 5,
  },
  {
    .seq_type = SENSOR_CLK,
    .seq_val = SENSOR_CAM_MCLK,
    .config_val = 24000000,
    .delay = 10,
  },
  {
    .seq_type = SENSOR_I2C_MUX,
    .seq_val = 0,
    .config_val = 0,
    .delay = 0,
  },
};

static struct msm_camera_i2c_reg_conf ov2685_svga_settings[] = {
	{0x4202, 0x0f},
	{0x3620, 0x26},
	{0x3621, 0x37},
	{0x3622, 0x04},
	{0x370a, 0x23},
	{0x3718, 0x88},
	{0x3721, 0x00},
	{0x3722, 0x00},
	{0x3723, 0x00},
	{0x3738, 0x00},
	{0x4008, 0x00},
	{0x4009, 0x03},
	{0x3820, 0xc6},//0xc2
	{0x3821, 0x05},//0x01
	{0x3808, 0x03},
	{0x3809, 0x20},
	{0x380a, 0x02},
	{0x380b, 0x58},
	{0x3811, 0x04},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3082, 0x2c},
	{0x3083, 0x03},
	{0x3084, 0x0f},
	{0x3085, 0x03},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x380c, 0x06},
	{0x380d, 0xac},
	{0x380e, 0x02},
	{0x380f, 0x84},
	{0x3a06, 0x00},
	{0x3a07, 0xc1},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	{0x3a0e, 0x02},
	{0x3a0f, 0x43},
	{0x3a10, 0x02},
	{0x3a11, 0x84},
	{0x5281, 0x03},
	{0x5282, 0x06},
	{0x5283, 0x0a},
	{0x5302, 0x06},
	{0x5303, 0x0a},
	{0x5484, 0x04},
	{0x5485, 0x08},
	{0x5486, 0x10},
	{0x5488, 0x20},
	{0x5503, 0x06},
	{0x5504, 0x07},
	{0x3503, 0x00},
	{0x4202, 0x00},
};

static struct msm_camera_i2c_reg_conf ov2685_uxga_settings[] = {
	{0x3503, 0x03},
	{0x3620, 0x24},
	{0x3621, 0x34},
	{0x3622, 0x03},
	{0x370a, 0x21},
	{0x3718, 0x80},
	{0x3721, 0x09},
	{0x3722, 0x06},
	{0x3723, 0x59},
	{0x3738, 0x99},
	{0x4008, 0x02},
	{0x4009, 0x09},
	{0x3820, 0xc4},//0xc0
	{0x3821, 0x04},//0x00
	{0x3808, 0x06},
	{0x3809, 0x40},
	{0x380a, 0x04},
	{0x380b, 0xb0},
	{0x3811, 0x08},
	{0x3813, 0x08},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3082, 0x2c},
	{0x3083, 0x03},
	{0x3084, 0x0f},
	{0x3085, 0x03},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x380c, 0x06},
	{0x380d, 0xa4},
	{0x380e, 0x05},
	{0x380f, 0x0e},
	{0x3a06, 0x00},
	{0x3a07, 0xc2},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	{0x3a0e, 0x04},
	{0x3a0f, 0x8c},
	{0x3a10, 0x05},
	{0x3a11, 0x0e},
	{0x5281, 0x07},
	{0x5282, 0x07},
	{0x5283, 0x0c},
	{0x5302, 0x07},
	{0x5303, 0x0b},
	{0x5484, 0x06},
	{0x5485, 0x0b},
	{0x5486, 0x11},
	{0x5488, 0x18},
	{0x5503, 0x05},
	{0x5504, 0x06},
};

static struct msm_camera_i2c_reg_conf ov2685_start_settings[] = {
	{0x0100, 0x01}, /* wake up */
	{0x4202, 0x00}, /* stream on */
};

static struct msm_camera_i2c_reg_conf ov2685_stop_settings[] = {
	{0x0100, 0x00}, /* sleep */
	{0x4202, 0x0f}, /* stream off */
};

static struct msm_camera_i2c_reg_conf ov2685_recommend_settings[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x3002, 0x00},
	{0x3016, 0x1c},
	{0x3018, 0x44},//0x44
	{0x301d, 0xf0},
	{0x3020, 0x00},
	{0x3501, 0x54},
	{0x3502, 0x30},
	{0x3503, 0x00},
	{0x350b, 0x2b},
	{0x3600, 0xb4},
	{0x3603, 0x35},
	{0x3604, 0x24},
	{0x3605, 0x00},
	{0x3620, 0x26},
	{0x3621, 0x37},
	{0x3622, 0x04},
	{0x3628, 0x10},
	{0x3705, 0x3c},
	{0x370a, 0x23},
	{0x370c, 0x50},
	{0x370d, 0xc0},
	{0x3717, 0x58},
	{0x3718, 0x88},
	{0x3720, 0x00},
	{0x3721, 0x00},
	{0x3722, 0x00},
	{0x3723, 0x00},
	{0x3738, 0x00},
	{0x3781, 0x80},
	{0x3789, 0x60},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x06},
	{0x3805, 0x4f},
	{0x3806, 0x04},
	{0x3807, 0xbf},
	{0x3808, 0x03},
	{0x3809, 0x20},
	{0x380a, 0x02},
	{0x380b, 0x58},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3819, 0x04},
	{0x3820, 0xc6},//0xc2
	{0x3821, 0x05},//0x00
	{0x3082, 0x2c},
	{0x3083, 0x03},
	{0x3084, 0x0f},
	{0x3085, 0x03},
	{0x3086, 0x00},
	{0x3087, 0x00},
	{0x380c, 0x06},
	{0x380d, 0xac},
	{0x380e, 0x02},
	{0x380f, 0x84},
	{0x3a02, 0x81},
	{0x3a06, 0x00},
	{0x3a07, 0xc1},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	{0x3a0e, 0x02},
	{0x3a0f, 0x43},
	{0x3a10, 0x02},
	{0x3a11, 0x84},
	{0x3a00, 0x43},
	{0x382a, 0x09},
	{0x3a0a, 0x07},
	{0x3a0b, 0x8c},
	{0x3a0c, 0x07},
	{0x3a0d, 0x8c},
	{0x4000, 0x81},
	{0x4001, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x03},
	{0x4300, 0x30},
	{0x430e, 0x00},
	{0x4602, 0x02},
	{0x4837, 0x1e},
	{0x5000, 0xff},
	{0x5001, 0x05},
	{0x5002, 0x32},
	{0x5003, 0x04},
	{0x5004, 0xff},
	{0x5005, 0x12},
	{0x0100, 0x01},
	{0x5180, 0xf4},
	{0x5181, 0x11},
	{0x5182, 0x41},
	{0x5183, 0x42},
	{0x5184, 0x78},
	{0x5185, 0x52},
	{0x5186, 0x86},
	{0x5187, 0xa6},
	{0x5188, 0x0c},
	{0x5189, 0x0e},
	{0x518a, 0x0d},
	{0x518b, 0x4f},
	{0x518c, 0x3c},
	{0x518d, 0xf8},
	{0x518e, 0x04},
	{0x518f, 0x7f},
	{0x5190, 0x40},
	{0x5191, 0x3f},
	{0x5192, 0x40},
	{0x5193, 0xff},
	{0x5194, 0x40},
	{0x5195, 0x05},
	{0x5196, 0xa7},
	{0x5197, 0x04},
	{0x5198, 0x00},
	{0x5199, 0x06},
	{0x519a, 0xac},
	{0x519b, 0x04},
	{0x5200, 0x09},
	{0x5201, 0x00},
	{0x5202, 0x06},
	{0x5203, 0x20},
	{0x5204, 0x41},
	{0x5205, 0x16},
	{0x5206, 0x00},
	{0x5207, 0x05},
	{0x520b, 0x30},
	{0x520c, 0x75},
	{0x520d, 0x00},
	{0x520e, 0x30},
	{0x520f, 0x75},
	{0x5210, 0x00},
	{0x5280, 0x17},
	{0x5281, 0x03},
	{0x5282, 0x06},
	{0x5283, 0x0a},
	{0x5284, 0x10},
	{0x5285, 0x18},
	{0x5286, 0x20},
	{0x5287, 0x10},
	{0x5288, 0x02},
	{0x5289, 0x12},
	{0x5300, 0xc5},
	{0x5301, 0xa0},
	{0x5302, 0x02},
	{0x5303, 0x14},
	{0x5304, 0x18},
	{0x5305, 0x30},
	{0x5306, 0x60},
	{0x5307, 0xc0},
	{0x5308, 0x82},
	{0x5309, 0x00},
	{0x530a, 0x26},
	{0x530b, 0x02},
	{0x530c, 0x02},
	{0x530d, 0x00},
	{0x530e, 0x0c},
	{0x530f, 0x14},
	{0x5310, 0x3a},
	{0x5311, 0x20},
	{0x5312, 0x80},
	{0x5313, 0x50},
	{0x5380, 0x01},
	{0x5381, 0xb1},
	{0x5382, 0x00},
	{0x5383, 0x23},
	{0x5384, 0x00},
	{0x5385, 0x98},
	{0x5386, 0x00},
	{0x5387, 0x92},
	{0x5388, 0x00},
	{0x5389, 0x48},
	{0x538a, 0x01},
	{0x538b, 0xee},
	{0x538c, 0x10},
	{0x5400, 0x08},
	{0x5401, 0x16},
	{0x5402, 0x28},
	{0x5403, 0x4a},
	{0x5404, 0x58},
	{0x5405, 0x64},
	{0x5406, 0x6d},
	{0x5407, 0x78},
	{0x5408, 0x82},
	{0x5409, 0x8c},
	{0x540a, 0x9c},
	{0x540b, 0xa9},
	{0x540c, 0xc0},
	{0x540d, 0xd4},
	{0x540e, 0xe5},
	{0x540f, 0xa0},
	{0x5410, 0x5e},
	{0x5411, 0x06},
	{0x5480, 0x19},
	{0x5481, 0x0a},
	{0x5482, 0x0a},
	{0x5483, 0x12},
	{0x5484, 0x04},
	{0x5485, 0x08},
	{0x5486, 0x10},
	{0x5487, 0x18},
	{0x5488, 0x20},
	{0x5489, 0x1e},
	{0x5500, 0x02},
	{0x5501, 0x03},
	{0x5502, 0x05},
	{0x5503, 0x06},
	{0x5504, 0x07},
	{0x5505, 0x08},
	{0x5506, 0x00},
	{0x5600, 0x02},
	{0x5603, 0x32},
	{0x5604, 0x20},
	{0x5609, 0x20},
	{0x560a, 0x60},
	{0x5800, 0x02},
	{0x5801, 0xF5},
	{0x5802, 0x02},
	{0x5803, 0x4E},
	{0x5804, 0x3C},
	{0x5805, 0x05},
	{0x5806, 0x1A},
	{0x5807, 0x05},
	{0x5808, 0x03},
	{0x5809, 0x09},
	{0x580A, 0x02},
	{0x580B, 0x4A},
	{0x580C, 0x37},
	{0x580D, 0x05},
	{0x580E, 0x8C},
	{0x580F, 0x05},
	{0x5810, 0x02},
	{0x5811, 0xF1},
	{0x5812, 0x02},
	{0x5813, 0x4B},
	{0x5814, 0x2C},
	{0x5815, 0x05},
	{0x5816, 0x1E},
	{0x5817, 0x05},
	{0x5818, 0x0d},
	{0x5819, 0x40},
	{0x581a, 0x04},
	{0x581b, 0x0c},
	{0x3a03, 0x50},
	{0x3a04, 0x40},
	{0x3a12, 0x00},
	{0x3a13, 0x8e},
	{0x5180, 0xf4},
	{0x5181, 0x11},
	{0x5182, 0x41},
	{0x5183, 0x42},
	{0x5184, 0x78},
	{0x5185, 0x52},
	{0x5186, 0x86},
	{0x5187, 0xa6},
	{0x5188, 0x0c},
	{0x5189, 0x0e},
	{0x518a, 0x0d},
	{0x518b, 0x4f},
	{0x518c, 0x3c},
	{0x518d, 0xf8},
	{0x518e, 0x04},
	{0x518f, 0x7f},
	{0x5190, 0x40},
	{0x5191, 0x3f},
	{0x5192, 0x40},
	{0x5193, 0xff},
	{0x5194, 0x40},
	{0x5195, 0x05},
	{0x5196, 0xa7},
	{0x5197, 0x04},
	{0x5198, 0x00},
	{0x5199, 0x06},
	{0x519a, 0xac},
	{0x519b, 0x04},
	{0x5480, 0x19},
	{0x5481, 0x0a},
	{0x5482, 0x0a},
	{0x5483, 0x12},
	{0x5484, 0x04},
	{0x5485, 0x08},
	{0x5486, 0x10},
	{0x5487, 0x18},
	{0x5488, 0x20},
	{0x5489, 0x1e},
	{0x5000, 0xFF},
	{0x5800, 0x03},
	{0x5801, 0x10},
	{0x5802, 0x02},
	{0x5803, 0x3d},
	{0x5804, 0x30},
	{0x5805, 0x05},
	{0x5806, 0x19},
	{0x5807, 0x05},
	{0x5808, 0x03},
	{0x5809, 0x2a},
	{0x580A, 0x02},
	{0x580B, 0x37},
	{0x580C, 0x27},
	{0x580D, 0x05},
	{0x580E, 0x83},
	{0x580F, 0x05},
	{0x5810, 0x03},
	{0x5811, 0x10},
	{0x5812, 0x02},
	{0x5813, 0x3b},
	{0x5814, 0x29},
	{0x5815, 0x05},
	{0x5816, 0x91},
	{0x5817, 0x05},
	{0x3a03, 0x50},
	{0x3a04, 0x40},
	{0x5180, 0xf4},
	{0x5181, 0x11},
	{0x5182, 0x41},
	{0x5183, 0x42},
	{0x5184, 0x82},
	{0x5185, 0x52},
	{0x5186, 0x86},
	{0x5187, 0xa6},
	{0x5188, 0x0c},
	{0x5189, 0x0e},
	{0x518a, 0x0d},
	{0x518b, 0x4f},
	{0x518c, 0x3c},
	{0x518d, 0xf8},
	{0x518e, 0x04},
	{0x518f, 0x7f},
	{0x5190, 0x40},
	{0x5191, 0x3f},
	{0x5192, 0x40},
	{0x5193, 0xff},
	{0x5194, 0x40},
	{0x5195, 0x06},
	{0x5196, 0x8b},
	{0x5197, 0x04},
	{0x5198, 0x00},
	{0x5199, 0x05},
	{0x519a, 0x96},
	{0x519b, 0x04},
	{0x5200, 0x09},
	{0x5201, 0x00},
	{0x5202, 0x06},
	{0x5203, 0x20},
	{0x5204, 0x41},
	{0x5205, 0x16},
	{0x5206, 0x00},
	{0x5207, 0x05},
	{0x520b, 0x30},
	{0x520c, 0x75},
	{0x520d, 0x00},
	{0x520e, 0x30},
	{0x520f, 0x75},
	{0x5210, 0x00},
	{0x5280, 0x15},
	{0x5281, 0x06},
	{0x5282, 0x06},
	{0x5283, 0x08},
	{0x5284, 0x1c},
	{0x5285, 0x1c},
	{0x5286, 0x20},
	{0x5287, 0x10},
	{0x5380, 0x01},
	{0x5381, 0xDF},
	{0x5382, 0x00},
	{0x5383, 0xBB},
	{0x5384, 0x00},
	{0x5385, 0xA7},
	{0x5386, 0x00},
	{0x5387, 0xB0},
	{0x5388, 0x00},
	{0x5389, 0x07},
	{0x538a, 0x01},
	{0x538b, 0x88},
	{0x538c, 0x00},
	/* Gamma*/
	{0x5400, 0x0d},
	{0x5401, 0x1a},
	{0x5402, 0x32},
	{0x5403, 0x59},
	{0x5404, 0x68},
	{0x5405, 0x76},
	{0x5406, 0x82},
	{0x5407, 0x8c},
	{0x5408, 0x94},
	{0x5409, 0x9c},
	{0x540a, 0xa9},
	{0x540b, 0xb6},
	{0x540c, 0xcc},
	{0x540d, 0xdd},
	{0x540e, 0xeb},
	{0x540f, 0xa0},
	{0x5410, 0x6e},
	{0x5411, 0x06},
	/*RGB De-noise*/
	{0x5480, 0x19},
	{0x5481, 0x00},
	{0x5482, 0x09},
	{0x5483, 0x12},
	{0x5484, 0x04},
	{0x5485, 0x06},
	{0x5486, 0x08},
	{0x5487, 0x0c},
	{0x5488, 0x10},
	{0x5489, 0x18},
	/*UV de-noise auto -1*/
	{0x5500, 0x00},
	{0x5501, 0x01},
	{0x5502, 0x02},
	{0x5503, 0x03},
	{0x5504, 0x04},
	{0x5505, 0x05},
	{0x5506, 0x00},
	/*UV adjust*/
	{0x5600, 0x06},
	{0x5603, 0x32},
	{0x5604, 0x20},
	{0x5608, 0x00},
	{0x5609, 0x40},
	{0x560a, 0x80},
	/*lens correction*/
	{0x5800, 0x03},/*red x0 H*/
	{0x5801, 0x08},/* red x0 L*/
	{0x5802, 0x02},/* red y0 H*/
	{0x5803, 0x40},/* red y0 L*/
	{0x5804, 0x3a},/* red a1*/
	{0x5805, 0x05},/* red a2*/
	{0x5806, 0x12},/* red b1*/
	{0x5807, 0x05},/* red b2*/
	{0x5808, 0x03},/* green x0 H*/
	{0x5809, 0x1f},/* green x0 L*/
	{0x580a, 0x02},/*green y0 H*/
	{0x580b, 0x38},/* green y0 L*/
	{0x580c, 0x2f},/* green a1*/
	{0x580d, 0x05},/* green a2*/
	{0x580e, 0x52},/* green b1*/
	{0x580f, 0x06},/* green b2*/
	{0x5810, 0x02},/* blue x0 H*/
	{0x5811, 0xfc},
	{0x5812, 0x02},/* blue y0 H*/
	{0x5813, 0x30},/* blue y0 L*/
	{0x5814, 0x2c},/* bule a1*/
	{0x5815, 0x05},/* blue a2*/
	{0x5816, 0x42},/* blue b1*/
	{0x5817, 0x06},/* blue b2*/
	{0x5818, 0x0d},/* rst_seed on, md_en, coef_m off, gcoef_en*/
	{0x5819, 0x40},/* lenc_coef_th*/
	{0x581a, 0x04},/* lenc_gain_thre1*/
	{0x581b, 0x0c},/* lenc_gain_thre2*/
};

static struct v4l2_subdev_info ov2685_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_saturation[11][4] = {
	{/* Saturation +5 */
	{0x5603, 0x00},
	{0x5604, 0x00},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation +4 */
	{0x5603, 0x00},
	{0x5604, 0x00},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation +3 */
	{0x5603, 0x10},
	{0x5604, 0x10},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation +2 */
	{0x5603, 0x20},
	{0x5604, 0x20},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation +1 */
	{0x5603, 0x30},
	{0x5604, 0x30},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation Standard */
	{0x5603, 0x32},
	{0x5604, 0x20},
	{0x5600, 0x02},
	{0x560b, 0x00},
	},
	{/* Saturation -1 */
	{0x5603, 0x50},
	{0x5604, 0x50},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation -2 */
	{0x5603, 0x60},
	{0x5604, 0x60},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation -3 */
	{0x5603, 0x70},
	{0x5604, 0x70},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation -4 */
	{0x5603, 0x80},
	{0x5604, 0x80},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
	{/* Saturation -5 */
	{0x5603, 0x80},
	{0x5604, 0x80},
	{0x5600, 0x02},
	{0x560b, 0x01},
	},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_contrast[11][6] = {
	{/* Contrast +5 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x11},
	{0x5605, 0x10},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast +4 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x14},
	{0x5605, 0x14},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast +3 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x17},
	{0x5605, 0x14},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast +2 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x1a},
	{0x5605, 0x18},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast +1 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x1d},
	{0x5605, 0x1c},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Standard Contrast */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x20},
	{0x5605, 0x00},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast -1 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x23},
	{0x5605, 0x10},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast -2 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x26},
	{0x5605, 0x18},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast -3 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x29},
	{0x5605, 0x1c},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/*Contrast -4 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x2c},
	{0x5605, 0x1c},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/* Contrast -5 */
	{0x3208, 0x00},
	{0x5002, 0x33},
	{0x5606, 0x30},
	{0x5605, 0x20},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_exposure_compensation[5][6] = {
	{/* EV +2 */
	{0x3208, 0x00},
	{0x5600, 0x04},
	{0x5608, 0x08},
	{0x5607, 0x40},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/* EV +1 */
	{0x3208, 0x00},
	{0x5600, 0x04},
	{0x5608, 0x08},
	{0x5607, 0x20},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/* EV standard */
	{0x3208, 0x00},
	{0x5600, 0x04},
	{0x5608, 0x00},
	{0x5607, 0x00},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/* EV -1 */
	{0x3208, 0x00},
	{0x5600, 0x04},
	{0x5608, 0x00},
	{0x5607, 0x20},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
	{/* EV -2 */
	{0x3208, 0x00},
	{0x5600, 0x04},
	{0x5608, 0x00},
	{0x5607, 0x40},
	{0x3208, 0x10},
	{0x3208, 0xa0},
	},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_sharpness[7][2] = {
	{/* level 0 */
	{0x530a, 0x00},
	},
	{/* level 1 */
	{0x530a, 0x10},
	},
	{/* level 2 */
	 {0x530a, 0x18},
	},
	{/* level 3 */
	 {0x530a, 0x2a},
	},
	{/* level 4 */
	 {0x530a, 0x34},
	},
	{/* level 5 */
	{0x530a, 0x38},
	},
	{/* level 6 */
	{0x530a, 0x3f},
	},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_normal[] = {
	/* Normal (off) */
	{0x5600, 0x06},
	{0x5603, 0x32},
	{0x5604, 0x20},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_neon[] = {
	/* Redish */
	{0x3208, 0x00},
	{0x5600, 0x1c},
	{0x5603, 0x80},
	{0x5604, 0xc0},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_sketch[] = {
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_emboss[] = {
};

static struct msm_camera_i2c_reg_conf ov2685_reg_effect_black_white[] = {
	/* black and white */
	{0x5600, 0x1e},
	{0x5603, 0x80},
	{0x5604, 0x80},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_aqua[] = {
	/* Blueish (cool light) */
	{0x3208, 0x00},
	{0x5600, 0x1c},
	{0x5603, 0xa0},
	{0x5604, 0x40},
	{0x3208, 0x10},
	{0x3208, 0xa0},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_effect_sepiablue[] = {
	/* sepia */
	{0x5600, 0x1e},
	{0x5603, 0xa0},
	{0x5604, 0x40},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_effect_negative[] = {
	/* negative */
	{0x5600, 0x46},
	{0x5603, 0x40},
	{0x5604, 0x28},
};

/*
static struct msm_camera_i2c_reg_conf ov2685_reg_scene_auto[] = {
};

static struct msm_camera_i2c_reg_conf ov2685_reg_scene_portrait[] = {
};

static struct msm_camera_i2c_reg_conf ov2685_reg_scene_landscape[] = {
};

static struct msm_camera_i2c_reg_conf ov2685_reg_scene_night[] = {
};
*/

static struct msm_camera_i2c_reg_conf ov2685_reg_iso[7][2] = {
	{/* auto */
	{0x3a12, 0x00},
	{0x3a13, 0xf8},
	},
	{/* hjr */
	{0x3a12, 0x00},
	{0x3a13, 0xf8},
	},
	{/* 100 */
	{0x3a12, 0x00},
	{0x3a13, 0x1f},
	},
	{/* 200 */
	{0x3a12, 0x00},
	{0x3a13, 0x3f},
	},
	{/* 400 */
	{0x3a12, 0x00},
	{0x3a13, 0x7f},
	},
	{/* 800 */
	{0x3a12, 0x00},
	{0x3a13, 0xf8},
	},
	{/* 1600 */
	{0x3a12, 0x01},
	{0x3a13, 0xf8},
	},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_antibanding[4][6] = {
	{/* off */
	{0x3a02, 0x10},
	{0x3a00, 0x40},
	{0x3a06, 0x00},
	{0x3a07, 0xe4},
	{0x3a08, 0x00},
	{0x3a09, 0xbe},
	},
	{/* 60hz */
	{0x3a02, 0x10},
	{0x3a00, 0x41},
	{0x3a06, 0x00},
	{0x3a07, 0xc1},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	},
	{/* 50hz */
	{0x3a02, 0x90},
	{0x3a00, 0x41},
	{0x3a06, 0x00},
	{0x3a07, 0xc1},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	},
	{/* auto */
	{0x3a02, 0x90},
	{0x3a00, 0x41},
	{0x3a06, 0x00},
	{0x3a07, 0xc1},
	{0x3a08, 0x00},
	{0x3a09, 0xa1},
	},
};
static struct msm_camera_i2c_reg_conf ov2685_reg_wb_auto[] = {
	/* Auto */
	{0x5180, 0xfd},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_wb_sunny[] = {
	/*Sunny*/
	{0x5180, 0x02},
	{0x5195, 0x07},/*R gain*/
	{0x5196, 0x9c},
	{0x5197, 0x04},/*G gain*/
	{0x5198, 0x00},
	{0x5199, 0x05},/*B gain*/
	{0x519a, 0xf3},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_wb_cloudy[] = {
	/* Cloudy */
	{0x5180, 0x02},
	{0x5195, 0x07},/* R gain*/
	{0x5196, 0xdc},
	{0x5197, 0x04},/* G gain*/
	{0x5198, 0x00},
	{0x5199, 0x05},/* B gain*/
	{0x519a, 0xd3},
};

static struct msm_camera_i2c_reg_conf ov2685_reg_wb_office[] = {
	/* Office */
	{0x5180, 0x02},
	{0x5195, 0x04},/* R gain*/
	{0x5196, 0xdc},
	{0x5197, 0x04},/* G gain*/
	{0x5198, 0x00},
	{0x5199, 0x06},/* B gain*/
	{0x519a, 0xd3},
	};

static struct msm_camera_i2c_reg_conf ov2685_reg_wb_home[] = {
	/*Home*/
	{0x5180, 0x02},
	{0x5195, 0x04},/* R gain*/
	{0x5196, 0x90},
	{0x5197, 0x04},/* G gain*/
	{0x5198, 0x00},
	{0x5199, 0x09},/* B gain*/
	{0x519a, 0x20},
};

static const struct i2c_device_id ov2685_i2c_id[] = {
	{OV2685_SENSOR_NAME, (kernel_ulong_t)&ov2685_s_ctrl},
	{ }
};

static int32_t msm_ov2685_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov2685_s_ctrl);
}

static struct i2c_driver ov2685_i2c_driver = {
	.id_table = ov2685_i2c_id,
	.probe = msm_ov2685_i2c_probe,
	.driver = {
		.name = OV2685_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov2685_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov2685_dt_match[] = {
	{.compatible = "qcom,ov2685", .data = &ov2685_s_ctrl},
	{ }
};

MODULE_DEVICE_TABLE(of, ov2685_dt_match);

static int32_t ov2685_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	CDBG("%s:%d\n", __func__, __LINE__);
	match = of_match_device(ov2685_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static struct platform_driver ov2685_platform_driver = {
	.driver = {
		.name = "qcom,ov2685",
		.owner = THIS_MODULE,
		.of_match_table = ov2685_dt_match,
	},
	.probe = ov2685_platform_probe,
};

static void ov2685_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_camera_i2c_reg_conf *table,
	int num)
{
	int i = 0;
	int rc = 0;
	for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
		i2c_write(
		s_ctrl->sensor_i2c_client, table->reg_addr,
		table->reg_data,
		MSM_CAMERA_I2C_BYTE_ADDR);
		if (rc < 0) {
			msleep(100);
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
				s_ctrl->sensor_i2c_client, table->reg_addr,
				table->reg_data,
				MSM_CAMERA_I2C_BYTE_ADDR);
	}
	table++;
	}

}

static uint16_t ov2685_i2c_read_register(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t reg_addr)
{
	int32_t rc = 0;
	uint16_t reg_val = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		reg_addr,
		&reg_val, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: OV2685 read I2C failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return reg_val;
	}

	pr_info("%s: reg_val : %x\n", __func__, reg_val);
	return reg_val;
}


static int __init ov2685_init_module(void)
{
	int32_t rc;
	pr_err("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_register(&ov2685_platform_driver);
	if (!rc){
		pr_err("fail to %s:%d rc %d\n", __func__, __LINE__, rc);
		return rc;
	}
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov2685_i2c_driver);
}

static void __exit ov2685_exit_module(void)
{
	pr_err("%s:%d\n", __func__, __LINE__);
	if (ov2685_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov2685_s_ctrl);
		platform_driver_unregister(&ov2685_platform_driver);
	} else {
		i2c_del_driver(&ov2685_i2c_driver);
	}
	return;
}
static void ov2685_set_saturation(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_info("%s %d", __func__, value);

	ov2685_reg_saturation[value][2] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5600) | 0x2;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_saturation[value][0],

	ARRAY_SIZE(ov2685_reg_saturation[value]));
}

static void ov2685_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	int val = value/6;
	pr_info("%s %d", __func__, val);
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_sharpness[val][0],
	ARRAY_SIZE(ov2685_reg_sharpness[value]));
}
static void ov2685_set_contrast(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_info("%s %d", __func__, value);
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_contrast[value][0],
	ARRAY_SIZE(ov2685_reg_contrast[value]));
}

static void ov2685_set_iso(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_info("%s %d", __func__, value);
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_iso[value][0],
	ARRAY_SIZE(ov2685_reg_iso[value]));
}
static void ov2685_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_info("%s %d", __func__, value);
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_antibanding[value][0],
	ARRAY_SIZE(ov2685_reg_antibanding[value]));
}

static void ov2685_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	int val = (value + 12) / 6;

	pr_info("%s val:%d value:%d\n", __func__, val, value);

	ov2685_reg_exposure_compensation[val][1] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5600) | 0x4;

	ov2685_i2c_write_table(s_ctrl,
		&ov2685_reg_exposure_compensation[val][0],
	ARRAY_SIZE(ov2685_reg_exposure_compensation[val]));
}

static void ov2685_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_EFFECT_MODE_OFF: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_normal[0],
		ARRAY_SIZE(ov2685_reg_effect_normal));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEON: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_neon[0],
		ARRAY_SIZE(ov2685_reg_effect_neon));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_SKETCH: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_sketch[0],
		ARRAY_SIZE(ov2685_reg_effect_sketch));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_EMBOSS: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_emboss[0],
		ARRAY_SIZE(ov2685_reg_effect_emboss));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_MONO: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_black_white[0],
		ARRAY_SIZE(ov2685_reg_effect_black_white));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_NEGATIVE: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_negative[0],
		ARRAY_SIZE(ov2685_reg_effect_negative));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_SEPIA: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_sepiablue[0],
		ARRAY_SIZE(ov2685_reg_effect_sepiablue));
	break;
	}
	case MSM_CAMERA_EFFECT_MODE_AQUA: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_aqua[0],
		ARRAY_SIZE(ov2685_reg_effect_aqua));
	break;
	}
	default:
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_effect_normal[0],
		ARRAY_SIZE(ov2685_reg_effect_normal));
	}
}

/*
static void ov2685_set_scene_mode(struct msm_sensor_ctrl_t *s_ctrl, int value)
{
	pr_debug("%s %d", __func__, value);
	switch (value) {
	case MSM_CAMERA_SCENE_MODE_OFF: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_scene_auto[0],
		ARRAY_SIZE(ov2685_reg_scene_auto));
	break;
	}
	case MSM_CAMERA_SCENE_MODE_NIGHT: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_scene_night[0],
		ARRAY_SIZE(ov2685_reg_scene_night));
	break;
	}
	case MSM_CAMERA_SCENE_MODE_LANDSCAPE: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_scene_landscape[0],
		ARRAY_SIZE(ov2685_reg_scene_landscape));
	break;
	}
	case MSM_CAMERA_SCENE_MODE_PORTRAIT: {
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_scene_portrait[0],
		ARRAY_SIZE(ov2685_reg_scene_portrait));
	break;
	}
	default:
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_scene_auto[0],
		ARRAY_SIZE(ov2685_reg_scene_auto));
	}
}
*/

static void ov2685_set_white_balance_mode(struct msm_sensor_ctrl_t *s_ctrl,
	int value)
{
	pr_info("%s %d\n", __func__, value);
	switch (value) {
	case MSM_CAMERA_WB_MODE_AUTO: {
	ov2685_reg_wb_auto[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) & 0xfd;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_auto[0],
		ARRAY_SIZE(ov2685_reg_wb_auto));
	break;
	}
	case MSM_CAMERA_WB_MODE_INCANDESCENT: {
	ov2685_reg_wb_home[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) | 0x2;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_home[0],
		ARRAY_SIZE(ov2685_reg_wb_home));
	break;
	}
	case MSM_CAMERA_WB_MODE_DAYLIGHT: {
	ov2685_reg_wb_sunny[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) | 0x2;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_sunny[0],
		ARRAY_SIZE(ov2685_reg_wb_sunny));
	break;
	}
	case MSM_CAMERA_WB_MODE_FLUORESCENT: {
	ov2685_reg_wb_office[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) | 0x2;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_office[0],
		ARRAY_SIZE(ov2685_reg_wb_office));
	break;
	}
	case MSM_CAMERA_WB_MODE_CLOUDY_DAYLIGHT: {
	ov2685_reg_wb_cloudy[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) | 0x2;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_cloudy[0],
		ARRAY_SIZE(ov2685_reg_wb_cloudy));
	break;
	}
	default:
	{
	ov2685_reg_wb_auto[0] .reg_data =
		ov2685_i2c_read_register(s_ctrl, 0x5180) & 0xfd;
	ov2685_i2c_write_table(s_ctrl, &ov2685_reg_wb_auto[0],
		ARRAY_SIZE(ov2685_reg_wb_auto));
	}
	}
}

/*
static int setshutter = 0;
void ov2685_Set_Shutter(uint16_t iShutter)
{
	CDBG(" ov2685_Set_Shutter\r\n");
}

int ov2685_Read_Shutter(void)
{
	int shutter = 0;
	CDBG(" ov2685_Read_Shutter \r\n");
	return shutter;
}

void ov2685_AfterSnapshot(void)
{
}

void ov2685_BeforeSnapshot(void)
{
}
*/

int32_t ov2685_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov2685_recommend_settings,
			ARRAY_SIZE(ov2685_recommend_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_RESOLUTION: {
	/*copy from user the desired resoltuion*/
		enum msm_sensor_resolution_t res = MSM_SENSOR_INVALID_RES;
		if (copy_from_user(&res, (void *)cdata->cfg.setting,
			sizeof(enum msm_sensor_resolution_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		pr_err("%s:%d res =%d\n", __func__, __LINE__, res);

		if (res == MSM_SENSOR_RES_FULL) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov2685_uxga_settings,
				ARRAY_SIZE(ov2685_uxga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
				pr_err("%s:%d res =%d\n ov2685_uxga_settings ",
				__func__, __LINE__, res);
		} else if (res == MSM_SENSOR_RES_QTR) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov2685_svga_settings,
				ARRAY_SIZE(ov2685_svga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
			pr_err("%s:%d res =%d ov2685_svga_settings\n",
				 __func__, __LINE__, res);
		} else {
			pr_err("%s:%d failed resoultion set\n", __func__,
				__LINE__);
			rc = -EFAULT;
		}
	}
		break;
	case CFG_SET_STOP_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov2685_stop_settings,
			ARRAY_SIZE(ov2685_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov2685_start_settings,
			ARRAY_SIZE(ov2685_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr)
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info.power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(struct msm_sensor_power_setting)
				* size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
				rc = -ENOMEM;
				break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;

		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info.power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s sensor id %x sensor addr type %d sensor reg %x\n"
			"sensor id %x\n", __func__,
			sensor_slave_info.slave_addr,
			sensor_slave_info.addr_type,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting, stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
		}
	case CFG_SET_SATURATION: {
		int32_t sat_lev;
		if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s: Saturation Value is %d", __func__, sat_lev);
		ov2685_set_saturation(s_ctrl, sat_lev);
		break;
	}
	case CFG_SET_SHARPNESS: {
		int32_t shp_lev;
		if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		ov2685_set_sharpness(s_ctrl, shp_lev);
		break;
	}
	case CFG_SET_CONTRAST: {
		int32_t con_lev;
		if (copy_from_user(&con_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Contrast Value is %d", __func__, con_lev);
		ov2685_set_contrast(s_ctrl, con_lev);
		break;
	}
	case CFG_SET_ISO: {
		int32_t iso_lev;
		if (copy_from_user(&iso_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: ISO Value is %d", __func__, iso_lev);
		ov2685_set_iso(s_ctrl, iso_lev);
		break;
	}
	case CFG_SET_EXPOSURE_COMPENSATION: {
		int32_t ec_lev;
		if (copy_from_user(&ec_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Exposure compensation Value is %d",
			__func__, ec_lev);
		ov2685_set_exposure_compensation(s_ctrl, ec_lev);
		break;
	}
	case CFG_SET_ANTIBANDING: {
		int32_t antibanding_mode;
		if (copy_from_user(&antibanding_mode,
			(void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: anti-banding mode is %d", __func__,
			antibanding_mode);
		ov2685_set_antibanding(s_ctrl, antibanding_mode);
		break;
	}
	case CFG_SET_EFFECT: {
		int32_t effect_mode;
		if (copy_from_user(&effect_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Effect mode is %d", __func__, effect_mode);
		ov2685_set_effect(s_ctrl, effect_mode);
		break;
	}
/*
	case CFG_SET_BESTSHOT_MODE: {
		int32_t bs_mode;
		if (copy_from_user(&bs_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: best shot mode is %d", __func__, bs_mode);
		ov2685_set_scene_mode(s_ctrl, bs_mode);
		break;
	}
*/
	case CFG_SET_WHITE_BALANCE: {
		int32_t wb_mode;
		if (copy_from_user(&wb_mode, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: white balance is %d", __func__, wb_mode);
		ov2685_set_white_balance_mode(s_ctrl, wb_mode);
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static struct msm_sensor_fn_t ov2685_sensor_func_tbl = {
	.sensor_config = ov2685_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t ov2685_s_ctrl = {
	.sensor_i2c_client = &ov2685_sensor_i2c_client,
	.power_setting_array.power_setting = ov2685_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov2685_power_setting),
	.msm_sensor_mutex = &ov2685_mut,
	.sensor_v4l2_subdev_info = ov2685_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov2685_subdev_info),
	.func_tbl = &ov2685_sensor_func_tbl,
};

module_init(ov2685_init_module);
module_exit(ov2685_exit_module);
MODULE_DESCRIPTION("ov2685 2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
