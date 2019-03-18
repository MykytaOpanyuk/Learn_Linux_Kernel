#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrii Tymkiv, Mykyta Opaniuk");
MODULE_DESCRIPTION("Say hello and goodbye");
MODULE_VERSION("0.23");

static char *name = "user";
static int a = 0;
static int b = 0;

/* get params to module :
-> insmod hello_module_arithmetic.ko name=\"Mykyta\" a=5 b=6
*/

module_param(name, charp, S_IRUGO); //just can be read
MODULE_PARM_DESC(name, "The name to display");  ///< parameter description

module_param(a, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(a, "First number");  ///< parameter description

module_param(b, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(b, "Second number");  ///< parameter description

static int __init hello_init(void){
   	static int c = 0;
   	
	c = a + b;
	printk(KERN_INFO "Hello, %s!\n", name);
   	printk(KERN_INFO "Sum, a + b = %d!\n", c);

   	c = c * a * b;
   	printk(KERN_INFO "Mult, c * a * b = %d!\n", c);

   	return 0;
}

static void __exit hello_exit(void){
   	printk(KERN_INFO "Goodbye, %s !\n", name);
}

module_init(hello_init);
module_exit(hello_exit);
