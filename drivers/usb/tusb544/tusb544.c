/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include "tusb544.h"
#include <linux/clk.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/extcon.h>

#include <linux/usb/usbpd.h>
#include <linux/power_supply.h>

enum port_states {
	PORT_NONE,
	PORT_USB,	
	PORT_DP
};

struct delayed_work *g_sm_work;
enum port_states g_port_states;

#define REGISTER_GERNEAL_0A 0x0A
#define REGISTER_GERNEAL_0C 0x0C
#define CTLSEL_MASK (BIT(0) | BIT(1))
#define CTLSEL_SHIFT 0
#define USB_ONLY 1
#define DP_ONLY 2

#define FLIP_MASK BIT(2)
#define FLIP_SHIFT 2
#define NO_FLIP 0
#define WITH_FLIP 1


struct tusb544_platform_data {
	unsigned int en_gpio;
};

static const struct of_device_id msm_match_table[] = {
	{.compatible = "ti,tusb544"},
	{}
};

enum tusb544_state {
	TUSB544_DEFAULT = 0,
	TUSB544_TEST,
	TUSB544_REGISTER,
};

struct tusb544_dev {
	struct device *dev;
	struct	i2c_client	*client;

	/* GPIO variables */
	unsigned int		en_gpio;

	struct tusb544_platform_data *pdata;

	struct delayed_work	sm_work;

	enum tusb544_state	tusb544_state;
	u8 orientation;

	struct power_supply	*usb_psy;
	struct notifier_block	psy_nb;

	struct usbpd *pd;
	struct usbpd_svid_handler svid_handler;
	enum port_states port;
	enum power_supply_typec_mode typec_mode;
	enum power_supply_type	psy_type;
	bool			vbus_present;
	enum plug_orientation plug_orientation;

	bool isRunning;
};

static const char * const tusb544_i2c_config_direction_strings[] = {
	"usb_dp_source",
	"usb_dp_sink",
	"usb_custom_source",
	"usb_custom_sink"
};

static const char * const tusb544_i2c_config_control_strings[] = {
	"power_down",
	"usb31",
	"alternate_mode",
	"usb31_alternate_mode"
};

static const char * const tusb544_i2c_config_flip_strings[] = {
	"no_flip",
	"with_flip"
};

static int tusb544_write(struct i2c_client *client, unsigned char addr, unsigned char val)
{
	int ret = 0;
	unsigned char buf[2] = { addr, val };

	ret = i2c_master_send(client, buf, 2);
	if (ret < 0) {
		dev_err(&client->dev,
		"%s: - i2c_master_send Error\n", __func__);
		return -ENXIO;
	}

	return ret;
}

static int tusb544_read(struct i2c_client *client, unsigned char addr, unsigned char *val)
{
	int ret = 0;

	ret = i2c_master_send(client, &addr, 1);
	if (ret < 0) {
		dev_err(&client->dev,
		"%s: - i2c_master_send Error\n", __func__);
		goto err_nfcc_hw_check;
	}
	/* hardware dependent delay */
	//msleep(30);

	/* Read Response of RESET command */
	ret = i2c_master_recv(client, val, 1);

	//dev_err(&client->dev,"%s: addr = %x (%x)\n", __func__, addr, *val);

	if (ret < 0) {
		dev_err(&client->dev,
		"%s: - i2c_master_recv Error\n", __func__);
		goto err_nfcc_hw_check;
	}

	goto done;

err_nfcc_hw_check:
	ret = -ENXIO;
	dev_err(&client->dev,
		"%s: - NFCC HW not available\n", __func__);
done:
	return ret;
}

static ssize_t tusb544_i2c_config_direction_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	unsigned char direction;
	const char *direction_str = "error";

	if (tusb544_read(client, REGISTER_GERNEAL_0C, &direction) >= 0)
		direction_str = tusb544_i2c_config_direction_strings[direction & 0x03];

	return sprintf(buf, "%s\n", direction_str);
}

