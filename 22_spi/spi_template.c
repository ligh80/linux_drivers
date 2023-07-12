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
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "icm20608reg.h"

#define THISDEVICE_CNT		1
#define THISDEVICE_NAME	"icm20608"	/* 设备注册名字 */
#if 0
#define DT_ND_PATH		"/key"					/* 设备树设备节点路径 */
#define DT_COMPATIBLE	"atkalpha-key"		/* 匹配用属性名字 */
#define DT_GPIO			"key-gpio"			/* gpio属性名字 */
#define INVAKEY			0XFF
#define KEY_NUM		1	/* 按键数量 */

/* 中断IO描述结构体 */
struct irp_keydesc{
	int gpio;								/* gpio */
	int irqnum;								/* 中断号 */
	irqreturn_t (*handler)(int, void *);	/* 中断处理函数 */
	unsigned long flags;					/* 中断标志 例如IRQF_TRIGGER_RISING */
	char name[10];							/* 中断名称 */					
	unsigned char value;					/* 按键对应的健值 */
};
#endif
/* 字符设备结构体 */
struct ThisDevice{
	dev_t devid;				/* 设备号 	 */
	struct cdev cdev;			/* cdev 	*/
	struct class *class;		/* 类 */
	struct device *device; 		/* 设备 */
	struct device_node *nd;		/* 设备节点 */
	int major;					/* 主设备号 */
	void *private_data;			/* 私有数据 */
	signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/
};

struct ThisDevice ThisDevice;

static int icm20608_read_regs(struct ThisDevice *dev, u8 reg, void *val, int len)
{

}
static s32 icm20608_write_regs(struct ThisDevice *dev, u8 reg,u8 *buf, u8 len)
{

}

static unsigned char icm20608_read_reg(struct ThisDevice *dev, u8 reg)
{

}

static void icm20608_write_reg(struct ThisDevice *dev, u8 reg, u8 data)
{

}


void icm20608_readdata(struct ThisDevice *dev)
{

}

static int ThisDevice_open(struct inode *inode, struct file *filp)
{
	return 0;

}
static ssize_t ThisDevice_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	return 0;
}

static int ThisDevice_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations ThisDevice_ops = {
	.owner = THIS_MODULE,
	.open = ThisDevice_open,
	.read = ThisDevice_read,
	.release = ThisDevice_release,
};

static int icm20608_driver_probe(struct spi_device *spi)
{
	printk("icm20608_ligh probe!\n");
	/* 1.构建设备号 */
	if (ThisDevice.major) {
		ThisDevice.devid = MKDEV(ThisDevice.major, 0);
		register_chrdev_region(ThisDevice.devid, THISDEVICE_CNT, THISDEVICE_NAME);
	} else {
		alloc_chrdev_region(&ThisDevice.devid, 0, THISDEVICE_CNT, THISDEVICE_NAME);
		ThisDevice.major = MAJOR(ThisDevice.devid);
	}
	/* 2.注册设备 */
	cdev_init(&ThisDevice.cdev, &ThisDevice_ops);
	cdev_add(&ThisDevice.cdev, ThisDevice.devid, THISDEVICE_CNT);
	/* 3.创造类 */
	ThisDevice.class = class_create(THIS_MODULE, THISDEVICE_NAME);
	if (IS_ERR(ThisDevice.class)) {
		return PTR_ERR(ThisDevice.class);
	}
	/* 4.创建设备 */
	ThisDevice.device = device_create(ThisDevice.class, NULL, ThisDevice.devid, NULL, THISDEVICE_NAME);
	if (IS_ERR(ThisDevice.device)) {
		return PTR_ERR(ThisDevice.device);
	}
	ThisDevice.private_data = spi;
	return 0;
}

static int icm20608_driver_remove(struct i2c_client *client)
{
	/* 删除设备 */
	cdev_del(&ThisDevice.cdev);
	unregister_chrdev_region(ThisDevice.devid, THISDEVICE_CNT);

	/* 注销类和设备 */
	device_destroy(ThisDevice.class, ThisDevice.devid);
	class_destroy(ThisDevice.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id icm20608_id[] = {
	{"alientek,icm20608", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_driver_of_match[] = {
	{ .compatible = "alientek,icm20608", },
	{	}
};

/* 第二层结构 spi驱动结构体 */
static struct spi_driver icm20608_driver = {
	.probe = icm20608_driver_probe,
	.remove = icm20608_driver_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "icm20608",			/* 匹配驱动：这个名字会跟设备树的compatible","后的字符进行匹配 */
		.of_match_table = icm20608_driver_of_match,/* 匹配驱动：这个名字会跟设备树的compatible整个字符进行匹配 */
	},
	.id_table = icm20608_id,//这是传统匹配方式，但似乎现有的设备树匹配不上
};
/* 第一层结构 */
static int __init ThisDevice_init(void)
{	
	return spi_register_driver(&icm20608_driver);
}

static void __exit ThisDevice_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}

module_init(ThisDevice_init);
module_exit(ThisDevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
