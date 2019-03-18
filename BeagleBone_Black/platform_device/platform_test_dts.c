#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/io.h>

#define DRV_NAME  "plat_dummy"
/*Device has 2 resources:
* 1) 4K of memory at address defined by dts - used for data transfer;
* 2) Two 32-bit registers at address (defined by dts)
*  2.1. Flag Register: @offset 0
*	bit 0: PLAT_IO_DATA_READY - set to 1 if data from device ready
*	other bits: reserved;
* 2.2. Data size Register @offset 4: - Contain data size from device (0..4095);
*/
#define MEM_SIZE	(4096)
#define REG_SIZE	(8)
#define DEVICE_POOLING_TIME_MS (500) /*500 ms*/
/**/
#define PLAT_IO_FLAG_REG		(0) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(4) /*Offset of flag register*/
#define PLAT_I_DATA_READY		(1) /*input data ready flag */
#define PLAT_O_DATA_READY		(1) /*output data ready fla */
#define MAX_DUMMY_PLAT_THREADS 1 /*Maximum amount of threads for this */


struct plat_dummy_device {
	void __iomem *read_mem;
	void __iomem *read_regs;
	void __iomem *write_mem;
	void __iomem *write_regs;
	struct delayed_work     dwork_read;
	struct workqueue_struct *data_wq_read;
	struct delayed_work     dwork_write;
	struct workqueue_struct *data_wq_write;
	u64 js_pool_time;
};

static u32 plat_dummy_read_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->read_mem + offset);
}

static u32 plat_dummy_read_reg_read32(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->read_regs + offset);
}

static void plat_dummy_read_reg_write32(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->read_regs + offset);
}

static u32 plat_dummy_write_mem_read8(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread8(my_dev->write_mem + offset);
}

static void plat_dummy_write_mem_write8(struct plat_dummy_device *my_dev, u32 offset, u8 val)
{
	iowrite8(val, my_dev->write_mem + offset);
}

static u32 plat_dummy_write_reg_read32(struct plat_dummy_device *my_dev, u32 offset)
{
	return ioread32(my_dev->write_regs + offset);
}

static void plat_dummy_write_reg_write32(struct plat_dummy_device *my_dev, u32 offset, u32 val)
{
	iowrite32(val, my_dev->write_regs + offset);
}

static void plat_dummy_work_read(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, size, status;
	u8 data;

	pr_info("++%s(%u)\n", __func__, jiffies_to_msecs(jiffies));

	my_device = container_of(work, struct plat_dummy_device, dwork_read.work);

	status = plat_dummy_read_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_I_DATA_READY) {
		size = plat_dummy_read_reg_read32(my_device, PLAT_IO_SIZE_REG);
		pr_info("%s: size = %d\n", __func__, size);

		if (size > MEM_SIZE)
			size = MEM_SIZE;

		for(i = 0; i < size; i++) {
			data = plat_dummy_read_mem_read8(my_device, i);
			pr_info("%s: mem[%d] = 0x%x (%u)\n", __func__,  i, data, data);
		}
		rmb();
		status &= ~PLAT_I_DATA_READY;
		plat_dummy_read_reg_write32(my_device, PLAT_IO_FLAG_REG, status);
	}
	queue_delayed_work(my_device->data_wq_read, &my_device->dwork_read, my_device->js_pool_time);
}

