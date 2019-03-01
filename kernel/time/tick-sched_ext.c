/*
 * Copyright(C) 2011-2017 Foxconn International Holdings, Ltd. All rights reserved.
 * Copyright (C) 2011 Foxconn.  All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include <linux/cpu.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/percpu.h>
#include <linux/profile.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/rq_stats.h>

#include <asm/irq_regs.h>

#include "tick-internal.h"

#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
/* for linux pt_regs struct */
#include <linux/ptrace.h>
#include <linux/thermal.h>

/* total cpu number*/
#ifdef CONFIG_NR_CPUS
#define CPU_NUMS CONFIG_NR_CPUS
#else
#define CPU_NUMS 8
#endif

u64 Last_checked_jiffies = 0;

u64 LastCheckedTime[CPU_NUMS] = {0};
u64 LastBusyTime[CPU_NUMS]  = {0};

u64 LastIdleTime[CPU_NUMS]  = {0};
u64 LastIoWaitTime[CPU_NUMS]  = {0};

unsigned int debug_cpu_usage_enable = 0;

#define CPU_USAGE_CHECK_INTERVAL_MS 2000  /* 2000ms */ 

unsigned int debug_cpu_usage_interval = CPU_USAGE_CHECK_INTERVAL_MS;

long LastCpuUsage[CPU_NUMS]  = {0};
long LastIowaitUsage[CPU_NUMS]  = {0};

#include <linux/workqueue.h>
struct delayed_work		update_tz_info_work;
static int need_init_tz_info_work = 1;

#define TZ_INFO_INTERVAL_MS	10000 /* 10s */

#define ARM_TZ_INFO_WORK_CNT (TZ_INFO_INTERVAL_MS/CPU_USAGE_CHECK_INTERVAL_MS)

typedef struct 
{ 
  char *alias; 
  char *type; 
  int divide; 
} __SENSOR_NAME__; 

#if defined(CONFIG_FIH_9801) || defined(CONFIG_FIH_9802)
    __SENSOR_NAME__ fih_therm_sensor_list[] = 
            { {"CPU0", "tsens_tz_sensor1", 10},
              {"CPU1", "tsens_tz_sensor2", 10},
              {"CPU2", "tsens_tz_sensor3", 10},
              {"CPU3", "tsens_tz_sensor4", 10},
              {"CPU4", "tsens_tz_sensor7", 10},
              {"CPU5", "tsens_tz_sensor8", 10},
              {"CPU6", "tsens_tz_sensor9", 10},
              {"CPU7", "tsens_tz_sensor10", 10},
              {"GPU", "tsens_tz_sensor12", 10},
              {"DSP", "tsens_tz_sensor15", 10},
              {"WLAN", "tsens_tz_sensor16", 10},
              {"CAMERA", "tsens_tz_sensor18", 10},
              {"VIDEO", "tsens_tz_sensor19", 10},
              {"MODEM", "tsens_tz_sensor20", 10},
              {"QUIET", "quiet_therm", 1},
              {"BAT", "battery", 1000},
              {"PM8998", "pm8998_tz", 1000},
              {"PMI8998", "pmi8998_tz", 1000},
              {"PM8005", "pm8005_tz", 1000},
              {"XO", "xo_therm", 1},
#if defined(CONFIG_FIH_9801)
              {"PA0", "pa_therm0", 1},
              {"PA1", "pa_therm1", 1}, 
#endif
            };
#else
    __SENSOR_NAME__ fih_therm_sensor_list[] = 
            { {"QUIET", "quiet_therm", 1},
              {"BAT", "battery", 1000},
              {"XO", "xo_therm", 1},
              {"PA0", "pa_therm0", 1},
              {"PA1", "pa_therm1", 1}, 
            };
#endif

//extern void cluster_actual_freq_get_all(u64 *val_pwr, u64 *val_perf);