static ssize_t tusb544_i2c_config_direction_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int dir_count = sizeof(tusb544_i2c_config_direction_strings) / sizeof(tusb544_i2c_config_direction_strings[0]);
	int i = -1;

	if (buf != NULL) {
		for (i = dir_count - 1; i >= 0; i--) {
			if (0 == strncmp(buf, tusb544_i2c_config_direction_strings[i], strlen(tusb544_i2c_config_direction_strings[i])))
				break;
		}
	}

	if (i < 0) {
		char help[256] = {'\0'};
		int j;
		for (j = 0; j < dir_count; j++) {
			strcat(help, tusb544_i2c_config_direction_strings[j]);
			strcat(help, " | ");
		}
		help[strlen(help) - 3] = '\0';
		pr_warn("%s: valid inputs: %s\n", __func__, help);
	} else {
		unsigned char direction;
		if (tusb544_read(client, REGISTER_GERNEAL_0C, &direction) >= 0) {
			tusb544_write(client, REGISTER_GERNEAL_0C, (direction & 0xfc) | (unsigned char)i);
		}
	}

	return count;
}

static ssize_t tusb544_i2c_config_control_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	unsigned char control;
	const char *control_str = "error";

	if (tusb544_read(client, REGISTER_GERNEAL_0A, &control) >= 0)
		control_str = tusb544_i2c_config_control_strings[control & 0x03];

	return sprintf(buf, "%s\n", control_str);
}

static ssize_t tusb544_i2c_config_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int ctrl_count = sizeof(tusb544_i2c_config_control_strings) / sizeof(tusb544_i2c_config_control_strings[0]);
	int i = -1;

	if (buf != NULL) {
		for (i = ctrl_count - 1; i >= 0; i--) {
			if (0 == strncmp(buf, tusb544_i2c_config_control_strings[i], strlen(tusb544_i2c_config_control_strings[i])))
				break;
		}
	}

	if (i < 0) {
		char help[256] = {'\0'};
		int j;
		for (j = 0; j < ctrl_count; j++) {
			strcat(help, tusb544_i2c_config_control_strings[j]);
			strcat(help, " | ");
		}
		help[strlen(help) - 3] = '\0';
		pr_warn("%s: valid inputs: %s\n", __func__, help);
	} else {
		unsigned char control;
		if (tusb544_read(client, REGISTER_GERNEAL_0A, &control) >= 0) {
			tusb544_write(client, REGISTER_GERNEAL_0A, (control & 0xfc) | (unsigned char)i);
		}
	}

	return count;
}

static ssize_t tusb544_i2c_config_flip_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	unsigned char flip;
	const char *flip_str = "error";

	if (tusb544_read(client, REGISTER_GERNEAL_0A, &flip) >= 0)
		flip_str = tusb544_i2c_config_flip_strings[(flip & 0x04) >> 2];

	return sprintf(buf, "%s\n", flip_str);
}

static ssize_t tusb544_i2c_config_flip_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int flip_count = sizeof(tusb544_i2c_config_flip_strings) / sizeof(tusb544_i2c_config_flip_strings[0]);
	int i = -1;

	if (buf != NULL) {
		for (i = flip_count - 1; i >= 0; i--) {
			if (0 == strncmp(buf, tusb544_i2c_config_flip_strings[i], strlen(tusb544_i2c_config_flip_strings[i])))
				break;
		}
	}

	if (i < 0) {
		char help[256] = {'\0'};
		int j;
		for (j = 0; j < flip_count; j++) {
			strcat(help, tusb544_i2c_config_flip_strings[j]);
			strcat(help, " | ");
		}
		help[strlen(help) - 3] = '\0';
		pr_warn("%s: valid inputs: %s\n", __func__, help);
	} else {
		unsigned char flip;
		if (tusb544_read(client, REGISTER_GERNEAL_0A, &flip) >= 0) {
			tusb544_write(client, REGISTER_GERNEAL_0A, (flip & 0xfb) | (((unsigned char)i) << 2));
		}
	}

	return count;
}

static ssize_t tusb544_i2c_registers_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	const unsigned char reg_offset[] = {0x0A, 0x0B, 0x0C, 0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23};
	unsigned char value;
	int written = 0;
	int i;

	for (i = 0; i < sizeof(reg_offset) / sizeof(reg_offset[0]); i++) {
		if (tusb544_read(client, reg_offset[i], &value) >= 0)
			written += sprintf(buf + written, "0x%02x: 0x%02x\n", reg_offset[i], value);
	}

	return written;
}

