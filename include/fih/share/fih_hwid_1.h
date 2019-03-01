#ifndef __FIH_HWID_1_H
#define __FIH_HWID_1_H

struct st_hwid_table {
	/* mpp */
	unsigned int r1; /* pin: PROJECT-ID */
	char r2; /* pin: HW_REV-ID */
	char r3; /* pin: RF_BAND-ID */
	/* info */
	char prj; /* project */
	char rev; /* hw_rev */
	char rf;  /* rf_band */
	/* device tree */
	char dtm; /* Major number */
	char dtn; /* minor Number */
	/* driver */
	char btn; /* button */
	char uart;
};

enum {
	/* mpp */
	FIH_HWID_R1 = 0,
	FIH_HWID_R2,
	FIH_HWID_R3,
	/* info */
	FIH_HWID_PRJ,
	FIH_HWID_REV,
	FIH_HWID_RF,
	/* device tree */
	FIH_HWID_DTM,
	FIH_HWID_DTN,
	/* driver */
	FIH_HWID_BTN,
	FIH_HWID_UART,
	/* max */
	FIH_HWID_MAX
};

/******************************************************************************
 *  device tree
 *****************************************************************************/

enum {
	FIH_DTM_DEFAULT = 0,
	FIH_DTM_MAX = 255,
};

enum {
	FIH_DTN_DEFAULT = 0,
	FIH_DTN_MAX = 255,
};

/******************************************************************************
 *  driver
 *****************************************************************************/

enum {
	FIH_BTN_DEFAULT = 0,
	FIH_BTN_NONE,
	FIH_BTN_PMIC_UP_5_DN_2,
	FIH_BTN_MAX
};

enum {
	FIH_UART_DEFAULT = 0,
	FIH_UART_NONE,
	FIH_UART_TX_4_RX_5,
	FIH_UART_MAX
};

#endif /* __FIH_HWID_1_H */
