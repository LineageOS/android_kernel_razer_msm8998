#ifndef __FIH_CPU_H
#define __FIH_CPU_H

/* Refer the 80-P2169-2X Rev.C MSM8998_Hardware_Register_Description.pdf
	FIH_MSM_HW_REV_NUM is mean JTAG_ID(0x00786130) (P.31)
  FIH_MSM_HW_FEATURE_ID is mean FEATURE_ID[bits 27:20](0x00784130) (P.15)
	FIH_MSM_SERIAL_NO is mean SERIAL_NUM(0x00786134) (P.31)
*/
#define FIH_MSM_HW_REV_NUM  0x00786130
#define FIH_MSM_HW_FEATURE_ID  0x00784130
#define FIH_MSM_SERIAL_NO   0x0005C008

/*
  Refer the 80-P2169-4 Rev.D msm8998_device_revision_guide.pdf
*/
#define HW_REV_NUM_MSM8998	0X0005E0E1

#define HW_REV_ES 0x00

#endif /* __FIH_CPU_H */
