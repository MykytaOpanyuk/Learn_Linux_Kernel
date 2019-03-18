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

/* Timer flags :



 * A deferrable timer will work normally when the system is busy, but
 * will not cause a CPU to come out of idle just to service it; instead,
 * the timer will be serviced when the CPU eventually wakes up with a
 * subsequent non-deferrable timer.
 *
 * An irqsafe timer is executed with IRQ disabled and it's safe to wait for
 * the completion of the running instance from IRQ handlers, for example,
 * by calling del_timer_sync().
 *
 * Note: The irq disabled callback execution is a special case for
 * workqueue locking issues. It's not meant for executing random crap
 * with interrupts disabled. Abuse is monitored!

#define TIMER_CPUMASK		0x0003FFFF
#define TIMER_MIGRATING		0x00040000
#define TIMER_BASEMASK		(TIMER_CPUMASK | TIMER_MIGRATING)
#define TIMER_DEFERRABLE	0x00080000
#define TIMER_PINNED		0x00100000
#define TIMER_IRQSAFE		0x00200000
#define TIMER_ARRAYSHIFT	22
#define TIMER_ARRAYMASK		0xFFC00000
*/

MODULE_AUTHOR("John Doe, Mykyta Opanyuk");
MODULE_DESCRIPTION("Testing of button IRQ handling");
MODULE_LICENSE("Dual BSD/GPL");

struct timer_list new_timer;
struct tasklet_struct new_task;
static unsigned int ret = 0;

/* tasklet function */
static void taskl_func(unsigned long param)
{
	printk(KERN_ALERT "Jiffies at tasklet : %lu\n", jiffies);
}

static void timer_func(struct timer_list *unused) /* timer callback func*/
{
	printk(KERN_ALERT "We are at timer_func!\n");
	printk(KERN_ALERT "Getting flags from timer : %u\n", unused->flags);
	printk(KERN_ALERT "Jiffies at timer_func : %lu\n", jiffies);
	printk(KERN_ALERT "Ret value after timer_func = %u!\n<->\n", ret);
	
	ret++;

	tasklet_schedule(&new_task);

	if (ret < 5) /* reset a new callback time for a timer 5th times */
		mod_timer(&new_timer, jiffies + 1 * HZ);
}

/* module */
static int __init timer_jiffies_init(void)
{
	printk(KERN_ALERT "Try to init timer_jiffies test module.\n");

	tasklet_init(&new_task, taskl_func, 0);
	printk(KERN_ALERT "Tasklet_init complete.\n");
	printk(KERN_ALERT "timer_setup complete.\n");

	timer_setup(&new_timer, timer_func, 0); /* new API for init timers */
	mod_timer(&new_timer, jiffies + 1 * HZ); /* set time for timer callback */


	printk(KERN_ALERT "add_timer complete.\n");
	printk(KERN_ALERT "Init test_sync module successful!\n"
			  "Ret is : %u\n", ret);

	return 0;
}

static void __exit timer_jiffies_exit(void)
{
	del_timer_sync(&new_timer);
	tasklet_kill(&new_task);
	printk(KERN_ALERT "Exit test_sync module successful\n");
}

module_init(timer_jiffies_init);
module_exit(timer_jiffies_exit);
