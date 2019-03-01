/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "QCOM-STEPCHG: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic-voter.h>
#include "step-chg-jeita.h"

#define MAX_STEP_CHG_ENTRIES	8
#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"
/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
#define FIH_WLC_VOTER "FIH_WLC_VOTER"
/* end 9802-799 */

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

struct range_data {
	u32 low_threshold;
	u32 high_threshold;
	u32 value;
};

struct step_chg_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
struct wlc_fcc_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
};
/* end 9802-799 */

struct jeita_fcc_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fv_cfg {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	struct range_data	fv_cfg[MAX_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	ktime_t			step_last_update_time;
	ktime_t			jeita_last_update_time;
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	ktime_t			wlc_last_update_time;
	/* end 9802-799 */
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			step_index;
	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	int			last_jeita_status;
	/* end 9801-1214 */
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	int			wlc_fcc_index;
	bool 		fih_wlc_fcc_en;
	/* end 9802-799 */

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	struct power_supply	*dc_psy;
	/* end 9802-799 */
	struct delayed_work	status_change_work;
	struct notifier_block	nb;
};

static struct step_chg_info *the_chip;

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

/*
 * Step Charging Configuration
 * Update the table based on the battery profile
 * Supports VBATT and SOC based source
 * range data must be in increasing ranges and shouldn't overlap
 */
static struct step_chg_cfg step_chg_config = {
/*
 *	.psy_prop	= POWER_SUPPLY_PROP_VOLTAGE_NOW,
 *	.prop_name	= "VBATT",
 *	.hysteresis	= 100000, // 100mV
 *	.fcc_cfg	= {
 *		// VBAT_LOW	VBAT_HIGH	FCC
 *		{3600000,	4000000,	3000000},
 *		{4001000,	4200000,	2800000},
 *		{4201000,	4400000,	2000000},
 *	},
*/

	//SOC STEP-CHG configuration example.
	.psy_prop = POWER_SUPPLY_PROP_CAPACITY,
	.prop_name = "SOC",
	.fcc_cfg	= {
		//SOC_LOW	SOC_HIGH	FCC
		{0,		25,		3900000},
		{26,		100,		1900000},
	},
};

/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
/*
 * Because wireless charging generate more power dissipation, the battery
 * temperature would be raise quickly. We need to use another charging
 * strategy to avoid it
 */
static struct wlc_fcc_cfg wlc_fcc_config = {
	.psy_prop	= POWER_SUPPLY_PROP_TEMP,
	.prop_name	= "BATT_TEMP",
	.hysteresis	= 10, /* 1degC hysteresis */
	.fcc_cfg	= {
		/* TEMP_LOW	TEMP_HIGH	FCC */
		{0,		       349,		3100000},
		{350,		379,		1000000},
		{380,		409,		700000},
		{410,		549,		400000},
	},
};
/* end 9802-799 */

/*
 * Jeita Charging Configuration
 * Update the table based on the battery profile
 * Please ensure that the TEMP ranges are programmed in the hw so that
 * an interrupt is issued and a consequent psy changed will cause us to
 * react immediately.
 * range data must be in increasing ranges and shouldn't overlap.
 * Gaps are okay
 */
static struct jeita_fcc_cfg jeita_fcc_config = {
	.psy_prop	= POWER_SUPPLY_PROP_TEMP,
	.prop_name	= "BATT_TEMP",
	.hysteresis	= 30, /* 3degC hysteresis */
	.fcc_cfg	= {
		/* TEMP_LOW	TEMP_HIGH	FCC */
		{0,		       159,		750000},
		{160,		449,		3900000},
		{450,		549,		1900000},
	},
};

static struct jeita_fv_cfg jeita_fv_config = {
	.psy_prop	= POWER_SUPPLY_PROP_TEMP,
	.prop_name	= "BATT_TEMP",
	.hysteresis	= 30, /* 3degC hysteresis */
	.fv_cfg		= {
		/* TEMP_LOW	TEMP_HIGH	FV */
		{0,		       149,		4400000},
		{150,		449,		4400000},
		{450,		549,		4100000},
	},
};

static bool is_batt_available(struct step_chg_info *chip)
{
	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy)
		return false;

	return true;
}

/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
static bool is_dc_available(struct step_chg_info *chip)
{
	if (!chip->dc_psy)
		chip->dc_psy = power_supply_get_by_name("dc");

	if (!chip->dc_psy)
		return false;

	return true;
}
/* end 9802-799 */

