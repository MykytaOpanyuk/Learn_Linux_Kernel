#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#define MY_MAJOR 200
#define MY_MINOR 0
#define MY_DEV_COUNT 1

static char *msg = NULL;
static struct task_struct *task;
static struct mutex etx_mutex;

static struct class *cl;
static struct cdev kernel_cdev; //device

static int open(struct inode *inod, struct file *filp)
{
    int major;
    int minor;
    major = imajor(inod);
    minor = iminor(inod);
    task = current;
    printk("\n***** Opened device at major %d  minor %d *****\n",major, minor);

    printk(KERN_INFO "Inside open. \n");

    return 0;
}

static ssize_t read(struct file *filp, char *buff, size_t count, loff_t *offp)
{
    unsigned long ret;
    int major;
    int minor;

    major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
    minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);

    printk("FILE OPERATION READ: %d : %d\n", major, minor);

    switch(minor) {
    case 0:
        strcpy(msg, "DATA FROM MOUDLE: minor : 0");
        break;
    default:
        ret = 0;
    }

    mutex_lock(&etx_mutex);

    ret = copy_to_user(buff, msg, count);
    printk("msg: %s", msg);
    ssleep(3);

    mutex_unlock(&etx_mutex);

    return ret;
}

static ssize_t write(struct file *filp, const char *buff, size_t count, loff_t *offp)
{
    unsigned long ret;
    int major;
    int minor;

    memset(msg, 0, 32);
    major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
    minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);

    mutex_lock(&etx_mutex);

    printk(KERN_INFO "Inside write. \n");
    ret = copy_from_user( msg, buff, count );
    ssleep(3);

    mutex_unlock(&etx_mutex);

    printk("FILE OPERATION WRITE: %d : %d\n", major, minor);
    printk("msg: %s", msg);

    return ret;
}

static int func_close(struct inode *inod, struct file *filp)
{
    int minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);

    printk(KERN_INFO "Inside close. \n");
    printk("***** Closed device at minor %d *****\n", minor);

    kfree(&kernel_cdev);

    return 0;
}

struct file_operations fops = {
    read    :            read,
    write   :            write,
    open    :            open,
    release :           func_close
};

int char_arr_init (void) {
    int ret;
    dev_t dev_no = MKDEV(MY_MAJOR, MY_MINOR);

    ret = register_chrdev_region(dev_no, MY_DEV_COUNT, "mykdev");

    if (ret < 0) {
        printk("Major number allocation is failed. =================== \n");
        return ret;
    }

    cdev_init(&kernel_cdev, &fops);

    ret = cdev_add(&kernel_cdev, dev_no, 1); //add new device
    if(ret < 0) {
        printk(KERN_INFO "Unable to allocate cdev. \n");
        return ret;
    }
    if (IS_ERR(cl = class_create(THIS_MODULE, "chardrv")))
    {
        cdev_del(&kernel_cdev);
        unregister_chrdev_region(dev_no, MY_DEV_COUNT);
        return PTR_ERR(cl);
    }

    printk("Hello World.\n");
    printk("'mknod /dev/mykdev c %d 0'.\n", MY_MAJOR);

    msg = (char *)kmalloc(32, GFP_KERNEL);

    if (msg != NULL)
        printk("malloc allocator address: 0x%p\n", msg);
    else {
        printk("Cannot get memory for msg!");
        return -1;
    }

    mutex_init(&etx_mutex);

    return 0;
}

void char_arr_cleanup(void) {
    printk(KERN_INFO " Inside cleanup_module. \n");

    dev_t devno;

    if (msg)
        kfree(msg);

    devno = MKDEV(MY_MAJOR, MY_MINOR);
    device_destroy(cl, devno);
    class_destroy(cl);
    cdev_del(&kernel_cdev);
    unregister_chrdev_region(devno, MY_DEV_COUNT);
    mutex_destroy(&etx_mutex);

    printk(KERN_ALERT "Goodbye, cruel world\n");
}

MODULE_LICENSE("GPL");
module_init(char_arr_init);
module_exit(char_arr_cleanup);
