#include <linux/init.h> 
#include <linux/module.h> 
#include <linux/printk.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define GPIO_PIN_LED 16
#define GPIO_PIN_HIGH 1
#define GPIO_PIN_BTN 14

int irq;

static irqreturn_t dump_intr(int irq, void *dev_id)
{

    printk("dump_intr: button was pressed!\n");

    return IRQ_HANDLED;
}

static int __init dump_init(void) 
{ 

    printk("dump_init: Hello world\n");

    // LEDを点灯する 
    gpio_direction_output(GPIO_PIN_LED, 1);

    if (gpio_request(GPIO_PIN_BTN,"GPIO_PIN_BTN") < 0) {
        printk("dump_init: request failed for gpio %d \n", GPIO_PIN_BTN);
        return -1;
    }

    // GPIO10を入力にする
    gpio_direction_input(GPIO_PIN_BTN);

    // 割り込み番号を取得する
    irq = gpio_to_irq(GPIO_PIN_BTN);
    printk("dump_init: given irq was %d\n", irq);

    if (request_irq(irq, (void*)dump_intr, IRQF_TRIGGER_RISING, "dump_intr", NULL) < 0) {
        printk("dump_init: error registering interrupt handler\n");
        return -1;
    }

    printk("dump_init: all set\n");

    return 0; 
} 

static void __exit dump_exit(void) 
{ 

    printk("dump_exit: Bye\n"); 

    // LEDを消灯する
    gpio_set_value(GPIO_PIN_LED, 0);

    // 登録していた割り込みを解放する
    free_irq(irq,NULL);

} 

module_init(dump_init); 
module_exit(dump_exit); 

MODULE_AUTHOR("RYO");
MODULE_DESCRIPTION("The plan is to trigger dump using this module.");
MODULE_LICENSE("GPL");