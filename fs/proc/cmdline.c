#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#ifdef CONFIG_MACH_RCL
#include <asm/setup.h>

static char new_command_line[COMMAND_LINE_SIZE];
#endif

static int cmdline_proc_show(struct seq_file *m, void *v)
{
#ifdef CONFIG_MACH_RCL
	seq_printf(m, "%s\n", new_command_line);
#else
	seq_printf(m, "%s\n", saved_command_line);
#endif
	return 0;
}

static int cmdline_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdline_proc_show, NULL);
}

static const struct file_operations cmdline_proc_fops = {
	.open		= cmdline_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_cmdline_init(void)
{
#ifdef CONFIG_MACH_RCL
	char *offset_addr, *cmd = new_command_line;

	strcpy(cmd, saved_command_line);

	/*
	 * Remove 'androidboot.console' flag from command line seen
	 * by userspace in order to stop console service from crashing.
	 */
	offset_addr = strstr(cmd, "androidboot.console=");
	if (offset_addr) {
		size_t i, len, offset;

		len = strlen(cmd);
		offset = offset_addr - cmd;

		for (i = 1; i < (len - offset); i++) {
			if (cmd[offset + i] == ' ')
				break;
		}

		memmove(offset_addr, &cmd[offset + i + 1], len - i - offset);
	}
#endif

	proc_create("cmdline", 0, NULL, &cmdline_proc_fops);
	return 0;
}
fs_initcall(proc_cmdline_init);
