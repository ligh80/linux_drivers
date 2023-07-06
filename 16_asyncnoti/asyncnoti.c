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
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/atomic.h>

#define DEVICE_CNT		1
#define DEVICE_NAME		"asyncnoti_dev"
#define CLOSE_CMD		_IO(0XEF, 0X1)
#define OPEN_CMD		_IO(0XEF, 0X2)
#define SETPERIOD_CMD	_IOW(0XEF, 0x3, int)
#define KEY_CNT			1
#define KEY0_VALUE		0X10
#define KEY_INV_VALUE	0XFF
//#define SLEEP_MANUAL /* 选择手动休眠还是自动休眠 */
#define ASYNC_NOTIC		/* 开启异步通知访问 */

/* 中断IO描述结构体 */
struct irp_keydesc{
	int gpio;								/* gpio */
	int irqnum;								/* 中断号 */
	irqreturn_t (*handler)(int, void *);
	unsigned long flags;					/* 中断标志 例如IRQF_TRIGGER_RISING */
	char name[10];							/* 中断名称 */					
	unsigned char value;					/* 按键对应的健值 */
};

/* 字符设备结构体 */
struct chardev{
	dev_t devid;
	struct cdev cdev;
	struct class *class;				/* 类 */
	struct device *device; 				/* 设备 */
	int major;							/* 主设备号 */
	int minor;							/* 次设备号 */
	struct device_node *nd;				/* 设备节点 */
	int led_gpio_id;					/* 设备所使用的GPIO编号*/
	int timerperiod;					/* 定时周期，单位为ms */
	struct timer_list timer;			/* 定义一个定时器 */
	spinlock_t lock;					/* 定义自旋锁 */
	struct irp_keydesc irqkeydesc[KEY_CNT];	/* 为每一个按键设置一个中断描述 */
	atomic_t keyvalue;					/* 有效的按键键值 */
	atomic_t releasekey;				/* 按键释放标志位 */
	unsigned char curkeynum;			/* 设置目前是第几个按键，用来对应读取desc里的信息	*/
	wait_queue_head_t r_wait;			/* 读等待队列头,用这个wait模块来唤醒进程 */
	struct fasync_struct *async_queue;	/* 异步相关结构体 */
};

struct chardev ThisDevice;

