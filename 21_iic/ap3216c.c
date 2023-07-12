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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "ap3216creg.h"

#define THISDEVICE_CNT		1
#define THISDEVICE_NAME	"ap3216c"	/* 设备注册名字 */
#define DT_ND_PATH	"/key"					/* 设备树设备节点路径 */
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

/* 字符设备结构体 */
struct ThisDevice{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	struct device_node *nd;	/* 设备节点 */
	int major;				/* 主设备号 */
	void *private_data;		/* 私有数据 */
	unsigned short ir, als, ps;	/* 三个传感器数据 */
};

struct ThisDevice ThisDevice;

static int ap3216c_read_regs(struct ThisDevice *dev, u8 reg, void *val, int len)
{
	int ret = 0;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->private_data;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr; /* ap3216c 地址 */
	msg[0].flags = 0; /* 标记为发送数据 */
	msg[0].buf = &reg; /* 读取的首地址 */
	msg[0].len = 1; /* reg 长度 */	

	/* msg[1]读取数据 */
	msg[1].addr = client->addr; /* ap3216c 地址 */
	msg[1].flags = I2C_M_RD; /* 标记为读取数据 */
	msg[1].buf = val; /* 读取数据缓冲区 */
	msg[1].len = len; /* 要读取的数据长度 */

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}
static s32 ap3216c_write_regs(struct ThisDevice *dev, u8 reg,u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->private_data;

	b[0] = reg; /* 寄存器首地址 */
	memcpy(&b[1],buf,len); /* 将要写入的数据拷贝到数组 b 里面 */

	msg.addr = client->addr; /* ap3216c 地址 */
	msg.flags = 0; /* 标记为写数据*/
	msg.buf = b; /* 要写入的数据缓冲区 */
	msg.len = len + 1; /* 要写入的数据长度 */

	return i2c_transfer(client->adapter, &msg, 1);
}

static unsigned char ap3216c_read_reg(struct ThisDevice *dev, u8 reg)
{
	u8 data = 0;

	ap3216c_read_regs(dev, reg, &data, 1);
	return data;
}

static void ap3216c_write_reg(struct ThisDevice *dev, u8 reg, u8 data)
{
	u8 buf = 0;
	buf = data;
	ap3216c_write_regs(dev, reg, &buf, 1);
}


void ap3216c_readdata(struct ThisDevice *dev)
{
	unsigned char i =0;
	unsigned char buf[6];

	/* 循环读取所有传感器数据 */
	for(i = 0; i < 6; i++)
	{
		buf[i] = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i);
	}

	if(buf[0] & 0X80) /* IR_OF 位为 1,则数据无效 */
	dev->ir = 0;
	else /* 读取 IR 传感器的数据 */
	dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03);

	dev->als = ((unsigned short)buf[3] << 8) | buf[2];/* ALS 数据 */

	if(buf[4] & 0x40) /* IR_OF 位为 1,则数据无效 */
	dev->ps = 0;
	else /* 读取 PS 传感器的数据 */
	dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F);
}

static int ThisDevice_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ThisDevice;


	/* 初始化 AP3216C */
	ap3216c_write_reg(&ThisDevice, AP3216C_SYSTEMCONG, 0x04);
	 mdelay(50); /* AP3216C 复位最少 10ms */
	ap3216c_write_reg(&ThisDevice, AP3216C_SYSTEMCONG, 0X03);
	return 0;

}
static ssize_t ThisDevice_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
 	short data[3];
	long err = 0;

	struct ThisDevice *dev = (struct ThisDevice *)filp->private_data;

	ap3216c_readdata(dev);

	data[0] = dev->ir;
	data[1] = dev->als;
	data[2] = dev->ps;
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

static int i2c_ThisDevice_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("ap3216c_ligh probe!\n");
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
	ThisDevice.private_data = client;
	return 0;
}

static int i2c_ThisDevice_remove(struct i2c_client *client)
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
static const struct i2c_device_id ap3216c_id[] = {
	//{"alientek,ap3216c", 0},
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id i2c_ThisDevice_of_match[] = {
	{ .compatible = "alientek,ap3216c", },
	{	}
};

/* 第二层结构 i2c驱动结构体 */
static struct i2c_driver i2c_ThisDevice = {
	.probe = i2c_ThisDevice_probe,
	.remove = i2c_ThisDevice_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "ap3216caaa",
		.of_match_table = i2c_ThisDevice_of_match,
	},
	.id_table = ap3216c_id,//这里很关键，该成员必须实现
};
/* 第一层结构 */
static int __init ThisDevice_init(void)
{	
	return i2c_add_driver(&i2c_ThisDevice);
}

static void __exit ThisDevice_exit(void)
{
	i2c_del_driver(&i2c_ThisDevice);
}

module_init(ThisDevice_init);
module_exit(ThisDevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
