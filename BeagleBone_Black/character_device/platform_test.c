#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include "platform_test.h"


#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address defined by dts - used for data transfer;
* 2) Two 32-bit registers at address (defined by dts)
*  2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
*/

#define MEM_SIZE			(0x1000)
#define REG_SIZE			(8)
#define DEVICE_POOLING_TIME_MS		(500) /*500 ms*/

#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_I_DATA_READY		(1) /*input data ready flag */
#define PLAT_O_DATA_READY		(1) /*output data ready fla */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */

static struct plat_dummy_device *mydevs[DUMMY_DEVICES];

static u32 plat_dummy_read_mem_read8(
		struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->mem_read + offset);
}

static u32 plat_dummy_read_reg_read32(
		struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs_read + offset);
}

static void plat_dummy_read_reg_write32(
		struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs_read + offset);
}

static u32 plat_dummy_write_mem_read8(
		struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->mem_write + offset);
}

static void plat_dummy_write_mem_write8(
		struct plat_dummy_device *my_dev, u32 offset, u8 val)
{
	iowrite8(val, my_dev->mem_write + offset);
}

static u32 plat_dummy_write_reg_read32(
		struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->regs_write + offset);
}

static void plat_dummy_write_reg_write32(
		struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->regs_write + offset);
}

/* How much space in read mem is free? */
static int spacefree_read(struct plat_dummy_device *my_dev)
{
	if (my_dev->rp_read == my_dev->wp_read)
		return my_dev->buffersize_read;
	return ((my_dev->rp_read + my_dev->buffersize_read -
		 my_dev->wp_read) % my_dev->buffersize_read);
}

/* How much free mem in write we can use for copy_from_user */
static int bytes_for_write(struct plat_dummy_device *my_dev)
{
	if (my_dev->rp_write == my_dev->wp_write)
		return my_dev->buffersize_write;
	return (my_dev->wp_write - my_dev->rp_write);
}

static ssize_t plat_dummy_read(struct plat_dummy_device *my_device,
			       char __user *buf, size_t count)
{
	if (!my_device)
		return -EFAULT;

	if (mutex_lock_interruptible(&my_device->rd_mutex))
		return -ERESTARTSYS;

	pr_info("++%s: my_device->rp_read = %p, "
		"my_device->wp_read = %p\n",
		__func__, my_device->rp_read, my_device->wp_read);

	while (my_device->rp_read == my_device->wp_read) { /* nothing to read */
		mutex_unlock(&my_device->rd_mutex); /* release the lock */
		pr_info("\"%s\" reading: going to sleep\n", current->comm);

		/* signal: tell the fs layer to handle it */
		if (wait_event_interruptible(my_device->rwq,
			(my_device->rp_read != my_device->wp_read)))
			return -ERESTARTSYS;

		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&my_device->rd_mutex))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	pr_info("++%s: count = %zu, my_device->rp_read = %p, "
		"my_device->wp_read = %p\n", __func__,
		count, my_device->rp_read, my_device->wp_read);

	if (my_device->wp_read > my_device->rp_read)
		count = min(count, (size_t)(my_device->wp_read - my_device->rp_read));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(my_device->end_read - my_device->rp_read));

	if (copy_to_user(buf, my_device->rp_read, count)) {
		mutex_unlock (&my_device->rd_mutex);
		return -EFAULT;
	}
	my_device->rp_read += count;
	if (my_device->rp_read == my_device->end_read)
		my_device->rp_read = my_device->buffer_read; /* wrapped */
	mutex_unlock (&my_device->rd_mutex);

	pr_info("\"%s\" did read %li bytes\n", current->comm, (long)count);
	return count;
}

