/*
 * include/media/lm3644.h
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

#ifndef __LM3644_H__
#define __LM3644_H__

#include <media/v4l2-subdev.h>

#define LM3644_NAME	"lm3644"
#define LM3644_I2C_ADDR	(0x63)

/*  FLASH Brightness
 *	min 11310uA, step 11720uA, max 1500000uA
 */
#define LM3644_FLASH_BRT_MIN	11310
#define LM3644_FLASH_BRT_STEP	11720
#define LM3644_FLASH_BRT_MAX	1500000
#define LM3644_FLASH_BRT_uA_TO_REG(a)	\
	((a) < LM3644_FLASH_BRT_MIN ? 0 :	\
	 (((a) - LM3644_FLASH_BRT_MIN) / LM3644_FLASH_BRT_STEP))

/*  TORCH BRT
 *	min 1060uA, step 1460uA, max 187000uA
 */
#define LM3644_TORCH_BRT_MIN 1060
#define LM3644_TORCH_BRT_STEP 1460
#define LM3644_TORCH_BRT_MAX 187000
#define LM3644_TORCH_BRT_uA_TO_REG(a)	\
	((a) < LM3644_TORCH_BRT_MIN ? 0 :	\
	 (((a) - LM3644_TORCH_BRT_MIN) / LM3644_TORCH_BRT_STEP))

/*  FLASH TIMEOUT DURATION
 *	min 10ms, max 400ms
 *  step 10ms in range from  10ms to 100ms
 *  setp 50ms in range from 100ms to 400ms
 */
#define LM3644_FLASH_TOUT_MIN		10
#define LM3644_FLASH_TOUT_LOW_STEP	10
#define LM3644_FLASH_TOUT_LOW_MAX	100
#define LM3644_FLASH_TOUT_HIGH_STEP	50
#define LM3644_FLASH_TOUT_MAX		400
#define LM3644_FLASH_TOUT_ms_TO_REG(a)	\
	((a) < LM3644_FLASH_TOUT_MIN ? 0 :	\
	 ((a) <= LM3644_FLASH_TOUT_LOW_MAX ?	\
	  (((a) - LM3644_FLASH_TOUT_MIN) / LM3644_FLASH_TOUT_LOW_STEP) :	\
	   ((((a) - LM3644_FLASH_TOUT_LOW_MAX) / LM3644_FLASH_TOUT_HIGH_STEP)	\
		+((LM3644_FLASH_TOUT_LOW_MAX - LM3644_FLASH_TOUT_MIN)	\
		/ LM3644_FLASH_TOUT_LOW_STEP))))

enum lm3644_led_id {
	LM3644_LED0 = 0,
	LM3644_LED1,
	LM3644_LED_MAX
};

/*
 * struct lm3644_platform_data
 * @led0_enable : led0 enable
 * @led1_enable : led1 enable
 * @flash1_override : led1 flash current override
 * @torch1_override : led1 torch current override
 */
struct lm3644_platform_data {

	u8 led0_enable;
	u8 led1_enable;
	u8 flash1_override;
	u8 torch1_override;
};

#endif /* __LM3644_H__ */
