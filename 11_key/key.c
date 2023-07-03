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

#define KEY_CNT		1
#define KEY_NAME	"key"
#define KEY0VALUE	0XF0	/* 按键值 */
#define INVAKEY		0X00	/* 无效的按键值 */

/* 字符设备结构体 */
struct char_dev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;	/* 类 */
	struct device *device; /* 设备 */
	int major;
	int minor;
	struct device_node *nd;
	int key_gpio_id;			/* 所使用的GPIO编号 */
	atomic_t keyvalue;
};

struct char_dev key;


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key;

	return 0;
}

static ssize_t key_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	int ret = 0;
	unsigned char value;
	struct char_dev *dev= filp->private_data;

	if(gpio_get_value(dev->key_gpio_id) == 0){/*如果按键按下*/
		while (!gpio_get_value(dev->key_gpio_id));
		atomic_set(&dev->keyvalue, KEY0VALUE);
	} else {
		atomic_set(&dev->keyvalue, INVAKEY);
	}

	value = atomic_read(&dev->keyvalue);	
	ret = copy_to_user(buf, &value, sizeof(value));
	return ret;
}


static const struct file_operations key_fops={
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
};

static int __init mykey_init(void)
{	
	int ret = 0;
	const char *str;
	struct property *proper;

	/* 初始化原子变量 */
	atomic_set(&key.keyvalue, INVAKEY);
	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点 */
	key.nd = of_find_node_by_path("/key");
	if(key.nd == NULL){
		printk("key node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("key node has been found!\r\n");
	}
	/* 2、获取compatible 属性内容 */
	proper = of_find_property(key.nd, "compatible", NULL);
	if(proper == NULL){
		printk("compatible property find failed!\r\n");
	} else {
		printk("compatible = %s\r\n", (char*)proper->value);
	}
	/* 3、获取status 属性内容 */
	ret = of_property_read_string(key.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\r\n", str);
	}

	key.key_gpio_id = of_get_named_gpio(key.nd, "key-gpio", 0);
	if(key.key_gpio_id < 0){
		printk("can not get key_gpio_id!\r\n");
	} else {
		printk("key_gpio_id num = %d\r\n", key.key_gpio_id);
	}
	/* 初始化LED 1.寄存器映射  */

	/* 2.使能GPIO1时钟 */

	/* 3.设置引脚复位功能,设置引脚IO属性 */

	/* 4.设置引脚输出方向 */
	gpio_request(key.key_gpio_id, KEY_NAME);
	gpio_direction_input(key.key_gpio_id);

	/* 5.默认开LED */


	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(key.major){
		key.devid = MKDEV(key.major, 0);
		register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
	}else{
		alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
		key.major = MAJOR(key.devid);
		key.minor = MINOR(key.devid);
	}

	printk("key major = %d, minor = %d\r\n", key.major, key.minor);
	
	/* 2.初始化cdev */
	key.cdev.owner = THIS_MODULE;
	cdev_init(&key.cdev, &key_fops);

	/* 3.添加cdev */
	cdev_add(&key.cdev, key.devid, KEY_CNT);

	/* 4.创造类 */
	key.class = class_create(THIS_MODULE, KEY_NAME);
	if (IS_ERR(key.class)){
		return PTR_ERR(key.class);
	}

	/* 5.创建设备 */
	key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
	if (IS_ERR(key.device)){
		return PTR_ERR(key.device);
	}

	return 0;
}

static void __exit mykey_exit(void)
{


	/* 注销字符设备驱动 */
	cdev_del(&key.cdev);
	unregister_chrdev_region(key.devid, KEY_CNT);
	device_destroy(key.class, key.devid);
	class_destroy(key.class);

	printk("key exit!\n");
}

module_init(mykey_init);
module_exit(mykey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
