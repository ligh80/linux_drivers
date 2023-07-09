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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define THISDEVICE_CNT		1
#define THISDEVICE_NAME	"keyinput"	/* 设备注册名字 */
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
	struct timer_list timer; /* 定义一个定时器 */
	struct irp_keydesc irq_keydesc[KEY_NUM];
	unsigned char crukeyindex;	/*当前使用的按键索引 */
	struct input_dev *input_dev;/* 定义input结构体 */
};

struct ThisDevice ThisDevice;

void timer_function(unsigned long arg)
{
	struct ThisDevice *dev = (struct ThisDevice *)arg;
	unsigned char value;
	unsigned char index;
	struct irp_keydesc *keydesc;

	/* 定时器10ms防抖定时处理 */
	index = dev->crukeyindex;		/* 确认是哪一个按键的中断,理解成索引值 */
	keydesc = &dev->irq_keydesc[index];
	value = gpio_get_value(keydesc->gpio);	/* 10ms后该引脚的状态 */
	if (value == 0){						/* 按键按下 */	
		input_report_key(dev->input_dev, keydesc->value, 1);/* 1 表示按下 */
		//printk("input_report_key arg code:%d\n", keydesc->value);
		input_sync(dev->input_dev);
	} else {								/* 按键松开 */
		input_report_key(dev->input_dev, keydesc->value, 0);/* 0 表示释放 */
		input_sync(dev->input_dev);
	}

}

static irqreturn_t key0_handler(int irq, void *dev_struct)
{
	struct ThisDevice *dev = (struct ThisDevice *)dev_struct;		/* 由requst函数传进来的 */
	dev->crukeyindex = 0;
	dev->timer.data = (volatile long)dev_struct;			/* 给time的私有变量赋值，传递设备结构体，timer_function的arg参数 */
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int keyio_init(void)
{
	int i, ret = 0;
	struct property *proper;
	const char *str;
	u32 interruptsData[KEY_NUM*2];

	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点： */
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
	/* 4 获取每个按键的中断信息 */
	of_property_read_u32_array(ThisDevice.nd, "interrupts", interruptsData, KEY_NUM*2);
	for (i=0; i<KEY_NUM; i++) {
		ThisDevice.irq_keydesc[i].gpio = of_get_named_gpio(ThisDevice.nd, DT_GPIO, i);
		if(ThisDevice.irq_keydesc[i].gpio < 0){
			printk("can not get %s_%d!\r\n", DT_GPIO, i);
		} else {
			printk("%s num = %d\r\n",DT_GPIO ,ThisDevice.irq_keydesc[i].gpio);
			memset(ThisDevice.irq_keydesc[i].name, 0, sizeof(ThisDevice.irq_keydesc[i].name));
			sprintf(ThisDevice.irq_keydesc[i].name, "KEY%d", i);
			gpio_request(ThisDevice.irq_keydesc[i].gpio, ThisDevice.irq_keydesc[i].name);
			gpio_direction_input(ThisDevice.irq_keydesc[i].gpio);
			/* 这个中断号是芯片设计已经分配好的,根据设备树节点里的interupts，查询map获取 */
			ThisDevice.irq_keydesc[i].irqnum = irq_of_parse_and_map(ThisDevice.nd, i);
			ThisDevice.irq_keydesc[i].flags = interruptsData[1+i*2];		
		}
	}
	ThisDevice.irq_keydesc[0].handler = key0_handler;
	ThisDevice.irq_keydesc[0].value = KEY_0;
	/* 申请中断 */
	for ( i = 0; i < KEY_NUM; i++){
		ret = request_irq(ThisDevice.irq_keydesc[i].irqnum, 
							ThisDevice.irq_keydesc[i].handler,
							ThisDevice.irq_keydesc[i].flags, 
							ThisDevice.irq_keydesc[i].name, 
							&ThisDevice);/* handler函数的最后一个参数 */
		printk("request_irq index:%d, name:%s gpio=%d, irqnum=%d, flags=%ld\r\n", i, 
										ThisDevice.irq_keydesc[i].name,
										ThisDevice.irq_keydesc[i].gpio,
										ThisDevice.irq_keydesc[i].irqnum,
										ThisDevice.irq_keydesc[i].flags);
		if (ret < 0){
			printk("irq %d request failed!\r\n", 
					ThisDevice.irq_keydesc[i].irqnum);
			return -EFAULT;
		}
	}
	/* 一般情况下会注册对应的字符设备，但是这里我们使用 input 设备
	* 所以我们不需要自己注册字符设备驱动，只需要注册 input 设备驱动即可
	*/
	/* 申请input_dev  */
	ThisDevice.input_dev = input_allocate_device();
	ThisDevice.input_dev->name = THISDEVICE_NAME;
	/*********采用第三种设置事件和事件值的方法***********/
	/* 要使用到按键需要使能 EV_KEY 事件，使用连按功能需要使能 EV_REP 事件 */
	ThisDevice.input_dev->evbit[0] = BIT_MASK(EV_KEY) |BIT_MASK(EV_REP);
	/* 设置支持的键盘事件（事件值） */
	input_set_capability(ThisDevice.input_dev, EV_KEY, KEY_0);//支持按键0
	
	ret = input_register_device(ThisDevice.input_dev);
	if (ret < 0 ){
		printk("register input device failed!\r\n");
		return ret;
	}

	init_timer(&ThisDevice.timer);
	ThisDevice.timer.function = timer_function;
	return 0;
}


/* 第一层结构-ligh */
static int __init ThisDevice_init(void)
{	
	return keyio_init();
}

static void __exit ThisDevice_exit(void)
{
	unsigned int i = 0;
	/* 删除定时器 */
	del_timer(&ThisDevice.timer);

	/* 释放key中断和goio */
	for (i=0; i<KEY_NUM; i++){
		free_irq(ThisDevice.irq_keydesc[i].irqnum,	&ThisDevice);
		gpio_free(ThisDevice.irq_keydesc[i].gpio);
	}

	/* 释放input dev */
	input_unregister_device(ThisDevice.input_dev);
	input_free_device(ThisDevice.input_dev);
}

module_init(ThisDevice_init);
module_exit(ThisDevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