static int get_val(struct range_data *range, int hysteresis, int current_index,
		int threshold,
		int *new_index, int *val,
		/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
		int last_jeita_status, int current_jeita_status)
		/* end 9801-1214 */
{
	int i;

	*new_index = -EINVAL;
	/* first find the matching index without hysteresis */
	for (i = 0; i < MAX_STEP_CHG_ENTRIES; i++)
		if (is_between(range[i].low_threshold,
			range[i].high_threshold, threshold)) {
			*new_index = i;
			*val = range[i].value;
			/* WayneWCShiue - 9801-1663 - If the threshold = 0, the undefined configuration will hit. We need to break the loop once got the confgiuration */
			break;
			/* end 9801-1663 */
		}

	/* if nothing was found, return -ENODATA */
	if (*new_index == -EINVAL)
		return -ENODATA;
	/*
	 * If we don't have a current_index return this
	 * newfound value. There is no hysterisis from out of range
	 * to in range transition
	 */
	if (current_index == -EINVAL)
		return 0;

	/*
	 * Check for hysteresis if it in the neighbourhood
	 * of our current index.
	 */

	/* WayneWCShiiue - If the health is from GOOD to WARM or COOL, we need to react immediately, ignore the hysteresis */
	if(last_jeita_status == POWER_SUPPLY_HEALTH_GOOD &&
	  (current_jeita_status == POWER_SUPPLY_HEALTH_COOL || current_jeita_status == POWER_SUPPLY_HEALTH_WARM)) {
		return 0;
	}
	/* END - WayneWCShiiue */


	if (*new_index == current_index + 1) {
		if (threshold < range[*new_index].low_threshold + hysteresis) {
			/*
			 * Stay in the current index, threshold is not higher
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	} else if (*new_index == current_index - 1) {
		if (threshold > range[*new_index].high_threshold - hysteresis + 1 ) { /* WayneWCShiue - we need to add 1 to avoid 1degC delay from JEITA WARM to GOOD*/
			/*
			 * stay in the current index, threshold is not lower
			 * by hysteresis amount
			 */
			*new_index = current_index;
			*val = range[current_index].value;
		}
	}
	return 0;
}

static int handle_step_chg_config(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	int rc = 0, fcc_ua = 0;
	u64 elapsed_us;

	elapsed_us = ktime_us_delta(ktime_get(), chip->step_last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto reschedule;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_STEP_CHARGING_ENABLED, &pval);
	if (rc < 0)
		chip->step_chg_enable = 0;
	else
		chip->step_chg_enable = pval.intval;

	if (!chip->step_chg_enable) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	rc = power_supply_get_property(chip->batt_psy,
				step_chg_config.psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				step_chg_config.prop_name, rc);
		return rc;
	}

	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	rc = get_val(step_chg_config.fcc_cfg, step_chg_config.hysteresis,
			chip->step_index,
			pval.intval,
			&chip->step_index,
			&fcc_ua,
			POWER_SUPPLY_HEALTH_GOOD,
			POWER_SUPPLY_HEALTH_GOOD);
	/* end 9801-1214 */

	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, STEP_CHG_VOTER, false, 0);
		goto update_time;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		return -EINVAL;

	vote(chip->fcc_votable, STEP_CHG_VOTER, true, fcc_ua);

	pr_debug("%s = %d Step-FCC = %duA\n",
		step_chg_config.prop_name, pval.intval, fcc_ua);

update_time:
	chip->step_last_update_time = ktime_get();
	return 0;

reschedule:
	/* reschedule 1000uS after the remaining time */
	return (STEP_CHG_HYSTERISIS_DELAY_US - elapsed_us + 1000);
}

static int handle_jeita(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	union power_supply_propval jeita_pval = {0, };
	/* end 9801-1214 */

	int rc = 0, fcc_ua = 0, fv_uv = 0;
	u64 elapsed_us;

	rc = power_supply_get_property(chip->batt_psy,
		POWER_SUPPLY_PROP_SW_JEITA_ENABLED, &pval);
	if (rc < 0)
		chip->sw_jeita_enable = 0;
	else
		chip->sw_jeita_enable = pval.intval;

	if (!chip->sw_jeita_enable) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->jeita_last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto reschedule;

	rc = power_supply_get_property(chip->batt_psy,
				jeita_fcc_config.psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				step_chg_config.prop_name, rc);
		return rc;
	}

	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_HEALTH, &jeita_pval);

	if(rc < 0) {
		pr_err("Could not get battery health in handle_jeita\n");
		return rc;
	}

	rc = get_val(jeita_fcc_config.fcc_cfg, jeita_fcc_config.hysteresis,
			chip->jeita_fcc_index,
			pval.intval,
			&chip->jeita_fcc_index,
			&fcc_ua,
			chip->last_jeita_status,
			jeita_pval.intval
			);
	/* end 9801-1214 */

	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, JEITA_VOTER, false, 0);
		goto update_time;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	vote(chip->fcc_votable, JEITA_VOTER, true, fcc_ua);

	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_HEALTH, &jeita_pval);

	rc = get_val(jeita_fv_config.fv_cfg, jeita_fv_config.hysteresis,
			chip->jeita_fv_index,
			pval.intval,
			&chip->jeita_fv_index,
			&fv_uv,
			chip->last_jeita_status,
			jeita_pval.intval
			);

	/* end 9801-1214 */

	if (rc < 0) {
		/* remove the vote if no step-based fcc is found */
		if (chip->fv_votable)
			vote(chip->fv_votable, JEITA_VOTER, false, 0);
		goto update_time;
	}

	chip->fv_votable = find_votable("FV");
	if (!chip->fv_votable)
		goto update_time;

	vote(chip->fv_votable, JEITA_VOTER, true, fv_uv);

	pr_debug("%s = %d FCC = %duA FV = %duV\n",
		step_chg_config.prop_name, pval.intval, fcc_ua, fv_uv);

