#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <fih/hwid.h>
#include <linux/seq_file.h>


static int fih_info_proc_open_project_show(struct seq_file *m, void *v)
{
	char msg[8];

	switch (fih_hwid_fetch(FIH_HWID_PRJ)) {
		case FIH_PRJ_9800: strcpy(msg, "9800"); break;
		case FIH_PRJ_9801: strcpy(msg, "9801"); break;
		case FIH_PRJ_9802: strcpy(msg, "9802"); break;
		case FIH_PRJ_RCL: strcpy(msg, "RCL"); break;
		default: strcpy(msg, "N/A"); break;
	}
	seq_printf(m, "%s\n", msg);

	return 0;
}

static int fih_info_proc_open_hw_rev_show(struct seq_file *m, void *v)
{
	char msg[8];

	switch (fih_hwid_fetch(FIH_HWID_REV)) {
		case FIH_REV_EVB:  strcpy(msg, "EVB"); break;
		case FIH_REV_EVB1:  strcpy(msg, "EVB1"); break;
		case FIH_REV_EVB2:  strcpy(msg, "EVB2"); break;
		case FIH_REV_EVT:  strcpy(msg, "EVT"); break;
		case FIH_REV_EVT0_5: strcpy(msg, "EVT0.5"); break;
		case FIH_REV_EVT1:  strcpy(msg, "EVT1"); break;
		case FIH_REV_EVT1_1:  strcpy(msg, "EVT1_1"); break;
		case FIH_REV_EVT1_2:  strcpy(msg, "EVT1_2"); break;
		case FIH_REV_DVT:  strcpy(msg, "DVT"); break;
		case FIH_REV_DVT1:  strcpy(msg, "DVT1"); break;
		case FIH_REV_DVT1_5:  strcpy(msg, "DVT1.5"); break;
		case FIH_REV_DVT2: strcpy(msg, "DVT2"); break;
		case FIH_REV_PVT:  strcpy(msg, "PVT"); break;
		case FIH_REV_PVT1:  strcpy(msg, "PVT1"); break;
		case FIH_REV_PVT2: strcpy(msg, "PVT2"); break;
		case FIH_REV_MP:   strcpy(msg, "MP"); break;
		case FIH_REV_MP1:   strcpy(msg, "MP1"); break;
		case FIH_REV_MP2:  strcpy(msg, "MP2"); break;
		default: strcpy(msg, "N/A"); break;
	}
	seq_printf(m, "%s\n", msg);

	return 0;
}

static int fih_info_proc_open_rf_band_show(struct seq_file *m, void *v)
{
	char msg[128];

	switch (fih_hwid_fetch(FIH_HWID_RF)) {
		case FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_L_1_2_3_4_5_7_8_20_28_38_40_41:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_5_8^L_1_2_3_4_5_7_8_20_28_38_40_41"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_L_1_2_3_4_5_7_8_20_28_38_40_41_SS:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_5_8^L_1_2_3_4_5_7_8_20_28_38_40_41"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_C_0_T_34_39_L_1_2_3_4_5_7_8_28_38_39_40_41:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_5_8^C_0^T_34_39^L_1_2_3_4_5_7_8_28_38_39_40_41"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_C_0_T_34_39_L_1_2_3_4_5_7_8_12_13_17_20_28_29_30_39_66:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_5_8_C_0^C_0^T_34_39^L_1_2_3_4_5_7_8_12_13_17_20_28_29_30_39_66"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_5_8_C_0_T_34_39_L_1_2_3_4_5_7_8_12_13_17_20_28_29_30_39_66_SS:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_5_8_C_0^C_0^T_34_39^L_1_2_3_4_5_7_8_12_13_17_20_28_29_30_39_66"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_3_4_5_8_T_34_39_L_1_2_3_4_5_7_8_12_17_19_20_28_29_30_38_39_40_41_66:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_3_4_5_8^T_34_39^L_1_2_3_4_5_7_8_12_17_19_20_28_29_30_38_39_40_41_66"); break;
		case FIH_BAND_G_850_900_1800_1900_W_1_2_3_4_5_8_T_34_39_L_1_2_3_4_5_7_8_12_17_19_20_25_26_28_29_30_38_39_40_41_66:
			strcpy(msg, "G_850_900_1800_1900^W_1_2_3_4_5_8^T_34_39^L_1_2_3_4_5_7_8_12_17_19_20_25_26_28_29_30_38_39_40_41_66"); break;
		/* NO BAND */
		case FIH_RF_NONE: strcpy(msg, "NONE"); break;
		default: strcpy(msg, "UNKNOWN\n"); break;
	}
	seq_printf(m, "%s\n", msg);

	return 0;
}

static int fih_info_proc_open_project(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_open_project_show, NULL);
}

static int fih_info_proc_open_hw_rev(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_open_hw_rev_show, NULL);
}

static int fih_info_proc_open_rf_band(struct inode *inode, struct file *file)
{
	return single_open(file, fih_info_proc_open_rf_band_show, NULL);
}

/* This structure gather "function" that manage the /proc file
 */
static const struct file_operations project_file_ops = {
	.owner   = THIS_MODULE,
	.open	 = fih_info_proc_open_project,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static const struct file_operations hw_rev_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_info_proc_open_hw_rev,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static const struct file_operations rf_band_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_info_proc_open_rf_band,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release
};

static int __init fih_info_init(void)
{
	if (proc_create("devmodel", 0, NULL, &project_file_ops) == NULL) {
		pr_err("fail to create proc/devmodel\n");
	}

	if (proc_create("baseband", 0, NULL, &hw_rev_file_ops) == NULL) {
		pr_err("fail to create proc/baseband\n");
	}

	if (proc_create("bandinfo", 0, NULL, &rf_band_file_ops) == NULL) {
		pr_err("fail to create proc/bandinfo\n");
	}

	return (0);
}

static void __exit fih_info_exit(void)
{
	remove_proc_entry("devmodel", NULL);
	remove_proc_entry("baseband", NULL);
	remove_proc_entry("bandinfo", NULL);
}

module_init(fih_info_init);
module_exit(fih_info_exit);