static void update_tz_info(struct work_struct *work)
{
    char buf[250];
    char *s = buf;
    int i =0;
    struct thermal_zone_device *tzd;
    int temperature,ret;
//    u64 pwr_clk, perf_clk;

    buf[0] = '\0';
    for(i = 0; i < ARRAY_SIZE(fih_therm_sensor_list); i++){
        tzd = thermal_zone_get_zone_by_name(fih_therm_sensor_list[i].type);

        if (!IS_ERR(tzd)) {
            ret = thermal_zone_get_temp(tzd, &temperature);
            
            if (!ret) {
                temperature /= fih_therm_sensor_list[i].divide;
                s += snprintf(s, sizeof(buf) - (size_t)(s-buf), "%s=%d ", 
                        fih_therm_sensor_list[i].alias, temperature);
            }
        }
    }

//    cluster_actual_freq_get_all(&pwr_clk, &perf_clk);
//    printk(KERN_INFO "TINFO:%spwrclk=%llu perfclk=%llu\n", buf, pwr_clk, perf_clk);
    printk(KERN_INFO "TINFO:%s\n", buf);
}

long get_cpu_usage(int cpu, long*  IoWaitUsage)
{
	u64 CurrTime = 0;
	u64 CurrBusyTime = 0;
	u64 CurrIoWaitTime = 0;
	
	long TotalTickCount = 0;
	long BusyTickCount = 0;
	long IoWaitTickCount = 0;

	long Usage = 0;
	
	*IoWaitUsage = 0;

	if(cpu >= CPU_NUMS) {
		return Usage;
	}
	
	/* get the current time */
	CurrTime = jiffies_to_usecs(jiffies_64);

	/* get this cpu's busy time */
	CurrBusyTime  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	CurrBusyTime += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	CurrBusyTime += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	CurrBusyTime += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	CurrBusyTime += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	CurrBusyTime += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	CurrBusyTime =  jiffies_to_usecs(CurrBusyTime);
	CurrIoWaitTime =  jiffies_to_usecs(kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT]);
		
	/* Calculate the time interval */
	TotalTickCount = CurrTime - LastCheckedTime[cpu];
	BusyTickCount = CurrBusyTime - LastBusyTime[cpu];
	IoWaitTickCount = CurrIoWaitTime - LastIoWaitTime[cpu];

	/* record last current and busy time */
	LastBusyTime[cpu] = CurrBusyTime;
	LastCheckedTime[cpu] = CurrTime;
	LastIoWaitTime[cpu] = CurrIoWaitTime;

	Last_checked_jiffies = jiffies_64;
	
	/* Calculate the busy rate */
	if (TotalTickCount >= BusyTickCount && TotalTickCount > 0) {
		BusyTickCount = 100 * BusyTickCount;
		do_div(BusyTickCount, TotalTickCount);
		Usage = BusyTickCount;

		IoWaitTickCount = 100 * IoWaitTickCount;
		do_div(IoWaitTickCount, TotalTickCount);
		*IoWaitUsage = IoWaitTickCount;
	}
	else {
		Usage = 0;
	}

	return Usage;
}

#define MAX_REG_LOOP_CHAR	512
static int param_get_cpu_usage(char *buf, const struct kernel_param *kp)
{
	int cpu = 0, count = 0, total_count = 0;
	for_each_present_cpu(cpu) {
		count = snprintf(buf + total_count, MAX_REG_LOOP_CHAR - total_count, 
			"CPU%d%s:%3ld%%,IOW=%3ld%%\n", 
			cpu, 
			(cpu_is_offline(cpu) ? "(off)" : "(on) "), 
			LastCpuUsage[cpu], 
			LastIowaitUsage[cpu]);
		
		total_count += count;
	}
	
	return total_count;
}
module_param_call(cpu_usage, NULL, param_get_cpu_usage, NULL, 0644);

static int debug_cpu_usage_enable_set(const char *val, struct kernel_param *kp)
{
	return param_set_int(val, kp);
}

