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

#define beep_CNT		1
#define beep_NAME	"beep"
#define BEEPOFF	0	
#define BEEPON	1


/* nwechrbeep设备结构体 */
struct beep_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *nd;
	int beep_gpio;			/* beep所使用的GPIO编号 */
};

struct beep_dev beep;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int beep_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep;
	return 0;
}

static ssize_t beep_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	return 0;
}

static ssize_t beep_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int retvalue = 0;
	unsigned char databuf[1];
	unsigned char beepstat;
	struct beep_dev *dev= filp->private_data;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];	/* 获取状态值 */
	if(beepstat == BEEPON){
		gpio_set_value(dev->beep_gpio, 0);
	}else if(beepstat == BEEPOFF){
		gpio_set_value(dev->beep_gpio, 1);
	}
	return 0;
}

static int beep_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations beep_fops={
	.owner = THIS_MODULE,
	.open = beep_open,
	.release = beep_release,
	.read = beep_read,
	.write = beep_write,
};

static int __init beep_init(void)
{	
	u32 val = 0;
	int ret = 0;
	const char *str;
	struct property *proper;

	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：alphabeep */
	beep.nd = of_find_node_by_path("/beep");
	if(beep.nd == NULL){
		printk("beep node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("beep node has been found!\r\n");
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(beep.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(beep.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}

	beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
	if(beep.beep_gpio < 0){
		printk("can not get beep_gpio!\r\n");
	} else {
		printk("beep_gpio num = %d\r\n", beep.beep_gpio);
	}
	/* 初始化beep 1.寄存器映射  */

	/* 2.使能GPIO1时钟 */

	/* 3.设置引脚复位功能,设置引脚IO属性 */

	/* 4.设置引脚输出方向 */
	ret = gpio_direction_output(beep.beep_gpio, 1);
	if(ret < 0){
		printk("can set gpio_direction!\r\n");
	} else {
		printk("set gpio_direction!\r\n");
	}
	/* 5.默认开beep */


	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(beep.major){
		beep.devid = MKDEV(beep.major, 0);
		register_chrdev_region(beep.devid, beep_CNT, beep_NAME);
	}else{
		alloc_chrdev_region(&beep.devid, 0, beep_CNT, beep_NAME);
		beep.major = MAJOR(beep.devid);
		beep.minor = MINOR(beep.devid);
	}

	printk("beep major = %d, minor = %d\r\n", beep.major, beep.minor);
	
	/* 2.初始化cdev */
	beep.cdev.owner = THIS_MODULE;
	cdev_init(&beep.cdev, &beep_fops);

	/* 3.添加cdev */
	cdev_add(&beep.cdev, beep.devid, beep_CNT);

	/* 4.创造类 */
	beep.class = class_create(THIS_MODULE, beep_NAME);
	if (IS_ERR(beep.class)){
		return PTR_ERR(beep.class);
	}

	/* 5.创建设备 */
	beep.device = device_create(beep.class, NULL, beep.devid, NULL, beep_NAME);
	if (IS_ERR(beep.device)){
		return PTR_ERR(beep.device);
	}

	return 0;
}

static void __exit beep_exit(void)
{

	/* 注销字符设备驱动 */
	cdev_del(&beep.cdev);
	unregister_chrdev_region(beep.devid, beep_CNT);
	device_destroy(beep.class, beep.devid);
	class_destroy(beep.class);

	printk("beep exit!\n");
}

module_init(beep_init);
module_exit(beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
