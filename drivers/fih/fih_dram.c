#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/uaccess.h>
//#include <mach/msm_iomap.h>
#include <linux/seq_file.h>
#include "fih_ramtable.h"

#define FIH_PROC_DIR   "AllHWList"
#define FIH_PROC_PATH  "AllHWList/draminfo"
//#define FIH_PROC_PATH  "draminfo"
#define FIH_PROC_SIZE  32

#define FIH_MEM_ST_HEAD  0x48464d45  /* HFME */
#define FIH_MEM_ST_TAIL  0x454d4654  /* EMFT */

struct st_fih_mem {
	unsigned int head;
	unsigned int mfr_id;
	unsigned int ddr_type;
	unsigned int size_mb;
	unsigned int tail;
};

struct st_fih_mem dram;

static int fih_proc_read(struct seq_file *m, void *v)
{
	char mfr[16], type[16], size[16];

	switch (dram.mfr_id) {
		case 1: snprintf(mfr, sizeof(mfr), "SAMSUNG"); break;
		case 3: snprintf(mfr, sizeof(mfr), "ELPIDA"); break;
		case 6: snprintf(mfr, sizeof(mfr), "SK-HYNIX"); break;
		default: snprintf(mfr, sizeof(mfr), "UNKNOWN"); break;
	}

	switch (dram.ddr_type) {
		case 2: snprintf(type, sizeof(type), "LPDDR2"); break;
		case 5: snprintf(type, sizeof(type), "LPDDR3"); break;
		case 6: snprintf(type, sizeof(type), "LPDDR4"); break;
		case 7: snprintf(type, sizeof(type), "LPDDR4x"); break;
		default: snprintf(type, sizeof(type), "LPDDRX"); break;
	}

	snprintf(size, sizeof(size), "%dMB", dram.size_mb);

	seq_printf(m, "%8s %7s %6s\n", mfr, type, size);

	return 0;
}

static int fih_ram_proc_read_dram_info(struct inode *inode, struct file *file)
{
	return single_open(file, fih_proc_read, NULL);
}

static const struct file_operations dram_info_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_ram_proc_read_dram_info,
	.read    = seq_read
};

static int __init fih_proc_init(void)
{
	struct st_fih_mem *p;

	/* get dram in mem which lk write */
	p = (struct st_fih_mem *)ioremap(FIH_MEM_MEM_ADDR, sizeof(struct st_fih_mem));
	if (p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		dram.head = FIH_MEM_ST_HEAD;
		dram.mfr_id = 0;
		dram.ddr_type = 0;
		dram.size_mb = 0;
		dram.tail = FIH_MEM_ST_TAIL;
	} else {
		memcpy(&dram, p, sizeof(struct st_fih_mem));
		iounmap(p);
	}

	/* check magic of dram */
	if ((dram.head != FIH_MEM_ST_HEAD)||(dram.tail != FIH_MEM_ST_TAIL)) {
		pr_err("%s: bad magic\n", __func__);
		dram.head = FIH_MEM_ST_HEAD;
		dram.mfr_id = 0;
		dram.ddr_type = 0;
		dram.size_mb = 0;
		dram.tail = FIH_MEM_ST_TAIL;
	}

	if (proc_create(FIH_PROC_PATH, 0, NULL, &dram_info_file_ops) == NULL)
	{
		proc_mkdir(FIH_PROC_DIR, NULL);
		if (proc_create(FIH_PROC_PATH, 0, NULL, &dram_info_file_ops) == NULL)
		{
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
