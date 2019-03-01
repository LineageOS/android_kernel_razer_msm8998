#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/seq_file.h>

#define FIH_BATTERY_DIR "fih_battery"
#define FIH_OVER_CHARGE_PROTECT_PROC_PATH "overChargeProtectEn"
#define FIH_SHOW_BATTERY_INFO_PROC_PATH "showBatteryInfoEn"

int overChargeProtectEn = 1;
int showBatteryInfoEn = 1;

static int overChargeProtect_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, overChargeProtectEn ? "1" : "0");
	return 0;
}

static int showBatteryInfo_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, showBatteryInfoEn ? "1" : "0");
	return 0;
}

static ssize_t showBatteryInfo_proc_write(struct file *file, const char __user *buffer,
        size_t count, loff_t *pos)
{
	char data;
	if (count > 0) {
		if (get_user(data, buffer))
			return -EFAULT;

		if(data == '0')
			showBatteryInfoEn = 0;
		else
			showBatteryInfoEn = 1;
	}

	return count;
}

static ssize_t overChargeProtect_proc_write(struct file *file, const char __user *buffer,
        size_t count, loff_t *pos)
{
	char data;
	if (count > 0) {
		if (get_user(data, buffer))
			return -EFAULT;

		if(data == '0')
			overChargeProtectEn = 0;
		else
			overChargeProtectEn = 1;
	}

	return count;
}

static int showBatteryInfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, showBatteryInfo_proc_show, inode->i_private);
}

static int overChargeProtect_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, overChargeProtect_proc_show, inode->i_private);
}

static const struct file_operations showBatteryInfo_proc_fops = {
	.owner = THIS_MODULE,
	.open  = showBatteryInfo_proc_open,
	.read  = seq_read,
	.write  = showBatteryInfo_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations overChargeProtect_proc_fops = {
	.owner = THIS_MODULE,
	.open  = overChargeProtect_proc_open,
	.read  = seq_read,
	.write  = overChargeProtect_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init fih_battery_init(void)
{
	char file_path[64];
	struct proc_dir_entry *fih_battery_dir = proc_mkdir(FIH_BATTERY_DIR, NULL);

	if(!fih_battery_dir) {
		pr_err("Can't create /proc/%s\n", FIH_BATTERY_DIR);
		return (1);
	}

	sprintf(file_path, "%s/%s", FIH_BATTERY_DIR, FIH_OVER_CHARGE_PROTECT_PROC_PATH);
	if(proc_create(file_path, 0666, NULL, &overChargeProtect_proc_fops) == NULL) {
		pr_err("fail to create proc/%s\n", FIH_OVER_CHARGE_PROTECT_PROC_PATH);
		return (1);
	}

	sprintf(file_path, "%s/%s", FIH_BATTERY_DIR, FIH_SHOW_BATTERY_INFO_PROC_PATH);
	if(proc_create(file_path, 0666, NULL, &showBatteryInfo_proc_fops) == NULL) {
		pr_err("fail to create proc/%s\n", FIH_SHOW_BATTERY_INFO_PROC_PATH);
		return (1);
	}

	return 0;
}

static void __exit fih_battery_exit(void)
{
	printk(KERN_INFO "EXIT fih_battery proc\n");
	remove_proc_entry(FIH_OVER_CHARGE_PROTECT_PROC_PATH, NULL);
	remove_proc_entry(FIH_SHOW_BATTERY_INFO_PROC_PATH, NULL);
}

module_init(fih_battery_init);
module_exit(fih_battery_exit);