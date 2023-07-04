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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>

#define DEVICE_CNT		1
#define DEVICE_NAME	"DeviceName"
#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define READ_CMD		_IOR(0xEF, 0X4, int)
#define LEDOFF	0	
#define LEDON	1

/* chardev设备结构体 */
struct chardev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;				/* 类 */
	struct device *device; 				/* 设备 */
	int major;							/* 主设备号 */
	int minor;							/* 次设备号 */
	struct device_node *nd;				/* 设备节点 */
	int dev_gpio_id;					/* 设备所使用的GPIO编号*/
	int timerperiod;					/* 定时周期，单位为ms */
	struct timer_list timer;			/* 定义一个定时器 */
	spinlock_t lock;					/* 定义自旋锁 */
};

struct chardev DeviceName;


static int led_init(void)
{
	int ret = 0;
	const char *str;
	struct property *proper;

	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：gpioled */
	DeviceName.nd = of_find_node_by_path("/gpioled");
	if(DeviceName.nd == NULL){
		printk("gpioled node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("gpioled node has been found!\r\n");
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(DeviceName.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(DeviceName.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}
	/* 3、获取分配的gpio id号 */
	DeviceName.dev_gpio_id = of_get_named_gpio(DeviceName.nd, "led-gpio", 0);
	if(DeviceName.dev_gpio_id < 0){
		printk("can not get dev_gpio_id!\r\n");
	} else {
		printk("dev_gpio_id num = %d\r\n", DeviceName.dev_gpio_id);
	}

	/* 4、初始化led所使用的IO */
	gpio_request(DeviceName.dev_gpio_id, DEVICE_NAME);
	ret = gpio_direction_output(DeviceName.dev_gpio_id, 1);
	if(ret < 0){
		printk("Set IO Failed!\r\n");
	} else {
		printk("Set IO Successed!\r\n");
	}

	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	filp->private_data = &DeviceName;

	DeviceName.timerperiod = 1000;
	ret = led_init();
	if(ret < 0){
		return ret;
	}
	return 0;
}

/*
* @description : ioctl 函数，
* @param – filp : 要打开的设备文件(文件描述符)
* @param - cmd : 应用程序发送过来的命令
* @param - arg : 参数
* @return : 0 成功;其他 失败
*/
static long timer_unlocked_ioctl(struct file *filp,unsigned int cmd,
			unsigned long arg)
{
	struct chardev *dev = (struct chardev *)filp->private_data;
	int timerperiod;
	unsigned long flags;
	printk("driver cmd:%d arg:%ld\r\n", cmd, arg);
	switch (cmd)
	{
	case CLOSE_CMD:		/* 关闭定时器 */
		gpio_set_value(dev->dev_gpio_id, 1);
		del_timer_sync(&dev->timer);
		break;
	case OPEN_CMD:		/* 打开定时器 */
		spin_lock_irqsave(&dev->lock, flags);
		timerperiod = dev->timerperiod;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
		break;
	case SETPERIOD_CMD:
		spin_lock_irqsave(&dev->lock, flags);
		dev->timerperiod = arg;
		spin_unlock_irqrestore(&dev->lock, flags);
		mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
		break;
	default:
		break;
	}
	return 0;
}

static ssize_t timer_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	return 0;
}

static ssize_t timer_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	return 0;
}

static int timer_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations DeviceName_fops={
	.owner = THIS_MODULE,
	.open = timer_open,
	.release = timer_release,
	.read = timer_read,
	.write = timer_write,
	.unlocked_ioctl = timer_unlocked_ioctl,
};

void timer_function(unsigned long arg)
{
	struct chardev *dev = (struct chardev *)arg;
	static int sta = 1;
	int timerperiod;
	unsigned long flags;

	sta =! sta;
	gpio_set_value(dev->dev_gpio_id, sta);

	/* 重启定时器 */
	spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timerperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
}

static int __init DeviceName_init(void)
{	
	/* 初始化自旋锁 */
	spin_lock_init(&DeviceName.lock);

	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(DeviceName.major){
		DeviceName.devid = MKDEV(DeviceName.major, 0);
		register_chrdev_region(DeviceName.devid, DEVICE_CNT, DEVICE_NAME);
	}else{
		alloc_chrdev_region(&DeviceName.devid, 0, DEVICE_CNT, DEVICE_NAME);
		DeviceName.major = MAJOR(DeviceName.devid);
		DeviceName.minor = MINOR(DeviceName.devid);
	}
	printk("DeviceName major = %d, minor = %d\r\n", DeviceName.major, DeviceName.minor);
	
	/* 2.初始化cdev */
	DeviceName.cdev.owner = THIS_MODULE;
	cdev_init(&DeviceName.cdev, &DeviceName_fops);

	/* 3.添加cdev */
	cdev_add(&DeviceName.cdev, DeviceName.devid, DEVICE_CNT);

	/* 4.创造类 */
	DeviceName.class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(DeviceName.class)){
		return PTR_ERR(DeviceName.class);
	}

	/* 5.创建设备 */
	DeviceName.device = device_create(DeviceName.class, NULL, DeviceName.devid, NULL, DEVICE_NAME);
	if (IS_ERR(DeviceName.device)){
		return PTR_ERR(DeviceName.device);
	}

	/* 6.初始化timer,设置定时器处理函数，还未设置周期，所以不会激活定时器 */
	init_timer(&DeviceName.timer);
	DeviceName.timer.function = timer_function;
	DeviceName.timer.data = (unsigned long)&DeviceName;

	return 0;
}

static void __exit DeviceName_exit(void)
{
	gpio_set_value(DeviceName.dev_gpio_id, 1);
	del_timer_sync(&DeviceName.timer);

	/* 注销字符设备驱动 */
	cdev_del(&DeviceName.cdev);
	unregister_chrdev_region(DeviceName.devid, DEVICE_CNT);
	device_destroy(DeviceName.class, DeviceName.devid);
	class_destroy(DeviceName.class);

	printk("DeviceName exit!\n");
}

module_init(DeviceName_init);
module_exit(DeviceName_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
