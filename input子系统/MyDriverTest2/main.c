#include <linux/module.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("innoink");
MODULE_DESCRIPTION("hello world module");

static int __init khello_init(void)
{
     printk(KERN_ERR "hello world!");
     return 0;
}

static void __exit khello_exit(void)
{
     printk(KERN_EMERG "hello exit!");
}

module_init(khello_init)
module_exit(khello_exit)
