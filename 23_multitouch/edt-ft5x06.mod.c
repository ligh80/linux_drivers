#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xfa985410, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x823d1f48, __VMLINUX_SYMBOL_STR(simple_open) },
	{ 0x921f2782, __VMLINUX_SYMBOL_STR(simple_attr_release) },
	{ 0x55c87040, __VMLINUX_SYMBOL_STR(simple_attr_write) },
	{ 0x33b3e16f, __VMLINUX_SYMBOL_STR(simple_attr_read) },
	{ 0xfb7851ba, __VMLINUX_SYMBOL_STR(generic_file_llseek) },
	{ 0xab5c8cb5, __VMLINUX_SYMBOL_STR(i2c_del_driver) },
	{ 0x19bdcb34, __VMLINUX_SYMBOL_STR(i2c_register_driver) },
	{ 0x6ab75d25, __VMLINUX_SYMBOL_STR(touchscreen_parse_of_params) },
	{ 0xe148dc62, __VMLINUX_SYMBOL_STR(device_init_wakeup) },
	{ 0xf130fc0c, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0xb599545a, __VMLINUX_SYMBOL_STR(debugfs_create_u16) },
	{ 0xc47b6521, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0x64b5d4a1, __VMLINUX_SYMBOL_STR(dev_driver_string) },
	{ 0xb81960ca, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0xae372752, __VMLINUX_SYMBOL_STR(of_property_read_u32_array) },
	{ 0xde6fb46c, __VMLINUX_SYMBOL_STR(of_get_named_gpio_flags) },
	{ 0x389329b3, __VMLINUX_SYMBOL_STR(gpiod_set_raw_value) },
	{ 0x1352f55c, __VMLINUX_SYMBOL_STR(devm_gpio_request_one) },
	{ 0xe3c07ed7, __VMLINUX_SYMBOL_STR(input_register_device) },
	{ 0xdd7ab7ec, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0x2770f6b8, __VMLINUX_SYMBOL_STR(devm_request_threaded_irq) },
	{ 0xb7cfa4a8, __VMLINUX_SYMBOL_STR(gpiod_direction_input) },
	{ 0x3089e8ae, __VMLINUX_SYMBOL_STR(gpio_to_desc) },
	{ 0x47229b5c, __VMLINUX_SYMBOL_STR(gpio_request) },
	{ 0xbedc9d6d, __VMLINUX_SYMBOL_STR(input_mt_init_slots) },
	{ 0x1e8b2bb, __VMLINUX_SYMBOL_STR(input_set_abs_params) },
	{ 0x73e20c1c, __VMLINUX_SYMBOL_STR(strlcpy) },
	{ 0x349cba85, __VMLINUX_SYMBOL_STR(strchr) },
	{ 0x2aa0e4fc, __VMLINUX_SYMBOL_STR(strncasecmp) },
	{ 0xf3bb59b5, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x34fff4c2, __VMLINUX_SYMBOL_STR(devm_input_allocate_device) },
	{ 0x93e3940, __VMLINUX_SYMBOL_STR(devm_kmalloc) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xfcec0987, __VMLINUX_SYMBOL_STR(enable_irq) },
	{ 0x3ce4ca6f, __VMLINUX_SYMBOL_STR(disable_irq) },
	{ 0x8e865d3c, __VMLINUX_SYMBOL_STR(arm_delay_ops) },
	{ 0x67c2fa54, __VMLINUX_SYMBOL_STR(__copy_to_user) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0xa46f2f1b, __VMLINUX_SYMBOL_STR(kstrtouint) },
	{ 0xce2840e7, __VMLINUX_SYMBOL_STR(irq_set_irq_wake) },
	{ 0x5fe64003, __VMLINUX_SYMBOL_STR(simple_attr_open) },
	{ 0x72350130, __VMLINUX_SYMBOL_STR(___ratelimit) },
	{ 0x1005a311, __VMLINUX_SYMBOL_STR(input_mt_report_pointer_emulation) },
	{ 0xddaa6aba, __VMLINUX_SYMBOL_STR(input_mt_report_slot_state) },
	{ 0x8e117d54, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0x382aef34, __VMLINUX_SYMBOL_STR(sysfs_remove_group) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x72fabb96, __VMLINUX_SYMBOL_STR(debugfs_remove_recursive) },
	{ 0x49fcab7e, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xf9e73082, __VMLINUX_SYMBOL_STR(scnprintf) },
	{ 0x226412e2, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0x7267eeee, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x59175f83, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x243f207d, __VMLINUX_SYMBOL_STR(i2c_transfer) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Cedt,edt-ft5206*");
MODULE_ALIAS("of:N*T*Cedt,edt-ft5306*");
MODULE_ALIAS("of:N*T*Cedt,edt-ft5406*");
MODULE_ALIAS("i2c:edt-ft5x06");

MODULE_INFO(srcversion, "6B39F7D000B87273AE1EFB1");
