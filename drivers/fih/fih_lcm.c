/*
 * Virtual file interface for FIH LCM, 20151103
 * KuroCHChung@fih-foxconn.com
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#define FIH_PROC_PATH_AOD "AOD"
#define FIH_PROC_PATH_PANEL_ID "PanelID"
#define FIH_PROC_DIR_LCM0 "AllHWList/LCM0"
#define FIH_PROC_PATH_AOD_LP "setlp"
#define FIH_PROC_FULL_PATH_AOD "AllHWList/LCM0/AOD"
#define FIH_PROC_FULL_PATH_PANEL_ID "AllHWList/LCM0/PanelID"
#define FIH_PROC_FULL_PATH_AOD_LP "AllHWList/LCM0/setlp"

#ifdef CONFIG_AOD_FEATURE
extern int fih_get_aod(void);
extern unsigned int fih_get_panel_id(void);
extern int fih_set_aod(int enable);
extern int fih_set_low_power_mode(int enable);
extern int fih_get_low_power_mode(void);
#endif


static int fih_lcm_show_aod_lp_settings(struct seq_file *m, void *v)
{
#ifdef CONFIG_AOD_FEATURE
    seq_printf(m, "%d\n", fih_get_low_power_mode());
#else
	seq_printf(m, "0\n");
#endif
    return 0;
}

static int fih_lcm_open_aod_lp_settings(struct inode *inode, struct  file *file)
{
    return single_open(file, fih_lcm_show_aod_lp_settings, NULL);
}


static ssize_t fih_lcm_write_aod_lp_settings(struct file *file, const char __user *buffer,
                    size_t count, loff_t *offp)
{
    char *buf;
    unsigned int res;

    if (count < 1)
        return -EINVAL;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;
	pr_err("fih_lcm_write_aod_lp_settings\n");

#ifdef CONFIG_AOD_FEATURE
    res = fih_set_low_power_mode(simple_strtoull(buf, NULL, 0));
#endif
    if (res < 0)
    {
        kfree(buf);
        return res;
    }

    kfree(buf);

    /* claim that we wrote everything */
    return count;
}
static struct file_operations aod_lp_file_ops = {
    .owner   = THIS_MODULE,
    .write   = fih_lcm_write_aod_lp_settings,
    .read    = seq_read,
    .open    = fih_lcm_open_aod_lp_settings,
    .release = single_release
};


static int fih_lcm_show_panelid_settings(struct seq_file *m, void *v)
{
#ifdef CONFIG_AOD_FEATURE
    seq_printf(m, "%d\n", fih_get_panel_id());
#else
	seq_printf(m, "0\n");
#endif
    return 0;
}

static int fih_lcm_open_panelid_settings(struct inode *inode, struct  file *file)
{
    return single_open(file, fih_lcm_show_panelid_settings, NULL);
}

static ssize_t fih_lcm_write_panelid_settings(struct file *file, const char __user *buffer,
                    size_t count, loff_t *offp)
{
    return count;
}


static struct file_operations panel_id_file_ops = {
    .owner   = THIS_MODULE,
    .write   = fih_lcm_write_panelid_settings,
    .read    = seq_read,
    .open    = fih_lcm_open_panelid_settings,
    .release = single_release
};

static int fih_lcm_show_aod_settings(struct seq_file *m, void *v)
{
#ifdef CONFIG_AOD_FEATURE
    seq_printf(m, "%d\n", fih_get_aod());
#else
	seq_printf(m, "0\n");
#endif
    return 0;
}

static int fih_lcm_open_aod_settings(struct inode *inode, struct  file *file)
{
    return single_open(file, fih_lcm_show_aod_settings, NULL);
}


static ssize_t fih_lcm_write_aod_settings(struct file *file, const char __user *buffer,
                    size_t count, loff_t *offp)
{
    char *buf;
    unsigned int res;

    if (count < 1)
        return -EINVAL;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;
#ifdef CONFIG_AOD_FEATURE
    res = fih_set_aod(simple_strtoull(buf, NULL, 0));
#endif
    if (res < 0)
    {
        kfree(buf);
        return res;
    }

    kfree(buf);

    /* claim that we wrote everything */
    return count;
}
static struct file_operations aod_file_ops = {
    .owner   = THIS_MODULE,
    .write   = fih_lcm_write_aod_settings,
    .read    = seq_read,
    .open    = fih_lcm_open_aod_settings,
    .release = single_release
};


static int __init fih_proc_init(void)
{
    struct proc_dir_entry *lcm0_dir;
    lcm0_dir = proc_mkdir (FIH_PROC_DIR_LCM0, NULL);

	pr_err("start to create proc/%s\n", FIH_PROC_PATH_AOD);
    if(proc_create(FIH_PROC_PATH_AOD, 0, lcm0_dir, &aod_file_ops) == NULL)
    {
        pr_err("fail to create proc/%s\n", FIH_PROC_PATH_AOD);
        return (1);
    }

	pr_err("start to create proc/%s\n", FIH_PROC_PATH_PANEL_ID);
    if(proc_create(FIH_PROC_PATH_PANEL_ID, 0, lcm0_dir, &panel_id_file_ops) == NULL)
    {
        pr_err("fail to create proc/%s\n", FIH_PROC_PATH_PANEL_ID);
        return (1);
    }
	pr_err("start to create proc/%s\n", FIH_PROC_PATH_AOD_LP);
	if(proc_create(FIH_PROC_PATH_AOD_LP, 0, lcm0_dir, &aod_lp_file_ops) == NULL)
	{
		pr_err("fail to create proc/%s\n", FIH_PROC_PATH_AOD_LP);
		return (1);
	}

    return (0);
}

static void __exit fih_proc_exit(void)
{
    remove_proc_entry(FIH_PROC_FULL_PATH_AOD, NULL);
    remove_proc_entry(FIH_PROC_FULL_PATH_PANEL_ID, NULL);
    remove_proc_entry(FIH_PROC_FULL_PATH_AOD_LP, NULL);
}

module_init(fih_proc_init);
module_exit(fih_proc_exit);
