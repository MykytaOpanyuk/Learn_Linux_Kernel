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

static int IRQ_NUM = 107; //IRQ number

MODULE_AUTHOR("John Doe, Mykyta Opanyuk");
MODULE_DESCRIPTION("Testing of button IRQ handling");
MODULE_LICENSE("Dual BSD/GPL");

/* Device context declaration */
struct fake_dev_ctx {
	/* This tasklet, will be scheduled from button IRQ */
	struct tasklet_struct taskl;
	/* spinlock for synchronize hard and soft irq's */
	spinlock_t lock_ctx;
	/* Counter of trigger rising */
	unsigned long long button;
};
/* Fake device passed to interrupt handler */
struct fake_dev_ctx fake_dev;

/* handler */
static irqreturn_t button_irq_handler_up_down(int irq, void *dev_id)
{
	struct fake_dev_ctx *p_ctx = dev_id;
	unsigned long flags;

	spin_lock_irqsave(&p_ctx->lock_ctx, flags);

	p_ctx->button++;

	spin_unlock_irqrestore(&p_ctx->lock_ctx, flags);

	tasklet_schedule(&p_ctx->taskl);
	return IRQ_HANDLED;
}

/* tasklet function */
static void taskl_func(unsigned long param)
{
	struct fake_dev_ctx *p_ctx = (struct fake_dev_ctx*)param;
	unsigned long flags;
	unsigned long long num = 0;
	spin_lock_irqsave(&p_ctx->lock_ctx, flags);

	num = p_ctx->button;

	spin_unlock_irqrestore(&p_ctx->lock_ctx, flags);

	printk(KERN_ALERT "number of button up/down %llu\n", num);
}

/* module */
static int __init test_sync_module_init(void)
{
	printk(KERN_ALERT "Try to init test_sync module\n");
	static int ret = 0;

	memset(&fake_dev, 0, sizeof(fake_dev));
	/* init lock before interrupt requesting - we use it inside interrupt handler */
	spin_lock_init(&fake_dev.lock_ctx);
	/* init tasklet before interrupt requesting - we use it inside interrupt handler */
	tasklet_init(&fake_dev.taskl, taskl_func, (unsigned long)&fake_dev);
	/* request irq */

	ret = request_irq(IRQ_NUM, button_irq_handler_up_down, IRQF_SHARED, "GPIO Power", &fake_dev);
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
	free_irq(IRQ_NUM, &fake_dev);
	tasklet_kill(&fake_dev.taskl);
	printk(KERN_ALERT "Exit test_sync module successful\n");
}

module_init(test_sync_module_init);
module_exit(test_sync_module_exit);