module_param_call(debug_cpu_usage_enable, debug_cpu_usage_enable_set, param_get_int, &debug_cpu_usage_enable, 0644);

static int debug_cpu_usage_interval_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;
	
	if (debug_cpu_usage_interval<1000) {
		debug_cpu_usage_interval = 1000;
	}
	return ret;
}

module_param_call(debug_cpu_usage_interval, debug_cpu_usage_interval_set, param_get_int, &debug_cpu_usage_interval, 0644);

extern void cpufreq_quick_get_infos(unsigned int cpu, unsigned int *min, unsigned int *max, unsigned int *cur);
extern void kgsl_pwr_quick_get_infos(unsigned int *min, unsigned int *max, unsigned *curr);
extern void quick_get_cooling_device_freq(unsigned int *curr);
/*
* cpu_info_msg: output message
* msg_len: the size of output message.
* output message is included cpu's usage, real cpu frequecy and cpu governor's current cpu frequency.
*/
static void show_cpu_usage_and_freq(char * cpu_info_msg, int msg_len)
{	
	
	unsigned int cpu_freq_min = 0;
	unsigned int cpu_freq_max = 0;
	unsigned int cpu_curr_freq = 0;
	unsigned int gpu_freq_min = 0;
	unsigned int gpu_freq_max = 0;
	unsigned int gpu_curr_freq = 0;
	unsigned int cooling_device[2] = {0, 0};
	long cpu_usage = 0;

	long iowait_usage = 0;

	int cpu;

	int len = msg_len; 
	int str_len = 0;
	int tmp_len = 0;

	if(!cpu_info_msg || msg_len <= 0) {
		return;
	}

	/*CPU0 Frequency*/
	cpufreq_quick_get_infos(0, &cpu_freq_min, &cpu_freq_max, &cpu_curr_freq);
	cpu_freq_max /= 1000;
	cpu_freq_min /= 1000;
	cpu_curr_freq /= 1000; /*cpu governor's current cpu frequency*/
	tmp_len = snprintf(cpu_info_msg, len, "FREQQ:[CPU0 min=%u max=%u curr=%u]",cpu_freq_min, cpu_freq_max, cpu_curr_freq);	
	str_len += tmp_len;
	len -= tmp_len;

	/*CPU4 Frequency*/
	cpufreq_quick_get_infos(4, &cpu_freq_min, &cpu_freq_max, &cpu_curr_freq);
	cpu_freq_max /= 1000;
	cpu_freq_min /= 1000;
	cpu_curr_freq /= 1000; /*cpu governor's current cpu frequency*/
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[CPU4 min=%u max=%u curr=%u]",cpu_freq_min, cpu_freq_max, cpu_curr_freq);	
	str_len += tmp_len;
	len -= tmp_len;

	quick_get_cooling_device_freq(cooling_device);
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[COOL0=%u][COOL1=%u]",cooling_device[0]/1000, cooling_device[1]/1000);	
	str_len += tmp_len;
	len -= tmp_len;

	/*GPU Frequency*/
	kgsl_pwr_quick_get_infos(&gpu_freq_min, &gpu_freq_max, &gpu_curr_freq);
	tmp_len = snprintf((cpu_info_msg + str_len), len, "[GPU min=%u max=%u curr=%u]\nCPUSG:",gpu_freq_min, gpu_freq_max, gpu_curr_freq);	
	str_len += tmp_len;
	len -= tmp_len;

	for_each_present_cpu(cpu) {
		cpu_usage = get_cpu_usage(cpu, &iowait_usage); /*get cpu usage*/

		LastCpuUsage[cpu]  = cpu_usage;
		LastIowaitUsage[cpu]  = iowait_usage;

		tmp_len = snprintf((cpu_info_msg + str_len), len, "[C%d%s:%3ld%% IOW=%3ld%%]", cpu, (cpu_is_offline(cpu) ? " xfx" : ""), cpu_usage, iowait_usage);	
		
		str_len += tmp_len;
		len -= tmp_len;
		
		if(len <= 0 || str_len >= msg_len) {
			break;
		}
	}

	if(len > 0 && str_len < msg_len) {
		snprintf((cpu_info_msg + str_len), len, "C%d:", smp_processor_id()); /*this cpu is handling this timer interrupt*/
	}
}

