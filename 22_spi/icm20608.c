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
	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	rxdata = kzalloc(sizeof(char)*len, GFP_KERNEL);/* 申请接收内存 */
	if(!rxdata) {
		goto out1;
	}

	txdata[0] = reg | 0x80;	/* 写数据的时候首寄存器地址bit8要置1 */
	t->tx_buf = txdata;
	t->rx_buf = rxdata;
	t->len = len+1;			/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);
	spi_message_add_tail(t, &m);
	ret = spi_sync(spi, &m);/* 同步发送 */
	if (ret) {
		goto out2;
	}
	memcpy(val, rxdata+1, len);		/*这一句很重要*/
	/*SPI 为全双工通讯没有所谓的发送和接收长度之分。要读取或者发送 N 个字节就
	要封装 N+1 个字节，第 1 个字节是告诉设备我们要进行读还是写，后面的 N 个字节才是我们
	要读或者发送的数据*/
out2:
	kfree(rxdata);
out1:
	kfree(t);

	return ret;
}
static s32 icm20608_write_regs(struct ThisDevice *dev, u8 reg,u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char * txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	txdata = kzalloc(sizeof(char)*len, GFP_KERNEL);/* 申请发送内存 */
	if(!txdata) {
		goto out1;
	}

	txdata[0] &= ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
	memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;
	t->len = len+1;			/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);
	spi_message_add_tail(t, &m);
	ret = spi_sync(spi, &m);/* 同步发送 */
	if (ret) {
		goto out2;
	}
out2:
	kfree(txdata);
out1:
	kfree(t);

	return ret;
}

static unsigned char icm20608_read_reg(struct ThisDevice *dev, u8 reg)
{
	u8 data = 0;
	icm20608_read_regs(dev, reg, &data, 1);
	return data;
}

static void icm20608_write_reg(struct ThisDevice *dev, u8 reg, u8 data)
{	
	icm20608_write_regs(dev, reg, &data, 1);
}


void icm20608_readdata(struct ThisDevice *dev)
{
	unsigned char data[14] = { 0 };
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);
	dev->accel_x_adc = (signed short)(data[0] << 8 | data[1]);
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
}

static int ThisDevice_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ThisDevice;	/* 设置私有数据 */
	return 0;
}
static ssize_t ThisDevice_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	signed int data[7];
	long err = 0;
	struct ThisDevice *dev = (struct ThisDevice *)filp->private_data;

	icm20608_readdata(dev);
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	err = copy_to_user(buf, data, sizeof(data));
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

void icm20608_reginit(void)
{
	u8 value = 0;
	icm20608_write_reg(&ThisDevice, ICM20_PWR_MGMT_1, 0X80);
	mdelay(50);
	icm20608_write_reg(&ThisDevice, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_reg(&ThisDevice, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_reg(&ThisDevice, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_reg(&ThisDevice, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_reg(&ThisDevice, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_reg(&ThisDevice, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_reg(&ThisDevice, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_reg(&ThisDevice, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_reg(&ThisDevice, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_reg(&ThisDevice, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/

}

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

	/* 初始化spi_driver */
	spi->mode = SPI_MODE_0; 
	spi_setup(spi);
	ThisDevice.private_data = spi;

	icm20608_reginit();
	return 0;
}

static int icm20608_driver_remove(struct spi_device *spi)
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