static ssize_t plat_dummy_write(struct plat_dummy_device *my_device,
			       const char __user *buf, size_t count)
{
	u32 index;
	char *timetable_buf = (char *)vmalloc(count);

	if (!my_device)
		return -EFAULT;

	if (mutex_lock_interruptible(&my_device->wr_mutex))
		return -ERESTARTSYS;

	/* data from user is there, return something */
	pr_info("++%s: count = %zu, my_device->rp_write = %p, "
		"my_device->wp_write = %p\n", __func__,
		count, my_device->rp_write, my_device->wp_write);

	if (copy_from_user(timetable_buf, buf, count)) {
		mutex_unlock (&my_device->wr_mutex);
		return -EFAULT;
	}

	plat_dummy_write_reg_write32(my_device, PLAT_IO_SIZE_REG, count);

	for (index = 0; index < count; index++) {
		*my_device->wp_write = timetable_buf[index];

		if ((index + 1) % 4096 != 0) {
			my_device->wp_write++;

			if (my_device->wp_write == my_device->end_write)
				my_device->wp_write = my_device->buffer_write; /* wrapped */
		}

		if (((index + 1) % 4096 == 0) && (index != 0)) {
			pr_info("++%s: before sleep index = %zu, my_device->rp_write = %p, "
				"my_device->wp_write = %p\n", __func__,
				(index + 1), my_device->rp_write, my_device->wp_write);

//			plat_dummy_write_reg_write32(my_device,
//				PLAT_IO_FLAG_REG, PLAT_O_DATA_READY);

			while (my_device->rp_write != my_device->wp_write) {
				/* waiting for driver read old info from user */
				mutex_unlock(&my_device->wr_mutex); /* release the lock */
				pr_info("\"%s\" reading: going to sleep\n", current->comm);

				/* signal: tell the fs layer to handle it */
				if (wait_event_interruptible(my_device->wwq,
					(my_device->rp_write == my_device->wp_write)))
					return -ERESTARTSYS;

				/* otherwise loop, but first reacquire the lock */
				if (mutex_lock_interruptible(&my_device->wr_mutex))
					return -ERESTARTSYS;
			}
		}
	}

	mutex_unlock (&my_device->wr_mutex);

	pr_info("\"%s\" did write %li bytes\n", current->comm, (long)count);
	vfree(timetable_buf);
	return count;
}

/*intervals in ms*/
#define MIN_PULL_INTERVAL 10
#define MAX_PULL_INTERVAL 10000

int set_poll_interval(struct plat_dummy_device *my_device, u32 ms_interval)
{
	pr_info("++%s(%p)\n", __func__, my_device);
	if (!my_device)
		return -EFAULT;

	if ((ms_interval < MIN_PULL_INTERVAL) ||
	     (ms_interval > MAX_PULL_INTERVAL)) {
		pr_err("%s: Value out of range %d\n", __func__, ms_interval);
		return -EFAULT;
	}
	spin_lock(&my_device->pool_lock);
	my_device->js_pool_time = msecs_to_jiffies(ms_interval);
	spin_unlock(&my_device->pool_lock);
	pr_info("%s: Setting Poliing Interval to %d ms\n", __func__, ms_interval);
	return 0;
}

int plat_dummy_mmap_read(struct plat_dummy_device *my_device, struct vm_area_struct *vma) {
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (size <= my_device->phys_mem_size_read)
		ret = io_remap_pfn_range(vma, vma->vm_start,
					 my_device->phys_mem_addr_read >> PAGE_SHIFT,
					 size, vma->vm_page_prot);
	else
		return -EINVAL;

	if (ret)
		return -EAGAIN;

	return 0;
}

int plat_dummy_mmap_write(struct plat_dummy_device *my_device, struct vm_area_struct *vma) {
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (size <= my_device->phys_mem_size_write)
		ret = io_remap_pfn_range(vma, vma->vm_start,
					 my_device->phys_mem_addr_write >> PAGE_SHIFT,
					 size, vma->vm_page_prot);
	else
		return -EINVAL;

	if (ret)
		return -EAGAIN;

	return 0;
}


