/*
* mouse_irq.c - Module for testing IRQ's handling
*
* Hard IRQ schedules tasklet
*
* Copyright (C)
*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or (at
* your option) any later version.

*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#define GPIO2_8		32 + 32 + 8 //IRQ number
static unsigned int irqNumber;

MODULE_AUTHOR("John Doe, Mykyta Opanyuk");
MODULE_DESCRIPTION("Testing of button IRQ handling");
MODULE_LICENSE("Dual BSD/GPL");

/* Device context declaration */
struct fake_dev_ctx {
	/* This tasklet, will be scheduled from button IRQ */
	struct tasklet_struct taskl;
	/* Counter of trigger rising */
	unsigned long long button;
};

/* Fake device passed to interrupt handler */
struct fake_dev_ctx fake_dev;

/* handler */
static irqreturn_t button_irq_handler_up_down(int irq, void *dev_id)
{
	struct fake_dev_ctx *p_ctx = dev_id;

	p_ctx->button++;

	tasklet_schedule(&p_ctx->taskl);
	return IRQ_HANDLED;
}

/* tasklet function */
static void taskl_func(unsigned long param)
{
	struct fake_dev_ctx *p_ctx = (struct fake_dev_ctx*)param;
	unsigned long long num = 0;

	num = p_ctx->button;

	printk(KERN_ALERT "number of button up/down %llu\n", num);
}

/* module */
static int __init test_sync_module_init(void)
{
	printk(KERN_ALERT "Try to init test_sync module.\n");

	static int ret = 0;

	memset(&fake_dev, 0, sizeof(fake_dev));
	/* init tasklet before interrupt requesting - we use it inside interrupt handler */
	tasklet_init(&fake_dev.taskl, taskl_func, (unsigned long)&fake_dev);
	/* request irq */

	ret = gpio_request(GPIO2_8, "user_boot");
	if (ret != 0)
		printk("=== gpio_request FAILED. ret = %d;\n", ret);

	ret = gpio_direction_input(GPIO2_8);
	if (ret != 0)
		printk("=== gpio_direction_input FAILED.\n");

	irqNumber = gpio_to_irq(GPIO2_8);
	if (irqNumber == -EINVAL)
		printk("=== gpio_to_irq FAILED.\n");

	printk("=== The button is mapped to IRQ: %d\n", irqNumber);

	ret = gpio_get_value(GPIO2_8);
	printk("=== The button value now: %d\n", ret);

	ret = request_irq(irqNumber, button_irq_handler_up_down, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "user boot", &fake_dev);
	if (ret != 0) {
		printk("=== FAILED; ret = %d \n", ret);
		return -EBUSY;
	}

	printk(KERN_ALERT "Init test_sync module successful\n");

	return 0;
}

static void __exit test_sync_module_exit(void)
{
	printk(KERN_ALERT "Try to exit test_sync module\n");
	/* free IRQ before tasklet kill to prevent it scheduling */
	free_irq(irqNumber, &fake_dev);
	gpio_free(GPIO2_8);
	tasklet_kill(&fake_dev.taskl);
	printk(KERN_ALERT "Exit test_sync module successful\n");
}

module_init(test_sync_module_init);
module_exit(test_sync_module_exit);
