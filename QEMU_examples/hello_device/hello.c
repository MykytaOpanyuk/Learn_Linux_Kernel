#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/param.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Jay Salzman");

#define MY_MAJOR 200
#define MY_MINOR 0
#define MY_DEV_COUNT 1

static int func_open( struct inode *, struct file * );
static ssize_t func_read( struct file *, char *  , size_t, loff_t *);
static ssize_t func_write(struct file *, const  char *, size_t, loff_t *);
static int func_close(struct inode *, struct file * );
static void my_timer_callback( struct timer_list *unused );

struct file_operations f_fops = {
	read    :       func_read,
	write   :       func_write,
	open    :       func_open,
	release :       func_close
};

struct cdev *f_cdev;
static char *msg = NULL;
static unsigned long onesec;
//static struct timer_list my_timer;

static DEFINE_TIMER(my_timer, my_timer_callback);

static void my_timer_callback( struct timer_list *unused )
{
	printk( "my_timer_callback called (%ld).\n", jiffies );

	mod_timer(&my_timer, jiffies + onesec);
}

static int __init hello_init(void)	
{
	dev_t devno;
	int err;

	devno = MKDEV(MY_MAJOR, MY_MINOR);
	register_chrdev_region(devno, MY_DEV_COUNT, "mykdev");

	f_cdev = cdev_alloc();

	cdev_init(f_cdev, &f_fops);

	err = cdev_add(f_cdev, devno, MY_DEV_COUNT);

	if (err < 0) {
		printk("Device Add Error!\n");
		return -1;
	}

	printk("Hello World.\n");
	printk("'mknod /dev/mykdev c %d 0'.\n", MY_MAJOR);

	msg = (char *)kmalloc(32, GFP_KERNEL);
	if (msg != NULL)
		printk("malloc allocator address: 0x%p\n", msg);

return 0;
}

static void __exit hello_exit(void)
{
	dev_t devno;

	if (msg)
		kfree(msg);

	devno = MKDEV(MY_MAJOR, MY_MINOR);
	unregister_chrdev_region(devno, MY_DEV_COUNT);
	cdev_del(f_cdev);
	del_timer(&my_timer);

	printk(KERN_ALERT "Goodbye, cruel world\n");
}

static int func_open(struct inode *inod, struct file *fil)
{
	int major;
	int minor;
	major = imajor(inod);
	minor = iminor(inod);
	printk("\n***** Opened device at major %d  minor %d *****\n",major, minor);
	return 0;
}

static ssize_t func_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
	int major, minor, ret;
	short count;

	printk("Timer module installing\n");

	onesec = msecs_to_jiffies(1000 * 1);

	printk( "Starting timer to fire in 200ms (%ld)\n", jiffies );
	ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(200) );
	if (ret)
		printk("Error in mod_timer\n");

	major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);

	printk("FILE OPERATION READ: %d : %d\n", major, minor);

	switch(minor) {
	        case 0:
		        strcpy(msg, "DATA FROM MOUDLE: minor : 0");
		        break;
	        default:
		        len = 0;
	}
	count = copy_to_user(buff, msg, len);
	printk("msg: %s", msg);

	return 0;
}

static ssize_t func_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	int major, minor;
	short count;
	unsigned long later = jiffies + 2*HZ;

	memset(msg, 0, 32);
	major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);

	count = copy_from_user( msg, buff, len );

	printk("FILE OPERATION WRITE: %d : %d\n", major, minor);
	printk("msg: %s", msg);

	while(time_before(jiffies, later)) {
		cpu_relax();
	}

	printk("Sleep for : %d", later);

	return len;
}

static int func_close(struct inode *inod, struct file *fil)
{
	int minor = MINOR(fil->f_path.dentry->d_inode->i_rdev), ret;

	ret = del_timer( &my_timer );
	if (ret)
		printk("The timer is still in use...\n");

	printk("Timer module uninstalling\n");

	printk("***** Closed device at minor %d *****\n", minor);
	return 0;

	kfree(f_cdev);
}

module_init(hello_init);
module_exit(hello_exit);
