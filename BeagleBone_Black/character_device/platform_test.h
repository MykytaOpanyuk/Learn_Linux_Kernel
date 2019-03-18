#ifndef _PLATFORM_TEST_H_
#define _PLATFORM_TEST_H_

#include <linux/mm.h>

#define DUMMY_IO_BUFF_SIZE (4*1024)
struct plat_dummy_device {
	struct platform_device	*pdev;

	void __iomem		*mem_read;
	dma_addr_t		phys_mem_addr_read;
	size_t			phys_mem_size_read;
	void __iomem		*regs_read;

	void __iomem		*mem_write;
	dma_addr_t		phys_mem_addr_write;
	size_t			phys_mem_size_write;
	void __iomem		*regs_write;

	struct delayed_work	dwork_read;
	struct delayed_work	dwork_write;
	struct workqueue_struct	*data_read_wq;
	struct workqueue_struct	*data_write_wq;
	u64			js_pool_time;
	spinlock_t		pool_lock;

	wait_queue_head_t	rwq; /* read queues */
	wait_queue_head_t	wwq; /* write queues */
	struct mutex		rd_mutex;
	/* mutex to synchronized reading from userspace */
	struct mutex		wr_mutex;
	char			buffer_read[DUMMY_IO_BUFF_SIZE];
	/* begin of buf, end of buf */
	char			*end_read;
	/* used in pointer arithmetic */
	u32			buffersize_read;
	/* where to read, where to write (pointers) in read region */
	char			*rp_read, *wp_read;
	char			buffer_write[DUMMY_IO_BUFF_SIZE];
	/* begin of buf, end of buf */
	char			*end_write;
	/* used in pointer arithmetic */
	u32			buffersize_write;
	/* where to read, where to write (pointers) in write reg */
	char			*rp_write, *wp_write;

	ssize_t (*dummy_read) (
			struct plat_dummy_device *my_device,
			char __user *buf, size_t count);
	ssize_t (*dummy_write) (
			struct plat_dummy_device *my_device,
			const char __user *buf, size_t count);
	int (*set_poll_interval) (
			struct plat_dummy_device *my_device,
			u32 ms_interval);
	int (*dummy_mmap_read) (
			struct plat_dummy_device *my_device,
			struct vm_area_struct *vma);
	int (*dummy_mmap_write) (
			struct plat_dummy_device *my_device,
			struct vm_area_struct *vma);
};

enum dummy_dev {
	DUMMY_DEV_1,
	DUMMY_DEV_2,
	DUMMY_DEVICES
};

struct plat_dummy_device *get_dummy_platfrom_device(enum dummy_dev devnum);

#endif
