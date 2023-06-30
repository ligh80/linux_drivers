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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define dtsled_CNT	1
#define dtsled_NAME	"dtsled"
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
struct dtsled_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *nd;
};

struct dtsled_dev dtsled;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON){
		val = readl(GPIO1_DR);
		val &= ~(1 <<3);
		writel(val, GPIO1_DR);
	}else if(sta == LEDOFF){
		val = readl(GPIO1_DR);
		val |= (1 << 3);
		writel(val, GPIO1_DR);
	}
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &dtsled;
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

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0){
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];	/* 获取状态值 */
	if(ledstat == LEDON){
		led_switch(LEDON);
	}else if(ledstat == LEDOFF){
		led_switch(LEDOFF);
	}
	return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations dtsled_fops={
	.owner = THIS_MODULE,
	.open = led_open,
	.release = led_release,
	.read = led_read,
	.write = led_write,
};

static int __init led_init(void)
{	
	u32 val = 0;
	int ret;
	u32 regdata[14];
	const char *str;
	struct property *proper;

	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：alphaled */
	dtsled.nd = of_find_node_by_path("/alphaled");
	if(dtsled.nd == NULL){
		printk("alphaled node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("alphaled node has been found!\r\n");
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(dtsled.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(dtsled.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}
	/* 4、获取 reg 属性内容 */
	ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10);
	if(ret < 0){
		printk("reg read failed!\r\n");
	} else {
		u8 i = 0;
		printk("reg data = \r\n");
		for(i = 0; i < 10; i++)
			printk("%#x ", regdata[i]);
		printk("\r\n");
	}


	/* 初始化LED 1.寄存器映射  */
#if 0 
	IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);	
	SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);	
	SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);	
	GPIO1_DR = ioremap(regdata[6], regdata[7]);
	GPIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
	IMX6U_CCM_CCGR1 = of_iomap(dtsled.nd, 0);
	SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
	SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
	GPIO1_DR = of_iomap(dtsled.nd, 3);
	GPIO1_GDIR = of_iomap(dtsled.nd, 4);
#endif

	/* 2.使能GPIO1时钟 */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);
	val |= (3 << 26);
	writel(val, IMX6U_CCM_CCGR1);

	/* 3.设置引脚复位功能,设置引脚IO属性 */
	writel(5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* 4.设置引脚输出方向 */
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO1_GDIR);
 
	/* 5.默认开LED */
	val = readl(GPIO1_DR);
	val &= ~(1 << 3);
	writel(val, GPIO1_DR);

	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(dtsled.major){
		dtsled.devid = MKDEV(dtsled.major, 0);
		register_chrdev_region(dtsled.devid, dtsled_CNT, dtsled_NAME);
	}else{
		alloc_chrdev_region(&dtsled.devid, 0, dtsled_CNT, dtsled_NAME);
		dtsled.major = MAJOR(dtsled.devid);
		dtsled.minor = MINOR(dtsled.devid);
	}

	printk("dtsled major = %d, minor = %d\r\n", dtsled.major, dtsled.minor);
	
	/* 2.初始化cdev */
	dtsled.cdev.owner = THIS_MODULE;
	cdev_init(&dtsled.cdev, &dtsled_fops);

	/* 3.添加cdev */
	cdev_add(&dtsled.cdev, dtsled.devid, dtsled_CNT);

	/* 4.创造类 */
	dtsled.class = class_create(THIS_MODULE, dtsled_NAME);
	if (IS_ERR(dtsled.class)){
		return PTR_ERR(dtsled.class);
	}

	/* 5.创建设备 */
	dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, dtsled_NAME);
	if (IS_ERR(dtsled.device)){
		return PTR_ERR(dtsled.device);
	}

	return 0;
}

static void __exit led_exit(void)
{
	/* 退出关闭LED */
	u32 val = 0;
	val = readl(GPIO1_DR);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	iounmap(IMX6U_CCM_CCGR1);	
	iounmap(SW_MUX_GPIO1_IO03);	
	iounmap(SW_PAD_GPIO1_IO03);	
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	/* 注销字符设备驱动 */
	cdev_del(&dtsled.cdev);
	unregister_chrdev_region(dtsled.devid, dtsled_CNT);
	device_destroy(dtsled.class, dtsled.devid);
	class_destroy(dtsled.class);

	printk("dtsled exit!\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
