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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>


static int __init dtsof_init(void)
{	
	int ret = 0;
	/* 找到lacklight节点 */
	struct device_node *bl_nd = NULL;
	struct property *comppro = NULL;

	bl_nd = of_find_node_by_path("/backlight");
	comppro = of_find_property(bl_nd, "compatible", NULL);
	printk("compatible=%s\r\n", (char*)comppro->value);
	return ret;
}

static void __exit dtsof_exit(void)
{

}

module_init(dtsof_init);
module_exit(dtsof_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("LIGH");