update_time:
	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	chip->last_jeita_status = jeita_pval.intval;
	/* end 9801-1214 */
	chip->jeita_last_update_time = ktime_get();
	return 0;

reschedule:
	/* reschedule 1000uS after the remaining time */
	return (STEP_CHG_HYSTERISIS_DELAY_US - elapsed_us + 1000);
}

/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
static int handle_WLC(struct step_chg_info *chip)
{
	union power_supply_propval pval = {0, };
	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	union power_supply_propval jeita_pval = {0, };
	/* end 9801-1214 */

	int rc = 0, fcc_ua = 0;
	u64 elapsed_us;
	bool dc_present = 0;
	bool dc_online = 0;

	if ((!chip->fih_wlc_fcc_en) || (!is_dc_available(chip))) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, FIH_WLC_VOTER, false, 0);
		return 0;
	}

	rc = power_supply_get_property(chip->dc_psy,
	POWER_SUPPLY_PROP_PRESENT, &pval);
	if(rc < 0)
		dc_present = 0;
	else
		dc_present = pval.intval;

	pval.intval = 0;
	rc = power_supply_get_property(chip->dc_psy,
	POWER_SUPPLY_PROP_ONLINE, &pval);
	if(rc < 0)
		dc_online = 0;
	else
		dc_online = pval.intval;

	if(dc_present !=1 || dc_online !=1) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, FIH_WLC_VOTER, false, 0);
		return 0;
	}

	elapsed_us = ktime_us_delta(ktime_get(), chip->wlc_last_update_time);
	if (elapsed_us < STEP_CHG_HYSTERISIS_DELAY_US)
		goto reschedule;

	rc = power_supply_get_property(chip->batt_psy,
				wlc_fcc_config.psy_prop, &pval);
	if (rc < 0) {
		pr_err("Couldn't read %s property rc=%d\n",
				wlc_fcc_config.prop_name, rc);
		return rc;
	}

	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	rc = power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_HEALTH, &jeita_pval);

	if(rc < 0) {
		pr_err("Could not get battery health in handle_jeita\n");
		return rc;
	}

	rc = get_val(wlc_fcc_config.fcc_cfg, wlc_fcc_config.hysteresis,
			chip->wlc_fcc_index,
			pval.intval,
			&chip->wlc_fcc_index,
			&fcc_ua,
			chip->last_jeita_status,
			jeita_pval.intval
			);
	/* end 9801-1214 */

	if (rc < 0) {
		/* remove the vote if no wlc fcc is found */
		if (chip->fcc_votable)
			vote(chip->fcc_votable, FIH_WLC_VOTER, false, 0);
		goto update_time;
	}

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");
	if (!chip->fcc_votable)
		/* changing FCC is a must */
		return -EINVAL;

	vote(chip->fcc_votable, FIH_WLC_VOTER, true, fcc_ua);

	pr_debug("WLC: %s = %d WLC-FCC = %duA\n",
		wlc_fcc_config.prop_name, pval.intval, fcc_ua);

update_time:
	/* WayneWCShiue - 9801-1214 - Only use hysteresis when JEITA change from noraml to cool or warm */
	chip->last_jeita_status = jeita_pval.intval;
	/* end 9801-1214 */
	chip->wlc_last_update_time = ktime_get();
	return 0;

reschedule:
	/* reschedule 1000uS after the remaining time */
	return (STEP_CHG_HYSTERISIS_DELAY_US - elapsed_us + 1000);
}
/* end 9802-799 */