static ssize_t tusb544_i2c_registers_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	char * const format = "0x?? 0x??";
	int i = -1;

	if (buf != NULL) {
		for (i = 0; i < strlen(format); i++)
			if (buf[i] != format[i] && format[i] != '?')
				break;
	}

	if (i >= strlen(format)) {
		char reg_offset_str[] = {buf[2], buf[3], '\0'};
		char value_str[] = {buf[7], buf[8], '\0'};
		unsigned long reg_offset, value;
		int err;

		err = kstrtoul(reg_offset_str, 16, &reg_offset);
		if (err) {
			pr_err("%s: kstrtoul returns error %d", __func__, err);
			goto exit;
		}
		err = kstrtoul(value_str, 16, &value);
		if (err) {
			pr_err("%s: kstrtoul returns error %d", __func__, err);
			goto exit;
		}

		tusb544_write(client, (unsigned char)reg_offset, (unsigned char)value);
	} else {
		pr_warn("%s: valid input format: \"%s\"\n", __func__, format);
	}

exit:
	return count;
}

static struct device_attribute attrs[] =
{
	__ATTR(direction, 0664, tusb544_i2c_config_direction_show, tusb544_i2c_config_direction_store),
	__ATTR(control, 0664, tusb544_i2c_config_control_show, tusb544_i2c_config_control_store),
	__ATTR(flip, 0664, tusb544_i2c_config_flip_show, tusb544_i2c_config_flip_store),
	__ATTR(registers, 0664, tusb544_i2c_registers_show, tusb544_i2c_registers_store),
};

static int tusb544_hw_check(struct i2c_client *client)
{
	int ret = 0;
	unsigned char nci_reset_rsp[6];

	/* Read Response of RESET command */
	ret = i2c_master_recv(client, nci_reset_rsp,
		sizeof(nci_reset_rsp));

	if (ret < 0) {
		dev_err(&client->dev,
		"%s: - i2c_master_recv Error\n", __func__);
		goto err_hw_check;
	}

	dev_err(&client->dev,"%s: %x %x %x %x %x %x \n",
		__func__,
		nci_reset_rsp[0],	nci_reset_rsp[1], nci_reset_rsp[2],
		nci_reset_rsp[3],	nci_reset_rsp[4], nci_reset_rsp[5]);

	goto done;

err_hw_check:
	ret = -ENXIO;
	dev_err(&client->dev,
		"%s: - HW not available\n", __func__);
done:
	return ret;
}

static int nfc_parse_dt(struct device *dev, struct tusb544_platform_data *pdata)
{
	int r = 0;
	struct device_node *np = dev->of_node;

	pdata->en_gpio = of_get_named_gpio(np, "fih,redriver-en", 0);
	if ((!gpio_is_valid(pdata->en_gpio)))
	{
		pr_err("%s gpio is invalid\n", __func__);
		return -EINVAL;
	}

	if (gpio_is_valid(pdata->en_gpio)) {
		r = gpio_request(pdata->en_gpio, "redriver_gpio_en");
		if (r) {
			pr_err("%s: unable to request gpio [%d]\n",
				__func__,
				pdata->en_gpio);
			return -EINVAL;
		}
	}

	r = gpio_direction_output(pdata->en_gpio, 0);

	return r;
}

int tusb544_notify_dp_status(bool connected)
{
	pr_err("%s connected = %d\n", __func__, connected);

	if (connected == true)
		g_port_states = PORT_DP;
	else 
		g_port_states = PORT_NONE;

	schedule_delayed_work(g_sm_work, 0);

	return 0;
}

EXPORT_SYMBOL(tusb544_notify_dp_status);


