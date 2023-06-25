#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#define CHRDEVBASE_MAJOR	200	/* 设备主设备号 */
#define CHRDEVBASE_NAME		"chrdevbase"

static char readbuf[100];	/* 读缓存区 */
static char writebuf[100];	/* 写缓存区 */
static char kerneldata[] = {"Kernel data!"};	



static int chrdevbase_open(struct inode *inode, struct file *filp)
{
	printk("chrdevbase opened!\r\n");
	return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{

	printk("chrdevbase released!\r\n");
	return 0;
}

static ssize_t chrdevbase_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
	int retvalue=0;

	memcpy(readbuf, kerneldata, sizeof(kerneldata));
	retvalue = copy_to_user(buf, readbuf, count);
	if(retvalue == 0){
		printk("kernel senddata ok!\r\n");
	}else{
		printk("kernel senddata failed!\r\n");
	}

	return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int retvalue = 0;

	retvalue = copy_from_user(writebuf, buf, count);
	if(retvalue == 0){
		printk("kernel recevdata:%s\r\n", writebuf);
	}else{
		printk("kernel recevdata failed!\r\n");
	}
	return 0;
}


static const struct file_operations chrdevbase_fops={
	.owner = THIS_MODULE,
	.open = chrdevbase_open,
	.release = chrdevbase_release,
	.read = chrdevbase_read,
	.write = chrdevbase_write,
};

static int __init chrdevbase_init(void)
{	
	int ret=0;

	ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
	if(ret < 0){
		printk("chrdevase init	failed!\n");
	}
	else{
		printk("chrdevase init	successed!\n");
	}

	return 0;
}

static void __exit chrdevbase_exit(void)
{
	unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
	printk("chrdevbase exit!\n");
}

module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ligh");