static void plat_dummy_work_write(struct work_struct *work)
{
	struct plat_dummy_device *my_device;
	u32 i, status, size_for_write = 128;
	u8 data;

	pr_info("++%s(%u)\n", __func__, jiffies_to_msecs(jiffies));

	my_device = container_of(work, struct plat_dummy_device, dwork_write.work);

	status = plat_dummy_write_reg_read32(my_device, PLAT_IO_FLAG_REG);

	if (status & PLAT_O_DATA_READY) {
		plat_dummy_write_reg_write32(my_device, PLAT_IO_SIZE_REG, size_for_write);
		pr_info("%s: size = %d\n", __func__, size_for_write);

		printk(KERN_WARNING "Start writing in kernel space!");

		for(i = 0; i < size_for_write; i++) {
			data = ((i + 32) * 5) % 256;
			plat_dummy_write_mem_write8(my_device, i, data);
		}
		wmb();
		status &= ~PLAT_O_DATA_READY;

		u8 *timetable = (u8 *)my_device->write_mem;
		for(i = 0; i < size_for_write; i++) {
			data = plat_dummy_write_mem_read8(my_device, i);
			printk(KERN_WARNING "status of write_reg: %u", data);
			timetable++;
		}

		plat_dummy_write_reg_write32(my_device, PLAT_IO_FLAG_REG, status);
	}
	queue_delayed_work(my_device->data_wq_write, &my_device->dwork_write, my_device->js_pool_time);
}

static const struct of_device_id plat_dummy_of_match[] = {
	{
		.compatible = "ti,plat_dummy",
	}, {
	},
 };

static int plat_dummy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct plat_dummy_device *my_device;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	pr_info("++%s\n", __func__);

	if (!np) {
		pr_info("No device node found!\n");
		return -ENOMEM;
	}
	pr_info("Device name: %s\n", np->name);
	my_device = devm_kzalloc(dev, sizeof(struct plat_dummy_device), GFP_KERNEL);
	if (!my_device)
		return -ENOMEM;

	/* Get resources (mem and regs) for "read from userspace" */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pr_info("res 0 = %zx..%zx\n", res->start, res->end);
	my_device->read_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->read_mem))
		return PTR_ERR(my_device->read_mem);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pr_info("res 1 = %zx..%zx\n", res->start, res->end);
	my_device->read_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->read_regs))
		return PTR_ERR(my_device->read_regs);

	/* Get resources (mem and regs) for "write to userspace" */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	pr_info("res 2 = %zx..%zx\n", res->start, res->end);

	my_device->write_mem = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->write_mem))
		return PTR_ERR(my_device->write_mem);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	pr_info("res 3 = %zx..%zx\n", res->start, res->end);

	my_device->write_regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(my_device->write_regs))
		return PTR_ERR(my_device->write_regs);

	platform_set_drvdata(pdev, my_device);

	pr_info("\"Read\" memory mapped to %p\n", my_device->read_regs);
	pr_info("\"Read\" Registers mapped to %p\n", my_device->read_mem);

	pr_info("\"Write\" memory mapped to %p\n", my_device->write_regs);
	pr_info("\"Write\" Registers mapped to %p\n", my_device->write_mem);

	/*Init data read WQ*/
	my_device->data_wq_read = alloc_workqueue("plat_dummy_read",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_wq_read)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork_read, plat_dummy_work_read);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_wq_read, &my_device->dwork_read, 0);

	/*Init data write WQ*/
	my_device->data_wq_write = alloc_workqueue("plat_dummy_write",
					WQ_UNBOUND, MAX_DUMMY_PLAT_THREADS);

	if (!my_device->data_wq_write)
		return -ENOMEM;

	INIT_DELAYED_WORK(&my_device->dwork_write, plat_dummy_work_write);
	my_device->js_pool_time = msecs_to_jiffies(DEVICE_POOLING_TIME_MS);
	queue_delayed_work(my_device->data_wq_write, &my_device->dwork_write, 0);

	return PTR_ERR_OR_ZERO(my_device->read_mem);
}

static int plat_dummy_remove(struct platform_device *pdev)
{
	struct plat_dummy_device *my_device = platform_get_drvdata(pdev);

	pr_info("++%s\n", __func__);

	if (my_device->data_wq_read) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork_read);
		destroy_workqueue(my_device->data_wq_read);
	}

	if (my_device->data_wq_write) {
	/* Destroy work Queue */
		cancel_delayed_work_sync(&my_device->dwork_write);
		destroy_workqueue(my_device->data_wq_write);
	}

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

MODULE_AUTHOR("Vitaliy Vasylskyy <vitaliy.vasylskyy@globallogic.com>");
MODULE_DESCRIPTION("Dummy platform driver");
MODULE_LICENSE("GPL");
