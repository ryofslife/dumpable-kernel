#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/printk.h>

static int __init dump_init(void) 
{ 
    printk("dump_init: Hello world\n"); 
    return 0; 
} 

static void __exit dump_exit(void) 
{ 
    printk("dump_exit: Hello world\n"); 
} 

module_init(dump_init); 
module_exit(dump_exit); 

MODULE_AUTHOR("RYO");
MODULE_DESCRIPTION("The plan is to trigger dump using this module.");
MODULE_LICENSE("GPL");