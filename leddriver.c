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

#define LEDDEV_CNT      1
#define LEDDEV_NAME     "dtsplatled"

#define LED_ON          1
#define LED_OFF         0

static int led_open(struct inode *nd, struct file *file);
static ssize_t led_write(struct file *file,
                         const char __user *user,
                         size_t size,
                         loff_t *loff);
static int led_release(struct inode *nd, struct file *file);
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
struct gpioled_dev {
        dev_t devid;
        int major;
        int minor;
        struct cdev cdev;
        struct class *class;
        struct device *device;
        struct device_node *nd;
        int gpio_led;
};
struct gpioled_dev leddev;

static const struct file_operations led_ops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .write = led_write,
        .release = led_release,
};

static void led_switch(u8 led_status)
{
        if (led_status == LED_ON)
                gpio_set_value(leddev.gpio_led, 0);
        else if(led_status == LED_OFF)
                gpio_set_value(leddev.gpio_led, 1);
}

static int led_open(struct inode *nd, struct file *file)
{
        file->private_data = &leddev;

        return 0;
}

static ssize_t led_write(struct file *file,
                         const char __user *user,
                         size_t size,
                         loff_t *loff)
{
        int ret = 0;
        unsigned char buf[1];
        struct gpioled_dev *dev = file->private_data;

        ret = copy_from_user(buf, user, 1);
        if (ret < 0) {
                printk("kernel write error!\n");
                ret = -EFAULT;
                goto error;
        }
        if((buf[0] != LED_OFF) && (buf[0] != LED_ON)) {
                ret = -EINVAL;
                goto error;
        }

        led_switch(buf[0]);

error:
        return 0;
}

static int led_release(struct inode *nd, struct file *file)
{
        file->private_data = NULL;
        return 0;
}

static int led_probe(struct platform_device *pdev)
{
        int ret = 0;
        printk("led probe!\n");
 
        /* 1.注册设备号 */
        if (leddev.major) {
                leddev.devid = MKDEV(leddev.major, leddev.minor);
                ret = register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);
        } else {
                ret = alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
                leddev.major = MAJOR(leddev.devid);
                leddev.minor = MINOR(leddev.devid);
        }
        if (ret < 0) {
                printk("chrdeev region error!\n");
                goto fail_region; 
        }
        printk("major: %d, minor: %d\n", leddev.major, leddev.minor);

        /* 2.注册字符设备 */
        leddev.cdev.owner = THIS_MODULE;
        cdev_init(&leddev.cdev, &led_ops);
        ret = cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);
        if (ret != 0) {
                printk("cdev_add error!\n");
                goto fail_cdevadd;
        }
        
        /* 3.创建类和设备 */
        leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
        if (IS_ERR(leddev.class)) {
                ret = PTR_ERR(leddev.class);
                goto fail_class;
        }
        leddev.device = device_create(leddev.class, NULL, leddev.devid,
                                      NULL, LEDDEV_NAME);
        if (IS_ERR(leddev.device)) {
                ret = PTR_ERR(leddev.device);
                goto fail_device;
        }

#if 0
        leddev.nd = of_find_node_by_path("/gpioled");
#endif
        leddev.nd = pdev->dev.of_node;
        if (leddev.nd == NULL) {
                printk("of_find_node_by_path error!\n");
                ret = -EINVAL;
                goto fail_findnode;
        }

        leddev.gpio_led = of_get_named_gpio(leddev.nd, "led-gpios", 0);
        if (leddev.gpio_led < 0) {
                printk("of_get_named_gpio error!\n");
                ret = -EINVAL;
                goto fail_getnamed;
        }
        ret = gpio_request(leddev.gpio_led, "led-gpios");
        if (ret != 0) {
                printk("gpio_request error!\n");
                goto fail_request;
        }
        ret = gpio_direction_output(leddev.gpio_led, 1);
        if (ret != 0) {
                printk("gpio direction output error!\n");
                goto fail_dir;
        }
        gpio_set_value(leddev.gpio_led, 0);     /* 打开led */

        goto success;

fail_dir:
        gpio_free(leddev.gpio_led);
fail_request:
fail_getnamed:
fail_findnode:
        device_destroy(leddev.class, leddev.devid);
fail_device:
        class_destroy(leddev.class);
fail_class:
        cdev_del(&leddev.cdev);
fail_cdevadd:
        unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
fail_region:
success:
        return ret;
}

static int led_remove(struct platform_device *pdev)
{
        printk("led remove!\n");
        gpio_set_value(leddev.gpio_led, 1);
        gpio_free(leddev.gpio_led);
        device_destroy(leddev.class, leddev.devid);
        class_destroy(leddev.class);
        cdev_del(&leddev.cdev);
        unregister_chrdev_region(leddev.devid, LEDDEV_CNT);

        return 0;
}


static int __init leddevice_init(void)
{
        int ret = 0;
        ret = platform_driver_register(&led_driver);
        if (ret) {
                printk("failed to register led device!\n");
        }
        return ret;
}

static void __exit leddevice_exit(void)
{
        platform_driver_unregister(&led_driver);
}

module_init(leddevice_init);
module_exit(leddevice_exit);
MODULE_AUTHOR("wanglei");
MODULE_LICENSE("GPL");