static void plat_dummy_work_read(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status, count;
	u64 js_time;

	my_device = container_of(work, struct plat_dummy_device, dwork_read.work);

	spin_lock(&my_device->pool_lock);
	js_time = my_device->js_pool_time;
	spin_unlock(&my_device->pool_lock);

	status = plat_dummy_read_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_I_DATA_READY) {
	
		if (mutex_lock_interruptible(&my_device->rd_mutex))
			goto exit_wq;

		
		size = plat_dummy_read_reg_read32(my_device, PLAT_IO_SIZE_REG);
		pr_info("++%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		count = min((size_t)size, (size_t)spacefree_read(my_device));
		pr_info("++%s: count = %d\n", __func__, count);
		if (count < size) {
			mutex_unlock(&my_device->rd_mutex);
			goto exit_wq;
		}

		for(i = 0; i < count; i++) {
			if (my_device->wp_read == my_device->end_read) {
				my_device->wp_read =
						my_device->buffer_read;
				pr_info("Start write into begin of buffer\n");
			}/* wrapped */
			*my_device->wp_read++ =
				plat_dummy_read_mem_read8(my_device,
					/*my_device->residue + */i);

//			pr_info("%s: mem[%d] = 0x%x ('%u')\n", __func__, i,
//				*(my_device->wp_read - 1),
//				*(my_device->wp_read - 1));

		}

		pr_info("++%s: after reading my_device->rp_write = %p, "
			"my_device->wp_write = %p\n", __func__,
			my_device->rp_read, my_device->wp_read);

		mutex_unlock (&my_device->rd_mutex);
		wake_up_interruptible(&my_device->rwq);
		rmb();
		status &= ~PLAT_I_DATA_READY;
		plat_dummy_read_reg_write32(my_device, PLAT_IO_FLAG_REG, status);
	}
exit_wq:
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork_read, js_time);
}

static void plat_dummy_work_write(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, status; /*size - how much bytes can read dev for 1 operation*/
	u64 js_time;
	u8 data;

	my_device = container_of(work, struct plat_dummy_device, dwork_write.work);

	spin_lock(&my_device->pool_lock);
	js_time = my_device->js_pool_time;
	spin_unlock(&my_device->pool_lock);

	status = plat_dummy_write_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_O_DATA_READY) {

		if (mutex_lock_interruptible(&my_device->wr_mutex))
			goto exit_wq;

		while(my_device->rp_write != my_device->wp_write) {
			plat_dummy_write_mem_write8(my_device,
				i, *my_device->rp_write);
			i++;
			my_device->rp_write++;
			if (my_device->rp_write == my_device->end_write)
				my_device->rp_write =
					my_device->buffer_write; /* wrapped */

			pr_info("%s: mem[%d] = 0x%x ('%d')\n", __func__, i,
				*(my_device->rp_write - 1),
				*(my_device->rp_write - 1));
		}

		pr_info("++%s: after writing my_device->rp_write = %p, "
			"my_device->wp_write = %p\n", __func__,
			my_device->rp_write, my_device->wp_write);

		mutex_unlock(&my_device->wr_mutex);
		wake_up_interruptible(&my_device->wwq);
		wmb();
		status &= ~PLAT_O_DATA_READY;
		plat_dummy_write_reg_write32(my_device, PLAT_IO_FLAG_REG, status);
	}
exit_wq:
	queue_delayed_work(my_device->data_write_wq, &my_device->dwork_write, js_time);
}

static void dummy_init_data_buffer(struct plat_dummy_device *my_device)
{
	my_device->buffersize_read = DUMMY_IO_BUFF_SIZE;
	my_device->end_read = my_device->buffer_read + my_device->buffersize_read;
	my_device->rp_read = my_device->wp_read = my_device->buffer_read;

	my_device->buffersize_write = DUMMY_IO_BUFF_SIZE;
	my_device->end_write = my_device->buffer_write + my_device->buffersize_write;
	my_device->rp_write = my_device->wp_write = my_device->buffer_write;
}

static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "ti,plat_dummy",
	}, {
	},
 };

struct plat_dummy_device *get_dummy_platfrom_device(enum dummy_dev devnum)
{
	if (mydevs[devnum])
		return mydevs[devnum];

	return NULL;
}
EXPORT_SYMBOL(get_dummy_platfrom_device);

