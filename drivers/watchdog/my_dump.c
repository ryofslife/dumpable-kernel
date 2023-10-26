#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/printk.h>
#include <linux/gpio.h>

#define GPIO_PIN_LED 16

static int __init dump_init(void) 
{ 

    printk("dump_init: Hello world\n");

    // LEDを点灯する 
    gpio_direction_output(GPIO_PIN_LED, 1);

    return 0; 
} 

static void __exit dump_exit(void) 
{ 

    printk("dump_exit: Bye\n"); 

    // LEDを消灯する
    gpio_set_value(GPIO_PIN_LED, 0);

} 

module_init(dump_init); 
module_exit(dump_exit); 

MODULE_AUTHOR("RYO");
MODULE_DESCRIPTION("The plan is to trigger dump using this module.");
MODULE_LICENSE("GPL");