static void status_change_work(struct work_struct *work)
{
	struct step_chg_info *chip = container_of(work,
			struct step_chg_info, status_change_work.work);
	int rc = 0;
	int reschedule_us;
	int reschedule_jeita_work_us = 0;
	int reschedule_step_work_us = 0;
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	int reschedule_wlc_work_us = 0;
	/* end 9802-799 */

	if (!is_batt_available(chip))
		return;

	/* skip elapsed_us debounce for handling battery temperature */
	rc = handle_jeita(chip);
	if (rc > 0)
		reschedule_jeita_work_us = rc;
	else if (rc < 0)
		pr_err("Couldn't handle sw jeita rc = %d\n", rc);

	rc = handle_step_chg_config(chip);
	if (rc > 0)
		reschedule_step_work_us = rc;
	if (rc < 0)
		pr_err("Couldn't handle step rc = %d\n", rc);

	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	rc = handle_WLC(chip);
	if (rc > 0)
		reschedule_wlc_work_us = rc;
	if (rc < 0)
		pr_err("Couldn't handle WLC rc = %d\n", rc);
	/* end 9802-799 */

	reschedule_us = min(reschedule_jeita_work_us, reschedule_step_work_us);
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	reschedule_us = min(reschedule_us, reschedule_wlc_work_us);
	/* end 9802-799 */

	if (reschedule_us == 0)
		__pm_relax(chip->step_chg_ws);
	else
		schedule_delayed_work(&chip->status_change_work,
				usecs_to_jiffies(reschedule_us));
}

static int step_chg_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct step_chg_info *chip = container_of(nb, struct step_chg_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if ((strcmp(psy->desc->name, "battery") == 0)) {
		__pm_stay_awake(chip->step_chg_ws);
		schedule_delayed_work(&chip->status_change_work, 0);
	}

	return NOTIFY_OK;
}

static int step_chg_register_notifier(struct step_chg_info *chip)
{
	int rc;

	chip->nb.notifier_call = step_chg_notifier_call;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
int qcom_step_chg_init(bool step_chg_enable, bool sw_jeita_enable, bool fih_wlc_fcc_enable)
/* end 9802-799 */
{
	int rc;
	struct step_chg_info *chip;

	if (the_chip) {
		pr_err("Already initialized\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->step_chg_ws = wakeup_source_register("qcom-step-chg");
	if (!chip->step_chg_ws) {
		rc = -EINVAL;
		goto cleanup;
	}

	chip->step_chg_enable = step_chg_enable;
	chip->sw_jeita_enable = sw_jeita_enable;
	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	chip->fih_wlc_fcc_en = fih_wlc_fcc_enable;
	/* end 9802-799 */

	chip->step_index = -EINVAL;
	chip->jeita_fcc_index = -EINVAL;
	chip->jeita_fv_index = -EINVAL;

	if (step_chg_enable && (!step_chg_config.psy_prop ||
				!step_chg_config.prop_name)) {
		/* fail if step-chg configuration is invalid */
		pr_err("Step-chg configuration not defined - fail\n");
		rc = -ENODATA;
		goto release_wakeup_source;
	}

	if (sw_jeita_enable && (!jeita_fcc_config.psy_prop ||
				!jeita_fcc_config.prop_name)) {
		/* fail if step-chg configuration is invalid */
		pr_err("Jeita TEMP configuration not defined - fail\n");
		rc = -ENODATA;
		goto release_wakeup_source;
	}

	if (sw_jeita_enable && (!jeita_fv_config.psy_prop ||
				!jeita_fv_config.prop_name)) {
		/* fail if step-chg configuration is invalid */
		pr_err("Jeita TEMP configuration not defined - fail\n");
		rc = -ENODATA;
		goto release_wakeup_source;
	}

	/* WayneWCShiue - 9802-799 - Implement the WLC FCC adjust mechansim */
	if (fih_wlc_fcc_enable && (!wlc_fcc_config.psy_prop ||
				!wlc_fcc_config.prop_name)) {
		/* fail if step-chg configuration is invalid */
		pr_err("WLC TEMP configuration not defined - fail\n");
		rc = -ENODATA;
		goto release_wakeup_source;
	}
	/* end 9802-799 */

	INIT_DELAYED_WORK(&chip->status_change_work, status_change_work);

	rc = step_chg_register_notifier(chip);
	if (rc < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", rc);
		goto release_wakeup_source;
	}

	the_chip = chip;

	if (step_chg_enable)
		pr_info("Step charging enabled. Using %s source\n",
				step_chg_config.prop_name);

	return 0;

release_wakeup_source:
	wakeup_source_unregister(chip->step_chg_ws);
cleanup:
	kfree(chip);
	return rc;
}

void qcom_step_chg_deinit(void)
{
	struct step_chg_info *chip = the_chip;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->status_change_work);
	power_supply_unreg_notifier(&chip->nb);
	wakeup_source_unregister(chip->step_chg_ws);
	the_chip = NULL;
	kfree(chip);
}