static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int id;

	pr_info("++%s(%p)\n", __func__, pdev);

	if (!np) {
		pr_info("No device node found!\n");
		return -ENOMEM;
	}
	pr_info("Device name: %s\n", np->name);
	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	id = of_alias_get_id(np, "dummy");
	if (id < 0) {
		dev_err(&pdev->dev, "failed to get alias id: %d\n", id);
		return id;
	}
	pr_info("Device ID: %d\n", id);

	if (id >= DUMMY_DEVICES) {
		pr_err("Device ID is wrong!\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("res 0 = %zx..%zx\n", res->start, res->end);

	/*This is our memory region, save it for mmap*/
	my_device->phys_mem_addr_read = res->start;
	my_device->phys_mem_size_read = resource_size(res);

	my_device->mem_read = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->mem_read))
		return PTR_ERR(my_device->mem_read);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("res 1 = %zx..%zx\n", res->start, res->end);

	my_device->regs_read = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs_read))
		return PTR_ERR(my_device->regs_read);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	pr_info("res 0 = %zx..%zx\n", res->start, res->end);

	/*This is our memory region, save it for mmap*/
	my_device->phys_mem_addr_write = res->start;
	my_device->phys_mem_size_write = resource_size(res);

	my_device->mem_write = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->mem_write))
		return PTR_ERR(my_device->mem_write);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	pr_info("res 1 = %zx..%zx\n", res->start, res->end);

	my_device->regs_write = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->regs_write))
		return PTR_ERR(my_device->regs_write);

	platform_set_drvdata(pdev, my_device);

	pr_info("READ Memory mapped to %p\n", my_device->mem_read);
	pr_info("READ Registers mapped to %p\n", my_device->regs_read);

	pr_info("WRITE Memory mapped to %p\n", my_device->mem_write);
	pr_info("WRITE Registers mapped to %p\n", my_device->regs_write);
		
	mutex_init(&my_device->rd_mutex);
	mutex_init(&my_device->wr_mutex);
	init_waitqueue_head(&my_device->rwq);
	init_waitqueue_head(&my_device->wwq);

	dummy_init_data_buffer(my_device);

	my_device->dummy_read = plat_dummy_read;
	my_device->dummy_write = plat_dummy_write;
	my_device->set_poll_interval = set_poll_interval;
	my_device->dummy_mmap_read = plat_dummy_mmap_read;
	my_device->dummy_mmap_write = plat_dummy_mmap_write;
	spin_lock_init(&my_device->pool_lock);

	my_device->pdev = pdev;
	mydevs[id] = my_device;

	/*Init data read WQ*/
	my_device->data_read_wq = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_read_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork_read, plat_dummy_work_read);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_read_wq, &my_device->dwork_read, 0);

	/*Init data write WQ*/
	my_device->data_write_wq = alloc_workqueue("plat_dummy_write",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_write_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork_write, plat_dummy_work_write);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_write_wq, &my_device->dwork_write, 0);

	return (PTR_ERR_OR_ZERO(my_device->mem_read) || PTR_ERR_OR_ZERO(my_device->mem_write));
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s(%p)\n", __func__, pdev);

	if (my_device->data_read_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork_read);
		pr_info("delayed 'read' work cancelled\n");
		destroy_workqueue(my_device->data_read_wq);
	}

	if (my_device->data_write_wq) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork_write);
		pr_info("delayed 'read' work cancelled\n");
		destroy_workqueue(my_device->data_write_wq);
	}

	pr_info("--%s\n", __func__);
	return 0;
}

static struct platform_driver plat_dummy_driver = {
	.driver = {
			.name = DRV_NAME,
			.of_match_table = plat_dummy_of_match,
	},
	.probe		= plat_dummy_probe,
	.remove		= plat_dummy_remove,
};

MODULE_DEVICE_TABLE(of, plat_dummy_of_match);
module_platform_driver(plat_dummy_driver);

MODULE_AUTHOR("Vitaliy Vasylskyy <vitaliy.vasylskyy@globallogic.com>,"
	      "Mykyta Opaniuk <mykyta.opaniuk@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");