static void tusb544_sm_work(struct work_struct *w)
{
	struct tusb544_dev *tusb544_dev = container_of(w, struct tusb544_dev, sm_work.work);
	unsigned char value = 0;

	if (tusb544_dev->tusb544_state == TUSB544_DEFAULT) 
	{		
		tusb544_dev->usb_psy = power_supply_get_by_name("usb");
		if (!tusb544_dev->usb_psy) {
			pr_err("Could not get USB power_supply, deferring probe\n");
		}
		tusb544_dev->tusb544_state = TUSB544_REGISTER;
	} 
	else if (tusb544_dev->tusb544_state == TUSB544_REGISTER)
	{
		enum plug_orientation orientiration;
		u8 flipValue, ctlselValue;

		pr_err("%s typec mode:%d present:%d type:%d orientation:%d\n",
			__func__,
			tusb544_dev->typec_mode, tusb544_dev->vbus_present, tusb544_dev->psy_type,
			tusb544_dev->plug_orientation);

		// cable is disappear
		if (tusb544_dev->typec_mode == POWER_SUPPLY_TYPEC_NONE) {
			gpio_direction_output(tusb544_dev->en_gpio, 0);
			tusb544_dev->isRunning = false;
			return;
		}

		// cable is present
		if (tusb544_dev->isRunning == false) {
			gpio_direction_output(tusb544_dev->en_gpio, 1);
			tusb544_dev->isRunning = true;
			// Enable cable mode
			tusb544_write(tusb544_dev->client, REGISTER_GERNEAL_0A, 0x41);
		}

		orientiration = tusb544_dev->plug_orientation;

		if (g_port_states == PORT_DP)
			ctlselValue = DP_ONLY;
		else
			ctlselValue = USB_ONLY;

		if (orientiration == ORIENTATION_CC2)
			flipValue = WITH_FLIP;
		else
			flipValue = NO_FLIP;

		tusb544_read(tusb544_dev->client, REGISTER_GERNEAL_0A, &value);
		pr_err("%s read value = %d\n",__func__, value);
		value &= ~FLIP_MASK;
		value |= (flipValue << FLIP_SHIFT);

		value &= ~CTLSEL_MASK;
		value |= (ctlselValue << CTLSEL_SHIFT);
		
		pr_err("%s try to write value = %d\n",__func__, value);
		tusb544_write(tusb544_dev->client, REGISTER_GERNEAL_0A, value);
	}
	
	return ;
}

static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct tusb544_dev *pd = container_of(nb, struct tusb544_dev, psy_nb);
	union power_supply_propval val;
	enum power_supply_typec_mode typec_mode;
	int ret;
	
	if (ptr != pd->usb_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	ret = power_supply_get_property(pd->usb_psy,
		POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (ret) {
		pr_err("Unable to read USB TYPEC_MODE: %d\n", ret);
		return ret;
	}

	typec_mode = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_PE_START, &val);
	if (ret) {
		pr_err("Unable to read USB PROP_PE_START: %d\n", ret);
		return ret;
	}

	/* Don't proceed if PE_START=0 as other props may still change */
	if (!val.intval &&
			typec_mode != POWER_SUPPLY_TYPEC_NONE)
		return 0;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		pr_err("Unable to read USB PRESENT: %d\n", ret);
		return ret;
	}

	pd->vbus_present = val.intval;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPE, &val);
	if (ret) {
		pr_err("Unable to read USB TYPE: %d\n", ret);
		return ret;
	}

	pd->psy_type = val.intval;

	if (pd->typec_mode == typec_mode)
		return 0;

	pd->typec_mode = typec_mode;

	ret = power_supply_get_property(pd->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, &val);
	if (ret) {
		pr_err("Unable to read USB TYPE: %d\n", ret);
		return ret;
	}

	pd->plug_orientation = val.intval;

	pr_err("%s typec mode:%d present:%d type:%d orientation:%d\n",
		__func__,
		typec_mode, pd->vbus_present, pd->psy_type,
		pd->plug_orientation);

	schedule_delayed_work(&pd->sm_work, 0);

	return 0;
}

