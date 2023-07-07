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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define THISDEVICE_CNT		1
#define THISDEVICE_MINOR	144
#define THISDEVICE_NAME	"ThisDevice_beep"	/* 设备注册名字 */
#define DT_ND_PATH	"/beep"					/* 设备树设备节点路径 */
#define DT_COMPATIBLE	"atkalpha-beep"		/* 匹配用属性名字 */
#define DT_GPIO			"beep-gpio"			/* gpio属性名字 */
#define BEEPOFF	0	
#define BEEPON	1


/* 字符设备结构体 */
struct ThisDevice{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *nd;
	int gpio;			/* ThisDevice所使用的GPIO编号 */
};

struct ThisDevice ThisDevice;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int ThisDevice_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ThisDevice;
	return 0;
}

static ssize_t ThisDevice_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	return 0;
}

static ssize_t ThisDevice_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int retvalue = 0;
	unsigned char databuf[1];
	unsigned char ThisDevicestat;
	struct ThisDevice *dev= filp->private_data;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ThisDevicestat = databuf[0];	/* 获取状态值 */
	if(ThisDevicestat == BEEPON){
		gpio_set_value(dev->gpio, 0);
	}else if(ThisDevicestat == BEEPOFF){
		gpio_set_value(dev->gpio, 1);
	}
	return 0;
}

static int ThisDevice_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations ThisDevice_fops={
	.owner = THIS_MODULE,
	.open = ThisDevice_open,
	.release = ThisDevice_release,
	.read = ThisDevice_read,
	.write = ThisDevice_write,
};

/* 第三层结构-ligh */
static struct miscdevice misc_ThisDevice = {
	.minor 	= THISDEVICE_MINOR,
	.name 	= THISDEVICE_NAME,
	.fops	= &ThisDevice_fops,
};

static const struct of_device_id pf_ThisDevice_of_match[] = {
	{ .compatible = DT_COMPATIBLE },
	{	}
};

static int pf_ThisDevice_probe(struct platform_device *pf_ThisDevice)
{
	int ret = 0;
	struct property *proper;
	const char *str;

	printk("This driver and device was matched!\r\n");
	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：alphaThisDevice */
	ThisDevice.nd = of_find_node_by_path(DT_ND_PATH);
	if(ThisDevice.nd == NULL){
		printk("%s node can not found!\r\n", DT_ND_PATH);
		return -EINVAL;
	} else {
		printk("%s node has been found!\r\n",DT_ND_PATH);
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(ThisDevice.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(ThisDevice.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}

	ThisDevice.gpio = of_get_named_gpio(ThisDevice.nd, DT_GPIO, 0);
	if(ThisDevice.gpio < 0){
		printk("can not get %s!\r\n", DT_GPIO);
	} else {
		printk("%s num = %d\r\n",DT_GPIO ,ThisDevice.gpio);
	}

	/* 设置引脚输出方向,默认置1*/
	ret = gpio_direction_output(ThisDevice.gpio, 1);
	if(ret < 0){
		printk("can set gpio_direction!\r\n");
	} else {
		printk("set gpio_direction!\r\n");
	}

	/* 一般情况下会注册对应的字符设备，但是这里我们使用 MISC 设备
	* 所以我们不需要自己注册字符设备驱动，只需要注册 misc 设备驱动即可
	*/
	ret = misc_register(&misc_ThisDevice);
	return 0;
}

static int pf_ThisDevice_remove(struct platform_device *pf_ThisDevice)
{
	gpio_set_value(ThisDevice.gpio, 1);
	gpio_free(ThisDevice.gpio);

	misc_deregister(&misc_ThisDevice);
	return 0;
}
/* 第二层结构-ligh */
static struct platform_driver pf_ThisDevice = {
	.driver		={
		.name 	= "imx6ul-beep",
		.of_match_table = pf_ThisDevice_of_match,
	},
	.probe		= pf_ThisDevice_probe,
	.remove		= pf_ThisDevice_remove,
};

/* 第一层结构-ligh */
static int __init ThisDevice_init(void)
{	
	return platform_driver_register(&pf_ThisDevice);
}

static void __exit ThisDevice_exit(void)
{
	platform_driver_unregister(&pf_ThisDevice);
}

module_init(ThisDevice_init);
module_exit(ThisDevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
