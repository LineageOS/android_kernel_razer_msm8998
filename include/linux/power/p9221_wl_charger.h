#ifndef P9221_WL_CHARGER_H
#define P9221_WL_CHARGER_H

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>

/* register address of chipset */
#define P9221_REG_LEN 2

#define P9221_REG_CHIPID 0x0

#define P9221_REG_FW_MAJVER 0x4

#define P9221_REG_FW_MINVER 0x6

#define P9221_REG_CHIPSTATUS 0x34

#define P9221_REG_RX_VOUT 0x3C

#define P9221_REG_RX_VRECT 0x40

#define P9221_REG_RX_IOUT 0x44

#define P9221_REG_RX_TEMP 0x46

#define P9221_REG_RX_OPFREQ 0x48

#define P9221_REG_FOD_0 0x68

#define P9221_REG_FOD_1 0x6A

#define P9221_REG_FOD_2 0x6C

#define P9221_REG_FOD_3 0x6E

#define P9221_REG_FOD_4 0x70

#define P9221_REG_FOD_5 0x72

/*
    Wireless charger FOD (Foreign Object Detection) configuration value
    BPP(Baseline Power Profile): (5V/1A)
    EPP(Extended Power Profile): (9V/1A)
    Reg.    Reg field name      BPP FOD     EPP FOD
    0x68        FOD_0_A         0xAA        0xAD
    0x69        FOD_0_B         0x34        0x7F
    0x6A        FOD_1_A         0x88        0x95
    0x6B        FOD_1_B         0x3B        0x7F
    0x6C        FOD_2_A         0x89        0x92
    0x6D        FOD_2_B         0x3F        0x7F
    0x6E        FOD_3_A         0x96        0x94
    0x6F        FOD_3_B         0x2C        0x72
    0x70        FOD_4_A         0x9A        0x94
    0x71        FOD_4_B         0x2A        0x7A
    0x72        FOD_5_A         0x9D        0x96
    0x73        FOD_5_B         0x1C        0x7C
*/

#define BPPFOD_0_A 0xAA
#define BPPFOD_0_B 0x34
#define BPPFOD_1_A 0x88
#define BPPFOD_1_B 0x3B
#define BPPFOD_2_A 0x89
#define BPPFOD_2_B 0x3F
#define BPPFOD_3_A 0x96
#define BPPFOD_3_B 0x2C
#define BPPFOD_4_A 0x9A
#define BPPFOD_4_B 0x2A
#define BPPFOD_5_A 0x9D
#define BPPFOD_5_B 0x1C

#define EPPFOD_0_A 0xAD
#define EPPFOD_0_B 0x7F
#define EPPFOD_1_A 0x95
#define EPPFOD_1_B 0x7F
#define EPPFOD_2_A 0x92
#define EPPFOD_2_B 0x7F
#define EPPFOD_3_A 0x94
#define EPPFOD_3_B 0x72
#define EPPFOD_4_A 0x94
#define EPPFOD_4_B 0x7A
#define EPPFOD_5_A 0x96
#define EPPFOD_5_B 0x7C

/* Status of chipset */
#define CHIPSET_UNINIT -1
#define CHIPSET_DISABLE 0
#define CHIPSET_ENABLE 1

/* Wireless charger Tx mode */
#define WL_TX_OFF 0
#define WL_TX_ON 1

enum wlc_power_profile {
    WLC_NONE = 0,
	WLC_BPP,        /* BPP(Baseline Power Profile): (5V/1A) */
	WLC_EPP,        /* EPP(Extended Power Profile): (9V/1A) */
};

struct p9221_wlc_chip {
    struct i2c_client *client;
    struct device *dev;

    /* locks */
    struct mutex lock;

    /* power supplies */
    struct power_supply_desc wireless_psy_d;
    struct power_supply *wireless_psy;
    struct power_supply	*dc_psy;

    /* notifiers */
    struct notifier_block	nb;

    /* work */
    struct work_struct	wlc_interrupt_work;
    struct delayed_work wlc_rxmode_monitor_work;
    struct delayed_work wlc_udpate_fod_work;

    /* gpio */
    int irq_gpio;
    int enable_gpio;
    int rx_on_gpio;
    int tx_on_gpio;

    /* irq */
    int irq_num;

    /* pin control */
    struct pinctrl *wl_pinctrl;
    struct pinctrl_state *wlc_intr_active;
    struct pinctrl_state *wlc_rxpath_active;
    struct pinctrl_state *wlc_rxpath_sleep;
    struct pinctrl_state *wlc_txpath_active;
    struct pinctrl_state *wlc_txpath_sleep;
    struct pinctrl_state *wlc_sleep;

    int chipset_enabled;
    enum wlc_power_profile power_profile;
    int tx_enabled;
    int i2c_failed;
};

#endif /* P9221_WL_CHARGER_H */
