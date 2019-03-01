#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include "fih_secboot.h"
#include <linux/seq_file.h>
#include "fih_ramtable.h"

#define SECBOOT_ENABLED_MEM_ADDR  0x00780350  //Reference 8998_fuseblower_OEM.xml
#define SECBOOT_ENABLED_MEM_MASK  0x00303030
#define SECBOOT_ENABLED_MEM_VALUE  0x00303030 //jason add for get fused status

#define SECBIIT_SERIAL_NO_ADDR  0x00786134 //SERIAL_NUM in 80-P2169-2X rev.C
//#define SECBIIT_SERIAL_NO_ADDR  0x00784138 //SERIAL_NUM form QCT uart log

/* reference from
  ./LINUX/android/bootable/bootloader/lk/app/aboot/devinfo.h */

#define DEVICE_MAGIC "ANDROID-BOOT!"
#define DEVICE_MAGIC_SIZE 13

struct device_info
{
	unsigned char magic[DEVICE_MAGIC_SIZE];
	unsigned char is_unlocked;
	unsigned char is_tampered;
	unsigned char charger_screen_enabled;
};

static int proc_read_enabled_state(struct seq_file *m, void *v)
{
	unsigned int *addr = ioremap(SECBOOT_ENABLED_MEM_ADDR, sizeof(unsigned int));

	if (NULL == addr)
		seq_printf(m, "0\n"); /* Disabled */
	else {
		seq_printf(m, "%d\n", (SECBOOT_ENABLED_MEM_VALUE == ((*addr) & SECBOOT_ENABLED_MEM_MASK)? 1:0));
		iounmap(addr);
	}

	return 0;
}

static int secboot_enabled_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_enabled_state, NULL);
};

static struct file_operations secboot_enabled_file_ops = {
	.owner   = THIS_MODULE,
	.open    = secboot_enabled_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int proc_read_unlocked_state(struct seq_file *m, void *v)
{
	struct device_info *info;

	info = (struct device_info *)ioremap(FIH_SECBOOT_DEVINFO_BASE, sizeof(struct device_info));
	if (info == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	if (memcmp(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE)) {
		pr_err("%s: bad magic = (%s)\n", __func__, info->magic);
		seq_printf(m, "1\n"); /* Unlocked */
	} else {
		seq_printf(m, "%d\n", info->is_unlocked);
	}

	iounmap(info);

	return 0;
}

static int secboot_unlocked_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_unlocked_state, NULL);
};

static struct file_operations secboot_unlocked_file_ops = {
	.owner   = THIS_MODULE,
	.open    = secboot_unlocked_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int proc_read_tampered_state(struct seq_file *m, void *v)
{
	struct device_info *info;

	info = (struct device_info *)ioremap(FIH_SECBOOT_DEVINFO_BASE, sizeof(struct device_info));
	if (info == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	if (memcmp(info->magic, DEVICE_MAGIC, DEVICE_MAGIC_SIZE)) {
		pr_err("%s: bad magic = (%s)\n", __func__, info->magic);
		seq_printf(m, "1\n"); /* Tampered */
	} else {
		seq_printf(m, "%d\n", info->is_tampered);
	}

	iounmap(info);

	return 0;
}

static int secboot_tampered_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_tampered_state, NULL);
};

static struct file_operations secboot_tampered_file_ops = {
	.owner   = THIS_MODULE,
	.open    = secboot_tampered_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int proc_read_serialno_state(struct seq_file *m, void *v)
{
	unsigned int *addr = ioremap(SECBIIT_SERIAL_NO_ADDR, sizeof(unsigned int));

	if (NULL == addr)
		seq_printf(m, "00000000\n");
	else {
		seq_printf(m, "%08X\n", *addr);
		iounmap(addr);
	}

	return 0;
}

static int secboot_serialno_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read_serialno_state, NULL);
};

static struct file_operations secboot_serialno_file_ops = {
	.owner   = THIS_MODULE,
	.open    = secboot_serialno_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};


static struct {
		char *name;
		struct file_operations *ops;
} *p, fih_secboot[] = {
	{"secboot/enabled", &secboot_enabled_file_ops},
	{"secboot/unlocked", &secboot_unlocked_file_ops},
	{"secboot/tampered", &secboot_tampered_file_ops},
	{"secboot/serialno", &secboot_serialno_file_ops},
	{NULL}, };


static int __init fih_secboot_init(void)
{
	proc_mkdir("secboot", NULL);
	for (p = fih_secboot; p->name; p++) {
		if (proc_create(p->name, 0, NULL, p->ops) == NULL) {
			pr_err("%s: fail to create proc/%s\n", __func__, p->name);
		}
	}

	return (0);
}

static void __exit fih_secboot_exit(void)
{
	for (p = fih_secboot; p->name; p++) {
		remove_proc_entry(p->name, NULL);
	}
}

late_initcall(fih_secboot_init);
module_exit(fih_secboot_exit);

/******************************************************************************
 *     FUNCTION
 *****************************************************************************/

int fih_secboot_unlock(const char *cmd)
{
	const char *key = NULL;
	char *p;
	char sec_key[FIH_SECBOOT_UNLOCK_SIZE];

	if (cmd == NULL) {
		pr_err("%s: cmd is NULL\n", __func__);
		return (1);
	}
	pr_info("%s: cmd = (%s)\n", __func__, cmd);

	if (strlen(cmd) <= strlen("unlock ")) {
		pr_err("%s: key is none\n", __func__);
		return (2);
	}

	if (!strncmp(cmd, "unlock ", sizeof("unlock "))) {
		pr_err("%s: bad command: %s\n", __func__, cmd);
		return (3);
	}

	key = cmd + strlen("unlock ");
	if (!key) {
		pr_err("%s: key is NULL\n", __func__);
		return (4);
	}

	if (strlen(key) > FIH_SECBOOT_UNLOCK_SIZE) {
		pr_err("%s: key is too long\n", __func__);
		return (5);
	}

	p = (char *)ioremap(FIH_SECBOOT_UNLOCK_BASE, FIH_SECBOOT_UNLOCK_SIZE);
	if (p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (6);
	}

	pr_info("%s: key = (%s)\n", __func__, key);
	memset(sec_key, 0x0, FIH_SECBOOT_UNLOCK_SIZE);
	memcpy(p, sec_key, FIH_SECBOOT_UNLOCK_SIZE);
	
	memcpy(sec_key, key, strlen(key));
	memcpy(p, sec_key, FIH_SECBOOT_UNLOCK_SIZE);
	//memset(p, 0x0, FIH_SECBOOT_UNLOCK_SIZE);
	//memcpy(p, key, strlen(key));

	iounmap(p);

	return (0);
}
