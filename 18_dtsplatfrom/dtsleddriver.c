#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define THISDRIVER_CNT	1
#define THISDRIVER_NAME	"pf_dts_led"
#define LEDOFF	0	
#define LEDON	1

/* 字符设备结构体 */
struct ThisDriver_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *node;	/* LED设备的设备节点 */
	int gpio_led;		/* LED设备使用的gpio 申请的编号 */
};

struct ThisDriver_dev ThisDriver;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	if(sta == LEDON){
		gpio_set_value(ThisDriver.gpio_led,0);
	}else if(sta == LEDOFF){
		gpio_set_value(ThisDriver.gpio_led,1);
	}
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ThisDriver_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ThisDriver;
	return 0;
}

static ssize_t ThisDriver_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	return 0;
}

static ssize_t ThisDriver_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int retvalue = 0;
	unsigned char databuf[1];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
	}

	ledstat = databuf[0];
	if(ledstat == LEDON){
		led_switch(LEDON);
	}else if(ledstat == LEDOFF){
		led_switch(LEDOFF);
	}
	return 0;
}

static int ThisDriver_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations ThisDriver_fops={
	.owner = THIS_MODULE,
	.open = ThisDriver_open,
	.release = ThisDriver_release,
	.read = ThisDriver_read,
	.write = ThisDriver_write,
};

/* 驱动与设备匹配成功以后此函数就会执行 */
static int pf_ThisDriver_probe(struct platform_device *pf_dev)
{
	printk("led driver and device has matched!\r\n");
	/* 初始化LED gpio */
	/* 2.使能GPIO1时钟 */
	/* 3.设置引脚复位功能,设置引脚IO属性 */
	/* 4.设置引脚输出方向 */
	/* 5.默认开LED */
	ThisDriver.node = of_find_node_by_path("/gpioled");
	ThisDriver.gpio_led = of_get_named_gpio(ThisDriver.node, "led-gpio", 0);
	gpio_request(ThisDriver.gpio_led, THISDRIVER_NAME);
	gpio_direction_output(ThisDriver.gpio_led, 0);/* 设置为输出,默认高电平*/

	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(ThisDriver.major){
		ThisDriver.devid = MKDEV(ThisDriver.major, 0);
		register_chrdev_region(ThisDriver.devid, THISDRIVER_CNT, THISDRIVER_NAME);
	}else{
		alloc_chrdev_region(&ThisDriver.devid, 0, THISDRIVER_CNT, THISDRIVER_NAME);
		ThisDriver.major = MAJOR(ThisDriver.devid);
		ThisDriver.minor = MINOR(ThisDriver.devid);
	}

	printk("ThisDriver major = %d, minor = %d\r\n", ThisDriver.major, ThisDriver.minor);
	
	/* 2.初始化cdev */
	ThisDriver.cdev.owner = THIS_MODULE;
	cdev_init(&ThisDriver.cdev, &ThisDriver_fops);

	/* 3.添加cdev */
	cdev_add(&ThisDriver.cdev, ThisDriver.devid, THISDRIVER_CNT);

	/* 4.创造类 */
	ThisDriver.class = class_create(THIS_MODULE, THISDRIVER_NAME);
	if (IS_ERR(ThisDriver.class)){
		return PTR_ERR(ThisDriver.class);
	}

	/* 5.创建设备 */
	ThisDriver.device = device_create(ThisDriver.class, NULL, ThisDriver.devid, NULL, THISDRIVER_NAME);
	if (IS_ERR(ThisDriver.device)){
		return PTR_ERR(ThisDriver.device);
	}
	return 0;
}

static int pf_ThisDriver_remove(struct platform_device *pf_dev)
{
	/* 退出关闭LED */
	gpio_set_value(ThisDriver.gpio_led, 1);
	gpio_free(ThisDriver.gpio_led);

	/* 注销字符设备驱动 */
	cdev_del(&ThisDriver.cdev);
	unregister_chrdev_region(ThisDriver.devid, THISDRIVER_CNT);
	device_destroy(ThisDriver.class, ThisDriver.devid);
	class_destroy(ThisDriver.class);	
	return 0;
}

static const struct of_device_id pf_ThisDriver_of_match[] = {
	{ .compatible = "atkalpha-gpioled" },
	{	}
};

static struct platform_driver pf_ThisDriver = {
	.driver		= {
		.name	= "imx6ull-led",	/* 驱动名字，用于和platfrom设备匹配 */
		.of_match_table = pf_ThisDriver_of_match,/* 匹配表，用于和设备树设备匹配 */
	},
	.probe		= pf_ThisDriver_probe,	/* 匹配成功以后此函数就会执行 */
	.remove		= pf_ThisDriver_remove,
};

static int __init ThisDriver_init(void)
{	
	return platform_driver_register(&pf_ThisDriver);
}

static void __exit ThisDriver_exit(void)
{
	platform_driver_unregister(&pf_ThisDriver);
	printk("ThisDriver exit!\n");
}

module_init(ThisDriver_init);
module_exit(ThisDriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
