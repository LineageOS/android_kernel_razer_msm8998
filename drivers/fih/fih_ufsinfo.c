#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <fih/fih_ufsinfo.h>

#define FIH_PROC_DIR   "AllHWList"
#define FIH_PROC_PATH  "AllHWList/ufsinfo"
#define FIH_PROC_SIZE  FIH_UFSINFO_SIZE

static char fih_proc_data[FIH_PROC_SIZE] = "unknown";

void fih_ufsinfo_setup(char *info)
{
	snprintf(fih_proc_data, sizeof(fih_proc_data), "%s", info);
}

static int fih_ufsinfo_proc_show_ufsinfo(struct seq_file *m, void *v)
{
	seq_printf(m, fih_proc_data);

	return 0;
}

static int fih_ufsinfo_proc_open_ufsinfo(struct inode *inode, struct file *file)
{
	return single_open(file, fih_ufsinfo_proc_show_ufsinfo, NULL);
}

static const struct file_operations fih_ufsinfo_proc_fops_ufsinfo = {
	.open    = fih_ufsinfo_proc_open_ufsinfo,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init fih_proc_init(void)
{
	if (proc_create(FIH_PROC_PATH, 0, NULL, &fih_ufsinfo_proc_fops_ufsinfo) == NULL) {
		proc_mkdir(FIH_PROC_DIR, NULL);
		if (proc_create(FIH_PROC_PATH, 0, NULL, &fih_ufsinfo_proc_fops_ufsinfo) == NULL) {
			pr_err("fail to create proc/%s\n", FIH_PROC_PATH);
			return (1);
		}
	}
	return (0);
}

static void __exit fih_proc_exit(void)
{
	remove_proc_entry(FIH_PROC_PATH, NULL);
}

module_init(fih_proc_init);
module_exit(fih_proc_exit);