static int tusb544_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int r = 0;
	struct tusb544_platform_data *platform_data;
	struct tusb544_dev *tusb544_dev;
	int ret;
	int attr_count;

	dev_dbg(&client->dev, "%s: enter\n", __func__);

	/* Create sysfs attribute files for device */
	for (attr_count = 0; attr_count < sizeof(attrs) / sizeof(attrs[0]); attr_count++) {
		ret = device_create_file(&client->dev, &attrs[attr_count]);
		if (ret) {
			pr_err("error device_create_file\n");
			goto err_device_create_file;
		}
	}

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(struct tusb544_platform_data), GFP_KERNEL);
		if (!platform_data) {
			r = -ENOMEM;
			goto err_platform_data;
		}

		r = nfc_parse_dt(&client->dev, platform_data);
		if (r)
			goto err_free_data;
	} else
		platform_data = client->dev.platform_data;

	if (platform_data == NULL) {
		dev_err(&client->dev, "%s: failed\n", __func__);
		r = -ENODEV;
		goto err_platform_data;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: need I2C_FUNC_I2C\n", __func__);
		r = -ENODEV;
		goto err_free_data;
	}

	tusb544_dev = kzalloc(sizeof(*tusb544_dev), GFP_KERNEL);
	if (tusb544_dev == NULL) {
		r = -ENOMEM;
		goto err_free_data;
	}

	tusb544_dev->client = client;
	tusb544_dev->en_gpio = platform_data->en_gpio;
	tusb544_dev->pdata = platform_data;
	tusb544_dev->tusb544_state = TUSB544_DEFAULT;
	tusb544_dev->typec_mode = POWER_SUPPLY_TYPEC_NONE;
	tusb544_dev->isRunning= false;

	i2c_set_clientdata(client, tusb544_dev);
	tusb544_dev->dev = &client->dev;

	INIT_DELAYED_WORK(&tusb544_dev->sm_work, tusb544_sm_work);

	g_sm_work = &tusb544_dev->sm_work;

	tusb544_hw_check(client);

	// Regist psy change
	tusb544_dev->psy_nb.notifier_call = psy_changed;
	ret = power_supply_reg_notifier(&tusb544_dev->psy_nb);
	if (ret)
		pr_err("%s fail to register power supply\n",__func__);

	// to statup a register timer for usb notifier
	schedule_delayed_work(&tusb544_dev->sm_work, (msecs_to_jiffies(5000)));

	return 0;

err_free_data:
	if (client->dev.of_node)
		devm_kfree(&client->dev, platform_data);
err_platform_data:
	dev_err(&client->dev,
	"%s: probing failed, check hardware\n",
		 __func__);
err_device_create_file:
	for (attr_count--; attr_count >= 0; attr_count--)
		device_remove_file(&client->dev, &attrs[attr_count]);

	return r;
}


static int tusb544_remove(struct i2c_client *client)
{
	int ret = 0;
	struct tusb544_dev *tusb544_dev;
	int attr_count;

	tusb544_dev = i2c_get_clientdata(client);
	if (!tusb544_dev) {
		dev_err(&client->dev,
		"%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	kfree(tusb544_dev);
err:
	for (attr_count = 0; attr_count < sizeof(attrs) / sizeof(attrs[0]); attr_count++)
		device_remove_file(&client->dev, &attrs[attr_count]);

	return ret;
}

static int tusb544_suspend(struct device *device)
{
	return 0;
}

static int tusb544_resume(struct device *device)
{
	return 0;
}

static const struct i2c_device_id tusb544_id[] = {
	{"tusb544-i2c", 0},
	{}
};

static const struct dev_pm_ops tusb544_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tusb544_suspend, tusb544_resume)
};

static struct i2c_driver tusb544 = {
	.id_table = tusb544_id,
	.probe = tusb544_probe,
	.remove = tusb544_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "tusb544",
		.of_match_table = msm_match_table,
		.pm = &tusb544_pm_ops,
	},
};

/*
 * module load/unload record keeping
 */
static int __init tusb544_dev_init(void)
{
	return i2c_add_driver(&tusb544);
}
module_init(tusb544_dev_init);

static void __exit tusb544_dev_exit(void)
{
	i2c_del_driver(&tusb544);
}
module_exit(tusb544_dev_exit);

MODULE_DESCRIPTION("TI TUSB544 Redriver");
MODULE_LICENSE("GPL v2");
