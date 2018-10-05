#include <linux/init.h>
#include <linux/module.h>
#include <linux/init.h>
MODULE_LICENSE("Dual BSD/GPL");

static int hello3_data __initdata = 3;

static int __init hello_init(void)	
{
	printk(KERN_INFO "Hello, world %d\n", hello3_data);
	printk(KERN_INFO "CHECK SUM : %d\n", hello3_data+10);
	return 0;
}
static void __exit hello_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world\n");
}
module_init(hello_init);
module_exit(hello_exit);
