#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

static int led_probe(struct platform_device *pdev);
static int led_remove(struct platform_device *pdev);

static const struct of_device_id led_of_match[] = {
        { .compatible = "alientek, gpioled" },
        { /* Sentinel */ },
};

static struct platform_driver led_driver = {
        .driver = {
                .name = "imx6ull-led",          /* 无设备树时 进行设备匹配，驱动名字 */
                .of_match_table = led_of_match, /* 有设备树时 进行设备匹配 */
        },
        .probe = led_probe,
        .remove = led_remove,
};

static int led_probe(struct platform_device *pdev)
{
        printk("led probe!\n");
        return 0;
}

static int led_remove(struct platform_device *pdev)
{
        printk("led remove!\n");
        return 0;
}

static int __init leddevice_init(void)
{
        int ret = 0;
        ret = platform_driver_register(&led_driver);
        if (ret) {
                printk("failed to register led device!\n");
        }
        return 0;
}

static void __exit leddevice_exit(void)
{
       platform_driver_unregister(&led_driver); 
}

module_init(leddevice_init);
module_exit(leddevice_exit);
MODULE_AUTHOR("wanglei");
MODULE_LICENSE("GPL");