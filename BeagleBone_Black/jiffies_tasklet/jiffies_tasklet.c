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
#include <linux/time.h>
#include <linux/interrupt.h>

MODULE_AUTHOR("Mykyta Opaniuk");
MODULE_DESCRIPTION("Testing jiffies/taslket");
MODULE_LICENSE("Dual BSD/GPL");

struct tasklet_struct new_task;

/* tasklet function */
static void taskl_func(unsigned long param)
{
	printk(KERN_ALERT "Jiffies at tasklet : %lu\n", jiffies);
}

/* module */
static int __init jiffies_init(void)
{
	printk(KERN_ALERT "Try to init jiffies test module.\n");
	tasklet_init(&new_task, taskl_func, 0);

	printk(KERN_ALERT "Jiffies at init : %lu\n", jiffies);
	printk(KERN_ALERT "Tasklet_init complete.\n");
	tasklet_schedule(&new_task);

	return 0;
}

static void __exit jiffies_exit(void)
{
	tasklet_kill(&new_task);
	printk(KERN_ALERT "Exit jiffies tasklet module successful\n");
}

module_init(jiffies_init);
module_exit(jiffies_exit);