void count_cpu_time(void)
{
	
	if (unlikely(debug_cpu_usage_enable && (1000*(jiffies_64 - Last_checked_jiffies)/HZ >= debug_cpu_usage_interval))) {
		struct task_struct * p = current;
		struct pt_regs *regs = get_irq_regs();
		
		char cpu_info_msg[512] = {0};
		int len = sizeof(cpu_info_msg);
		
		show_cpu_usage_and_freq(cpu_info_msg, len);

		if (likely(p != 0)) {
			if (regs != 0) {
				if (regs->pc <= TASK_SIZE) { /* User space */
					struct mm_struct *mm = p->mm;
					struct vm_area_struct *vma;
					struct file *map_file = NULL;

					/* Parse vma information */
					vma = find_vma(mm, regs->pc);
					if (vma != NULL) {
						map_file = vma->vm_file;
					
						if (map_file) {  /* memory-mapped file */
							printk(KERN_INFO "%sLR=0x%08llx PC=0x%08llx[U][%4d][%s][%s+0x%lx]\r\n",
								cpu_info_msg,
								regs->compat_lr, 
								regs->pc, 
								(unsigned int) p->pid,
								p->comm,
								map_file->f_path.dentry->d_iname, 
								(unsigned long)regs->pc - vma->vm_start);
						}
						else { 
							const char *name = arch_vma_name(vma);
							if (!name) { 
								if (mm) { 
									if (vma->vm_start <= mm->start_brk &&
										vma->vm_end >= mm->brk) { 
										name = "heap";
									} 
									else if (vma->vm_start <= mm->start_stack &&
										vma->vm_end >= mm->start_stack) { 
										name = "stack";
									}
								}
								else { 
									name = "vdso";
								}
							}
							printk(KERN_INFO "%sLR=0x%08llx PC=0x%08llx[U][%4d][%s][%s]\r\n",
								cpu_info_msg,
								regs->compat_lr, 
								regs->pc, 
								(unsigned int) p->pid,
								p->comm, 
								name);
						}
					}
				}
				else { /* Kernel space */
					printk(KERN_INFO "%sLR=0x%08llx PC=0x%08llx[K][%4d][%s]", 
						cpu_info_msg,
						regs->regs[30], 
						regs->pc, 
						(unsigned int) p->pid,
						p->comm);
					
					#ifdef CONFIG_KALLSYMS
					print_symbol("[%s]\r\n", regs->pc);
					#else
					printk(KERN_INFO "\r\n"); 
					#endif
				}
			}
			else { /* Can't get PC & RA address */
				printk(KERN_INFO "%s[%s]\r\n", 
					cpu_info_msg, 
					p->comm);
			}
		}
		else { /* Can't get process information */
			printk(KERN_INFO "%sERROR: p=0x%08lx regs=0x%08lx\r\n", 
				cpu_info_msg, 
				(long)p, 
				(long)regs);
		}
		if (debug_cpu_usage_enable & 2) {
            static int times = 0;
            
            // echo 2 > /sys/module/tick_sched_ext/parameters/debug_cpu_usage_enable
            if (need_init_tz_info_work) {
                INIT_DELAYED_WORK(&update_tz_info_work, update_tz_info);
                need_init_tz_info_work = 0;
            }

            if ((times % ARM_TZ_INFO_WORK_CNT) == 0) {
                // schedule next queue
                schedule_delayed_work(&update_tz_info_work,
                          round_jiffies_relative(msecs_to_jiffies(0)));
                times = 0;
            }
            
            times ++;
		}
	}

}
