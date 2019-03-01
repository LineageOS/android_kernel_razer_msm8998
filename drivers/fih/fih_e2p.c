#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <fih/share/e2p.h>
#include <linux/seq_file.h>
#include "fih_ramtable.h"

#define FIH_E2P_DATA_ST  FIH_CSP_E2P_V02

static int fih_e2p_proc_read_pid_show(struct seq_file *m, void *v)
{
	FIH_E2P_DATA_ST *e2p;

	e2p = (FIH_E2P_DATA_ST *)ioremap(FIH_E2P_ST_ADDR, sizeof(FIH_E2P_DATA_ST));
	if (e2p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	seq_printf(m, "%s\n", e2p->pid);
	iounmap(e2p);

	return 0;
}

static int fih_e2p_proc_read_bt_mac_show(struct seq_file *m, void *v)
{
	FIH_E2P_DATA_ST *e2p;

	e2p = (FIH_E2P_DATA_ST *)ioremap(FIH_E2P_ST_ADDR, sizeof(FIH_E2P_DATA_ST));
	if (e2p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	seq_printf(m, "%02X:%02X:%02X:%02X:%02X:%02X\n",
		e2p->bt_mac[0], e2p->bt_mac[1], e2p->bt_mac[2], e2p->bt_mac[3], e2p->bt_mac[4], e2p->bt_mac[5]);

	iounmap(e2p);

	return 0;
}

static int fih_e2p_proc_read_wifi_mac_show(struct seq_file *m, void *v)
{
	FIH_E2P_DATA_ST *e2p;

	e2p = (FIH_E2P_DATA_ST *)ioremap(FIH_E2P_ST_ADDR, sizeof(FIH_E2P_DATA_ST));
	if (e2p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	seq_printf(m, "%02X:%02X:%02X:%02X:%02X:%02X\n",
		e2p->wifi_mac[0], e2p->wifi_mac[1], e2p->wifi_mac[2], e2p->wifi_mac[3], e2p->wifi_mac[4], e2p->wifi_mac[5]);
	iounmap(e2p);

	return 0;
}
static int fih_e2p_proc_read_wifi_mac2_show(struct seq_file *m, void *v)
{
	FIH_E2P_DATA_ST *e2p;

	e2p = (FIH_E2P_DATA_ST *)ioremap(FIH_E2P_ST_ADDR, sizeof(FIH_E2P_DATA_ST));
	if (e2p == NULL) {
		pr_err("%s: ioremap fail\n", __func__);
		return (0);
	}

	seq_printf(m, "%02X:%02X:%02X:%02X:%02X:%02X\n",
		e2p->wifi_mac2[0], e2p->wifi_mac2[1], e2p->wifi_mac2[2], e2p->wifi_mac2[3], e2p->wifi_mac2[4], e2p->wifi_mac2[5]);
	iounmap(e2p);

	return 0;
}


static int fih_e2p_proc_read_pid(struct inode *inode, struct file *file)
{
	return single_open(file, fih_e2p_proc_read_pid_show, NULL);
}

static int fih_e2p_proc_read_bt_mac(struct inode *inode, struct file *file)
{
	return single_open(file, fih_e2p_proc_read_bt_mac_show, NULL);
}

static int fih_e2p_proc_read_wifi_mac(struct inode *inode, struct file *file)
{
	return single_open(file, fih_e2p_proc_read_wifi_mac_show, NULL);
}

static int fih_e2p_proc_read_wifi_mac2(struct inode *inode, struct file *file)
{
	return single_open(file, fih_e2p_proc_read_wifi_mac2_show, NULL);
}


/* This structure gather "function" that manage the /proc file
 */
static const struct file_operations pid_file_ops = {
	.owner   = THIS_MODULE,
	.open	 = fih_e2p_proc_read_pid,
	.read    = seq_read
};

static const struct file_operations bt_mac_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_e2p_proc_read_bt_mac,
	.read    = seq_read
};

static const struct file_operations wifi_mac_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_e2p_proc_read_wifi_mac,
	.read    = seq_read
};
static const struct file_operations wifi_mac2_file_ops = {
	.owner   = THIS_MODULE,
	.open    = fih_e2p_proc_read_wifi_mac2,
	.read    = seq_read
};


static int __init fih_e2p_init(void)
{
	if (FIH_E2P_ST_SIZE < sizeof(FIH_E2P_DATA_ST)) {
		pr_err("%s: WARNNING!! FIH_E2P_ST_SIZE (%d) < sizeof(FIH_E2P_DATA_ST) (%lu)",
			__func__, FIH_E2P_ST_SIZE, sizeof(FIH_E2P_DATA_ST));
		return (1);
	}

	if (proc_create("productid", 0, NULL, &pid_file_ops) == NULL) {
		pr_err("fail to create proc/productid\n");
	}

	if (proc_create("bt_mac", 0, NULL, &bt_mac_file_ops) == NULL) {
		pr_err("fail to create proc/bt_mac\n");
	}

	if (proc_create("wifi_mac", 0, NULL, &wifi_mac_file_ops) == NULL) {
		pr_err("fail to create proc/wifi_mac\n");
	}

	if (proc_create("wifi_mac2", 0, NULL, &wifi_mac2_file_ops) == NULL) {
		pr_err("fail to create proc/wifi_mac\n");
	}

	return (0);
}

static void __exit fih_e2p_exit(void)
{
	remove_proc_entry("productid", NULL);
	remove_proc_entry("bt_mac", NULL);
	remove_proc_entry("wifi_mac", NULL);
}

module_init(fih_e2p_init);
module_exit(fih_e2p_exit);
