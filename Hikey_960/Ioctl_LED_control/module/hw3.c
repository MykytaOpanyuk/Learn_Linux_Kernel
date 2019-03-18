// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for handling button and LED on HiKey960 board.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>

#include "hw3.h"

#define DRIVER_NAME	"hw3"
#define WRITE_BUF_LEN	10
#define READ_BUF_LEN	2	/* 1 character and \0 */

struct hw3 {
	struct miscdevice mdev;
	struct gpio_desc *btn_gpio;
	struct gpio_desc *led_gpio;
	int btn_irq;
	int led_on;
	int btn_on;
	int control;			/* kernel controls LED switching? */
	spinlock_t lock;
	wait_queue_head_t wait;
	bool data_ready;		/* new data ready to read */
};

static inline struct hw3 *to_hw3_struct(struct file *file)
{
	struct miscdevice *miscdev = file->private_data;

	return container_of(miscdev, struct hw3, mdev);
}

static ssize_t hw3_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	struct hw3 *hw3 = to_hw3_struct(file);
	unsigned long flags;
	ssize_t ret;
	const char *val;

	spin_lock_irqsave(&hw3->lock, flags);
	while (!hw3->data_ready) {
		spin_unlock_irqrestore(&hw3->lock, flags);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(hw3->wait, hw3->data_ready))
			return -ERESTARTSYS;
		spin_lock_irqsave(&hw3->lock, flags);
	}

	val = hw3->btn_on ? "1" : "0";
	hw3->data_ready = false;
	spin_unlock_irqrestore(&hw3->lock, flags);

	/* Do not advance ppos, do not use simple_read_from_buffer() */
	if (copy_to_user(buf, val, READ_BUF_LEN))
		ret = -EFAULT;
	else
		ret = READ_BUF_LEN;

	return ret;
}

static ssize_t hw3_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct hw3 *hw3 = to_hw3_struct(file);
	unsigned long flags;
	char kbuf[WRITE_BUF_LEN];
	long val;
	int res;

	/* Do not advance ppos, do not use simple_write_to_buffer() */
	if (copy_from_user(kbuf, buf, WRITE_BUF_LEN))
		return -EFAULT;

	kbuf[1] = '\0'; /* get rid of possible \n from "echo" command */
	res = kstrtol(kbuf, 0, &val);
	if (res)
		return -EINVAL;
	val = !!val;

	spin_lock_irqsave(&hw3->lock, flags);
	if (hw3->led_on != val) {
		hw3->led_on = val;
		gpiod_set_value(hw3->led_gpio, hw3->led_on);
	}
	spin_unlock_irqrestore(&hw3->lock, flags);

	return count;
}

static unsigned int hw3_poll(struct file *file, poll_table *wait)
{
	struct hw3 *hw3 = to_hw3_struct(file);
	unsigned long flags;
	unsigned int mask = 0;

	poll_wait(file, &hw3->wait, wait);

	spin_lock_irqsave(&hw3->lock, flags);
	if (hw3->data_ready)
		mask = POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&hw3->lock, flags);

	return mask;
}

static long hw3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hw3 *hw3 = to_hw3_struct(file);
	unsigned long flags;
	int val;

	switch (cmd) {
	case HW3IOC_SETLED:
		if (get_user(val, (int __user *)arg))
			return -EFAULT;

		spin_lock_irqsave(&hw3->lock, flags);
		hw3->led_on = !!val;
		gpiod_set_value(hw3->led_gpio, hw3->led_on);
		spin_unlock_irqrestore(&hw3->lock, flags);
		break;
		/* Fall through */
	case HW3IOC_GETLED:
		spin_lock_irqsave(&hw3->lock, flags);
		val = hw3->led_on;
		spin_unlock_irqrestore(&hw3->lock, flags);

		return put_user(val, (int __user *)arg);
	case HW3IOC_KERN_CONTROL:
		if (get_user(val, (int __user *)arg))
			return -EFAULT;

		spin_lock_irqsave(&hw3->lock, flags);
		hw3->control = !!val;
		spin_unlock_irqrestore(&hw3->lock, flags);

		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations hw3_fops = {
	.owner		= THIS_MODULE,
	.read		= hw3_read,
	.write		= hw3_write,
	.poll		= hw3_poll,
	.unlocked_ioctl = hw3_ioctl,
	.llseek		= no_llseek,
};

static irqreturn_t hw3_btn_isr(int irq, void *data)
{
	struct hw3 *hw3 = data;
	unsigned long flags;

	spin_lock_irqsave(&hw3->lock, flags);
	hw3->data_ready = true;
	hw3->btn_on = gpiod_get_value(hw3->btn_gpio);
	if (hw3->btn_on && hw3->control) {
		hw3->led_on ^= 0x1;
		gpiod_set_value(hw3->led_gpio, hw3->led_on);
	}
	spin_unlock_irqrestore(&hw3->lock, flags);

	wake_up_interruptible(&hw3->wait);

	return IRQ_HANDLED;
}

static int hw3_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hw3 *hw3;
	int ret;

	hw3 = devm_kzalloc(&pdev->dev, sizeof(*hw3), GFP_KERNEL);
	if (!hw3)
		return -ENOMEM;

	hw3->control = 0;

	/* "button-gpios" in dts */
	hw3->btn_gpio = devm_gpiod_get(dev, "button", GPIOD_IN);
	if (IS_ERR(hw3->btn_gpio))
		return PTR_ERR(hw3->btn_gpio);

	/* "led-gpios" in dts */
	hw3->led_gpio = devm_gpiod_get(dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(hw3->led_gpio))
		return PTR_ERR(hw3->led_gpio);

	hw3->btn_irq = gpiod_to_irq(hw3->btn_gpio);
	if (hw3->btn_irq < 0)
		return hw3->btn_irq;

	ret = devm_request_irq(dev, hw3->btn_irq, hw3_btn_isr,
			       IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			       dev_name(dev), hw3);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, hw3);
	spin_lock_init(&hw3->lock);
	init_waitqueue_head(&hw3->wait);

	hw3->mdev.minor		= MISC_DYNAMIC_MINOR;
	hw3->mdev.name		= DRIVER_NAME;
	hw3->mdev.fops		= &hw3_fops;
	hw3->mdev.parent	= dev;
	ret = misc_register(&hw3->mdev);
	if (ret)
		return ret;

	gpiod_set_value(hw3->led_gpio, 0);

	return 0;
}

static int hw3_remove(struct platform_device *pdev)
{
	struct hw3 *hw3 = platform_get_drvdata(pdev);

	misc_deregister(&hw3->mdev);
	return 0;
}

static const struct of_device_id hw3_of_match[] = {
	{ .compatible = "globallogic,hw3" },
	{ .compatible = "globallogic,hw4" },
	{ .compatible = "globallogic,hw5" },
	{}, /* sentinel */
};
MODULE_DEVICE_TABLE(of, hw3_of_match);

static struct platform_driver hw3_driver = {
	.probe = hw3_probe,
	.remove = hw3_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = hw3_of_match,
	},
};

module_platform_driver(hw3_driver);

MODULE_ALIAS("platform:hw3");
MODULE_AUTHOR("Sam Protsenko <semen.protsenko@globallogic.com>");
MODULE_DESCRIPTION("Test module 3");
MODULE_LICENSE("GPL");
