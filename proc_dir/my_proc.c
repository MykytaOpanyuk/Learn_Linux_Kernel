#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>


#define procfs_name "helloworld"

int len,temp;
char *msg;

ssize_t procfile_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
	if(count > temp) {
		count = temp;
	}
	temp = temp - count;
	copy_to_user(buf, msg, count);
	if(count == 0)
		temp = len;
	return count;
}

struct file_operations proc_fops = {
	read: procfile_read
};

static int __init init_proc(void)
{
	proc_create("hello_write", 0, NULL, &proc_fops);
	msg=kmalloc(10 * sizeof(char), GFP_KERNEL);

	printk(KERN_INFO "/proc/%s created\n", procfs_name);
	return 0;
}

static void __exit cleanup_proc(void)
{
	remove_proc_entry(procfs_name, NULL);
	printk(KERN_INFO "/proc/%s removed\n", procfs_name);
}

MODULE_LICENSE("GPL");
module_init(init_proc);
module_exit(cleanup_proc);
