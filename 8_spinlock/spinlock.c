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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>

#define gpioled_CNT		1
#define gpioled_NAME	"gpioled"
#define LEDOFF	0	
#define LEDON	1

/* 寄存器物理地址 
#define CCM_CCGR1_BASE			(0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE	(0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE	(0X020E02F4)
#define GPIO1_DR_BASE			(0X0209C000)
#define GPIO1_GDIR_BASE			(0X0209C004)*/

static void __iomem *IMX6U_CCM_CCGR1;	
static void __iomem *SW_MUX_GPIO1_IO03;	
static void __iomem *SW_PAD_GPIO1_IO03;	
static void __iomem *GPIO1_DR;	
static void __iomem *GPIO1_GDIR;	

/* nwechrled设备结构体 */
struct gpioled_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *nd;
	int led_gpio;		/* led所使用的GPIO编号*/
	int dev_stats;			/* led所使用的GPIO编号 */
	spinlock_t lock;
};

struct gpioled_dev gpioled;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	unsigned long flags;	/*用来保存中断状态*/
	filp->private_data = &gpioled;

	spin_lock_irqsave(&gpioled.lock, flags); /* spin上锁 */
	if(gpioled.dev_stats){
		spin_unlock_irqrestore(&gpioled.lock, flags);	/* spin解锁 */
		return -EBUSY;
	}
	gpioled.dev_stats++;/*如果设备没有打开，就自加1，表示现在打开*/
	spin_unlock_irqrestore(&gpioled.lock, flags);	/* spin解锁 */
	return 0;
}

static ssize_t led_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int retvalue = 0;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct gpioled_dev *dev= filp->private_data;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];	/* 获取状态值 */
	if(ledstat == LEDON){
		gpio_set_value(dev->led_gpio, 0);
	}else if(ledstat == LEDOFF){
		gpio_set_value(dev->led_gpio, 1);
	}
	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
	unsigned long flags;	/*用来保存中断状态*/
	struct gpioled_dev *dev = filp->private_data;
	spin_lock_irqsave(&gpioled.lock, flags); /* spin上锁 */
	if(gpioled.dev_stats){
		gpioled.dev_stats--;/*关闭驱动的时候，清除使用标记*/
	}

	spin_unlock_irqrestore(&gpioled.lock, flags);	/* spin解锁 */
	return 0;
}

static const struct file_operations gpioled_fops={
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.read = led_read,
	.write = led_write,
};

static int __init led_init(void)
{	
	u32 val = 0;
	int ret = 0;
	const char *str;
	struct property *proper;

	/* 初始化自旋锁 */
	spin_lock_init(&gpioled.lock);
	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：alphaled */
	gpioled.nd = of_find_node_by_path("/gpioled");
	if(gpioled.nd == NULL){
		printk("gpioled node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("gpioled node has been found!\r\n");
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(gpioled.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(gpioled.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}

	gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
	if(gpioled.led_gpio < 0){
		printk("can not get led_gpio!\r\n");
	} else {
		printk("led_gpio num = %d\r\n", gpioled.led_gpio);
	}
	/* 初始化LED 1.寄存器映射  */

	/* 2.使能GPIO1时钟 */

	/* 3.设置引脚复位功能,设置引脚IO属性 */

	/* 4.设置引脚输出方向 */
	ret = gpio_direction_output(gpioled.led_gpio, 1);
	if(ret < 0){
		printk("can set gpio_direction!\r\n");
	} else {
		printk("set gpio_direction!\r\n");
	}
	/* 5.默认开LED */


	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(gpioled.major){
		gpioled.devid = MKDEV(gpioled.major, 0);
		register_chrdev_region(gpioled.devid, gpioled_CNT, gpioled_NAME);
	}else{
		alloc_chrdev_region(&gpioled.devid, 0, gpioled_CNT, gpioled_NAME);
		gpioled.major = MAJOR(gpioled.devid);
		gpioled.minor = MINOR(gpioled.devid);
	}

	printk("gpioled major = %d, minor = %d\r\n", gpioled.major, gpioled.minor);
	
	/* 2.初始化cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);

	/* 3.添加cdev */
	cdev_add(&gpioled.cdev, gpioled.devid, gpioled_CNT);

	/* 4.创造类 */
	gpioled.class = class_create(THIS_MODULE, gpioled_NAME);
	if (IS_ERR(gpioled.class)){
		return PTR_ERR(gpioled.class);
	}

	/* 5.创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, gpioled_NAME);
	if (IS_ERR(gpioled.device)){
		return PTR_ERR(gpioled.device);
	}

	return 0;
}

static void __exit led_exit(void)
{

	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);
	unregister_chrdev_region(gpioled.devid, gpioled_CNT);
	device_destroy(gpioled.class, gpioled.devid);
	class_destroy(gpioled.class);

	printk("gpioled exit!\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
