/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H
#define __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H

#include <linux/types.h>
//#include <dt-bindings/input/atmel_mxt_ts.h>
//#include <linux/i2c/atmel_mxt_ts.h>
enum mxt_suspend_mode
{
    MXT_SUSPEND_DEEP_SLEEP = 0,
    MXT_SUSPEND_T9_CTRL = 1,
    MXT_SUSPEND_REGULATOR = 2,
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	unsigned long irqflags;
	u8 t19_num_keys;
	const unsigned int *t19_keymap;
	enum mxt_suspend_mode suspend_mode;
	int t15_num_keys;
	const unsigned int *t15_keymap;
	unsigned int gpio_reset;
	u32 ldo_gpio;
	const char *cfg_name;
	const char *input_name;
};

#endif /* __LINUX_PLATFORM_DATA_ATMEL_MXT_TS_H */