static irqreturn_t key0_handler(int irq, void *dev_struct)
{
	struct chardev *dev = (struct chardev *)dev_struct;		/* 这里有比较大的疑惑 */
	dev->curkeynum =0;
	dev->timer.data = (volatile long)dev_struct;			/* 给time的私有变量赋值，传递设备结构体，timer_function的arg参数 */
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int ledio_init(void)
{
	int ret = 0;
	const char *str;
	struct property *proper;

	/* 获取设备树中的属性数据 */
	/* 1、获取设备节点：gpioled */
	ThisDevice.nd = of_find_node_by_path("/gpioled");
	if(ThisDevice.nd == NULL){
		printk("gpioled node can not found!\r\n");
		return -EINVAL;
	} else {
		printk("gpioled node has been found!\r\n");
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
	/* 3、获取分配的gpio id号 */
	ThisDevice.led_gpio_id = of_get_named_gpio(ThisDevice.nd, "led-gpio", 0);
	if(ThisDevice.led_gpio_id < 0){
		printk("can not get led_gpio_id!\r\n");
	} else {
		printk("led_gpio_id num = %d\r\n", ThisDevice.led_gpio_id);
	}

	/* 4、初始化led所使用的IO */
	gpio_request(ThisDevice.led_gpio_id, DEVICE_NAME);		/* 申请的作用的排除已经有人在使用 */
	ret = gpio_direction_output(ThisDevice.led_gpio_id, 1);
	if(ret < 0){
		printk("Set IO Failed!\r\n");
	} else {
		printk("Set IO Successed!\r\n");
	}

	return 0;
}

static int keyio_init(void)
{
	unsigned char i = 0;
	int ret = 0;
	u32 InterruptsData[KEY_CNT*2];

	ThisDevice.nd = of_find_node_by_path("/key");
	/* 提取GPIO */
	for (i = 0; i <KEY_CNT; i++) {
		ThisDevice.irqkeydesc[i].gpio = of_get_named_gpio(ThisDevice.nd, "key-gpio", i);
		if (ThisDevice.irqkeydesc[i].gpio < 0) {
			printk("can't get key-gpio\r\n");
		}
	}	

	/* 初始化key所使用的IO，并且设置成中断模式 */
	for (i = 0; i < KEY_CNT; i++) {
		memset(ThisDevice.irqkeydesc[i].name, 0, sizeof(ThisDevice.irqkeydesc[i].name));
		sprintf(ThisDevice.irqkeydesc[i].name, "KEY%d", i);
		gpio_request(ThisDevice.irqkeydesc[i].gpio, ThisDevice.irqkeydesc[i].name);
		gpio_direction_input(ThisDevice.irqkeydesc[i].gpio);
		/* 这个中断号是芯片设计已经分配好的,根据设备树节点里的interupts，查询map获取 */
		ThisDevice.irqkeydesc[i].irqnum = irq_of_parse_and_map(ThisDevice.nd, i);
		of_property_read_u32_array(ThisDevice.nd, "interrupts", InterruptsData, KEY_CNT*2);
		ThisDevice.irqkeydesc[i].flags = InterruptsData[1+i*2];
	}
	ThisDevice.irqkeydesc[0].handler = key0_handler;
	ThisDevice.irqkeydesc[0].value = KEY0_VALUE;

	/* 申请中断 */
	for ( i = 0; i < KEY_CNT; i++){
		ret = request_irq(ThisDevice.irqkeydesc[i].irqnum, 
							ThisDevice.irqkeydesc[i].handler,
							ThisDevice.irqkeydesc[i].flags, 
							ThisDevice.irqkeydesc[i].name, 
							&ThisDevice);
		printk("request_irq index:%d, name:%s gpio=%d, irqnum=%d, flags=%ld\r\n", i, 
										ThisDevice.irqkeydesc[i].name,
										ThisDevice.irqkeydesc[i].gpio,
										ThisDevice.irqkeydesc[i].irqnum,
										ThisDevice.irqkeydesc[i].flags);
		if (ret < 0){
			printk("irq %d request failed!\r\n", 
					ThisDevice.irqkeydesc[i].irqnum);
			return -EFAULT;
		}
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
static int ThisDevice_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &ThisDevice;

	ThisDevice.timerperiod = 1000;

	/* 初始化等队列头 */
	init_waitqueue_head(&ThisDevice.r_wait);


	return 0;
}

/*
* @description : ioctl 函数，
* @param – filp : 要打开的设备文件(文件描述符)
* @param - cmd : 应用程序发送过来的命令
* @param - arg : 参数
* @return : 0 成功;其他 失败
*/
static long ThisDevice_unlocked_ioctl(struct file *filp,unsigned int cmd,
			unsigned long arg)
{
	struct chardev *dev = (struct chardev *)filp->private_data;
	int timerperiod;
	unsigned long flags;
	printk("driver cmd:%d arg:%ld\r\n", cmd, arg);
	switch (cmd)
	{
	case CLOSE_CMD:		/* 关闭定时器 */
		gpio_set_value(dev->led_gpio_id, 1);
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
/* 当应用程序通过“fcntl(fd, F_SETFL, flags | FASYNC)”
*改变fasync 标记的时候,该成员函数就会执行 */
static int ThisDevice_fasync (int fd, struct file *filp, int on)
{
	struct chardev *dev = (struct chardev *)filp->private_data;
	/* 初始化fasync_struct结构体async_queue */
	return fasync_helper(fd, filp, on, &dev->async_queue);
}

static ssize_t ThisDevice_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	int ret = 0;
	unsigned char keyvalue = 0;
	unsigned char releasekey = 0;
	struct chardev *dev = (struct chardev *)filp->private_data;


	if (filp->f_flags & O_NONBLOCK) {	/* 如果是非阻塞访问 */
		if (atomic_read(&dev->releasekey) == 0)		/* 没有按键按下 */
			return -EAGAIN;
	} else {		/* 如果是阻塞访问 */
	#ifdef SLEEP_MANUAL	//采用手动进入休眠
	 /* 阻塞IO访问设置 */
	DECLARE_WAITQUEUE(wait, current);	/* 定义一个等待队列项 */
	if (atomic_read(&dev->releasekey) == 0) {	/* 没有按键按下并且松开 */
		add_wait_queue(&dev->r_wait, &wait);	/* 将等待队列项增加到等待队列头 */
		__set_current_state(TASK_INTERRUPTIBLE);/* 将当前进程设置成可信号打断模式 */
		schedule();								/* 进行一次任务切换，当前进程进入休眠，等待中断唤醒 */
		if (signal_pending(current)) {			/* 进程唤醒点，判断是不是信号引起的唤醒 */
			ret = -ERESTARTSYS;						/* 如果是信号唤醒，直接退出 */
			__set_current_state(TASK_RUNNING);		/* 设置成运行状态 */
			remove_wait_queue(&dev->r_wait, &wait);	/* 移除等待队列项 */
			goto wait_error;
		}
		__set_current_state(TASK_RUNNING);		/* 设置成运行状态 */
		remove_wait_queue(&dev->r_wait, &wait);	/* 移除等待队列项 */
	}
	#else	/* 采用判断条件进入休眠 */
		/* 设置等待队列，条件不为0时进程马上进入休眠 */
		ret = wait_event_interruptible(dev->r_wait, atomic_read(&dev->releasekey));
		if (ret) {
			goto wait_error;
		}
	#endif //SLEEP_MANUAL
	
	}
	
	keyvalue = atomic_read(&dev->keyvalue);
	releasekey = atomic_read(&dev->releasekey);

	if (releasekey) {		/* 有键按下且已经释放，且未读取 */
		if (keyvalue & 0x80) {
			keyvalue &= ~0x80;
			ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
		} else {
			goto data_error;
		}
		atomic_set(&dev->releasekey, 0);	/* 按下键且释放标记清零 */
	} else {
		goto data_error;
	}
	
	return 0;

wait_error:
	return ret;

data_error:
	return -EINVAL;
}

static ssize_t ThisDevice_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	return 0;
}

static int ThisDevice_release(struct inode *inode, struct file *filp)
{
	return ThisDevice_fasync(-1, filp, 0);	/* 删除异步通知 */
}

unsigned int ThisDevice_poll (struct file *filp, struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct chardev *dev = (struct chardev *)filp->private_data;
	/* 将当前进程添加到指定的等待队列中，此时未执行休眠 */
	poll_wait(filp, &dev->r_wait, wait);
	/* 判断设备就绪状态，后续机制会执行timeout时长的休眠 */
	if (atomic_read(&dev->releasekey)){
		mask = POLLIN | POLLRDBAND;
	}
	return mask;
}


static const struct file_operations ThisDevice_fops={
	.owner = THIS_MODULE,
	.open = ThisDevice_open,
	.release = ThisDevice_release,
	.read = ThisDevice_read,
	.write = ThisDevice_write,
	.unlocked_ioctl = ThisDevice_unlocked_ioctl,
	.poll = ThisDevice_poll,
	.fasync = ThisDevice_fasync,
};

void timer_function(unsigned long arg)
{
	struct chardev *dev = (struct chardev *)arg;
	unsigned char value;
	unsigned char num;
	struct irp_keydesc *keydesc;

	/* 定时器10ms防抖定时处理 */
	num = dev->curkeynum;		/* 确认是哪一个按键的中断,理解成索引值 */
	keydesc = &dev->irqkeydesc[num];
	value = gpio_get_value(keydesc->gpio);	/* 10ms后该引脚的状态 */
	if (value == 0){						/* 按键按下 */	
		atomic_set(&dev->keyvalue, keydesc->value);
		gpio_set_value(ThisDevice.led_gpio_id, 0);	/* 打开led */
	} else {								/* 按键松开 */
		atomic_set(&dev->keyvalue, 0x80 | keydesc->value);		/* 这里为什么要或0x80？ */	
		atomic_set(&dev->releasekey, 1);						/* 标记按键按下后已经松开 */
		gpio_set_value(ThisDevice.led_gpio_id, 1);	/* 关闭led */
		#ifdef ASYNC_NOTIC	/* 使用异步通知 */
		if(atomic_read(&dev->releasekey)){
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);/* kill_fasync 函数发送 SIGIO 信号 */
		}
		#else		/* 使用wait唤醒阻塞或非阻塞进程 */
		/* 唤醒ThisDevice_read()函数里的休眠进程 */
		wake_up_interruptible(&dev->r_wait);
		#endif // ASYNC_NOTIC
	}

}

static int __init ThisDevice_init(void)
{	
	int ret = 0;
	/* 初始化自旋锁 */
	spin_lock_init(&ThisDevice.lock);

	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	if(ThisDevice.major){
		ThisDevice.devid = MKDEV(ThisDevice.major, 0);
		register_chrdev_region(ThisDevice.devid, DEVICE_CNT, DEVICE_NAME);
	}else{
		alloc_chrdev_region(&ThisDevice.devid, 0, DEVICE_CNT, DEVICE_NAME);
		ThisDevice.major = MAJOR(ThisDevice.devid);
		ThisDevice.minor = MINOR(ThisDevice.devid);
	}
	printk("ThisDevice major = %d, minor = %d\r\n", ThisDevice.major, ThisDevice.minor);
	
	/* 2.初始化cdev */
	ThisDevice.cdev.owner = THIS_MODULE;
	cdev_init(&ThisDevice.cdev, &ThisDevice_fops);

	/* 3.添加cdev */
	cdev_add(&ThisDevice.cdev, ThisDevice.devid, DEVICE_CNT);

	/* 4.创造类 */
	ThisDevice.class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(ThisDevice.class)){
		return PTR_ERR(ThisDevice.class);
	}

	/* 5.创建设备 */
	ThisDevice.device = device_create(ThisDevice.class, NULL, ThisDevice.devid, NULL, DEVICE_NAME);
	if (IS_ERR(ThisDevice.device)){
		return PTR_ERR(ThisDevice.device);
	}

	/* 6.初始化timer,设置定时器处理函数，还未设置周期，这里不会激活定时器 */
	init_timer(&ThisDevice.timer);
	ThisDevice.timer.function = timer_function;
	ThisDevice.timer.data = (unsigned long)&ThisDevice;

	/* 7.初始化按键 */
	atomic_set(&ThisDevice.keyvalue, KEY_INV_VALUE);
	atomic_set(&ThisDevice.releasekey, 0);

	ret = keyio_init();
	if(ret < 0){
		return ret;
	}
	ret = ledio_init();
	if(ret < 0){
		return ret;
	}	

	return 0;
}

static void __exit ThisDevice_exit(void)
{	
	unsigned int i = 0;

	gpio_set_value(ThisDevice.led_gpio_id, 1);	/* 关闭led */
	del_timer_sync(&ThisDevice.timer);			/* 删除定时器 */
	gpio_free(ThisDevice.led_gpio_id);			/* 释放led的gpio标号 */

	for (i = 0; i < KEY_CNT; i++){
		free_irq(ThisDevice.irqkeydesc[i].irqnum, &ThisDevice); /* 释放中断函数 */
		gpio_free(ThisDevice.irqkeydesc[i].gpio);				
	}

	/* 注销字符设备驱动 */
	cdev_del(&ThisDevice.cdev);
	unregister_chrdev_region(ThisDevice.devid, DEVICE_CNT);
	device_destroy(ThisDevice.class, ThisDevice.devid);
	class_destroy(ThisDevice.class);

	printk("ThisDevice exit!\n");
}

module_init(ThisDevice_init);
module_exit(ThisDevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
