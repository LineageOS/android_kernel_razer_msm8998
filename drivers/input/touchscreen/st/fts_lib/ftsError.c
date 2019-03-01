/*

**************************************************************************
**                        STMicroelectronics 		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*                  FTS error/info kernel log reporting			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/


#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/completion.h>
#include <linux/wakelock.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "../fts.h"
#include "ftsCrossCompile.h"
#include "ftsError.h"
#include "ftsIO.h"
#include "ftsTool.h"
#include "ftsCompensation.h"

extern chipInfo ftsInfo;

void logError(int force, const char *msg, ...){
	if (force == 1 
#ifdef DEBUG
		|| 1 
#endif
		) {
		va_list args;
		va_start(args, msg);
		vprintk(msg, args);
		va_end(args);
	}
}

int isI2cError(int error){
	if(((error & 0x000000FF) >= (ERROR_I2C_R & 0x000000FF)) && ((error & 0x000000FF) <= (ERROR_I2C_O & 0x000000FF)))
		return 1;
	else
		return 0;
}

int dumpErrorInfo(){
	int ret,i;
	u8 data[ERROR_INFO_SIZE] = {0};
	u32 sign =0;	

	logError(0,"%s %s: Starting dump of error info...\n",tag,__func__);
	if(ftsInfo.u16_errOffset!=INVALID_ERROR_OFFS){
		ret=readCmdU16(FTS_CMD_FRAMEBUFFER_R, ftsInfo.u16_errOffset, data, ERROR_INFO_SIZE, DUMMY_FRAMEBUFFER);
		if(ret<OK){
			logError(1, "%s %s: reading data ERROR %02X\n", tag, __func__, ret );
			return ret;
		}else{
			logError(1, "%s %s: Error Info = \n", tag, __func__);
			u8ToU32(data,&sign);
			if(sign!=ERROR_SIGNATURE)
				logError(1, "%s %s: Wrong Error Signature! Data may be invalid! \n", tag, __func__);
			else
				logError(1, "%s %s: Error Signature OK! Data are valid! \n", tag, __func__);

			for(i=0; i<ERROR_INFO_SIZE; i++){
				if(i%4==0){
					logError(1, KERN_ERR "\n%s %s: %d) ", tag, __func__,i/4);
				}
				logError(1, "%02X ", data[i]);
			}
			logError(1, "\n");

			logError(0,"%s %s: dump of error info FINISHED!\n",tag,__func__);
			return OK;
		}
	}else{
		logError(1, "%s %s: Invalid error offset ERROR %02X\n", tag, __func__, ERROR_OP_NOT_ALLOW );
		return ERROR_OP_NOT_ALLOW;
	}
	
}

int errorHandler(u8 *event, int size){
	int res=OK;
	struct fts_ts_info *info=NULL;

	if(getClient()!=NULL)
		info = i2c_get_clientdata(getClient());

	if(info!=NULL && event!=NULL && size>1 && event[0]==EVENTID_ERROR_EVENT){
		logError(1,"%s errorHandler: Starting handling...\n",tag);
		switch(event[1])			 //TODO: write an error log for undefinied command subtype 0xBA
		{
			case EVENT_TYPE_ESD_ERROR:	//esd
			res=fts_chip_powercycle(info);
			if (res<OK){
				logError(1, "%s errorHandler: Error performing powercycle ERROR %08X\n", tag, res);
			}

			res=fts_system_reset();
			if (res<OK)
			{
				logError(1, "%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);
			}
			res = (ERROR_HANDLER_STOP_PROC|res);
			break;

			case EVENT_TYPE_WATCHDOG_ERROR:	//watchdog
			dumpErrorInfo();
			res=fts_system_reset();
				if (res<OK)
				{
					logError(1, "%s errorHandler: Cannot reset the device ERROR %08X\n", tag, res);
				}
			res = (ERROR_HANDLER_STOP_PROC|res);
			break;

			case EVENT_TYPE_CHECKSUM_ERROR: //CRC ERRORS
				switch(event[2]){
					case CRC_CONFIG_SIGNATURE:
						logError(1, "%s errorHandler: Config Signature ERROR !\n", tag);
					break;

					case CRC_CONFIG:
						logError(1, "%s errorHandler: Config CRC ERROR !\n", tag);
					break;

					case CRC_CX_MEMORY:
						logError(1, "%s errorHandler: CX CRC ERROR !\n", tag);
					break;
				}
			break;

			case EVENT_TYPE_LOCKDOWN_ERROR:
				//res = (ERROR_HANDLER_STOP_PROC|res); 				//stop lockdown code routines in order to retry
				switch(event[2]){
					case 0x01:
						logError(1, "%s errorHandler: Lockdown code alredy written into the IC !\n", tag);
						break;

					case 0x02:
						logError(1, "%s errorHandler: Lockdown CRC check fail during a WRITE !\n", tag);
						break;

					case 0x03:
						logError(1, "%s errorHandler: Lockdown WRITE command format wrong !\n", tag);
						break;

					case 0x04:
						logError(1, "%s errorHandler: Lockdown Memory Corrupted! Please contact ST for support !\n", tag);
						break;

					case 0x11:
						logError(1, "%s errorHandler: NO Lockdown code to READ into the IC !\n", tag);
						break;

					case 0x12:
						logError(1, "%s errorHandler: Lockdown code data corrupted !\n", tag);
						break;

					case 0x13:
						logError(1, "%s errorHandler: Lockdown READ command format wrong !\n", tag);
						break;

					case 0x21:
						logError(1, "%s errorHandler: Exceeded maximum number of Lockdown code REWRITE into the IC !\n", tag);
						break;

					case 0x22:
						logError(1, "%s errorHandler: Lockdown CRC check fail during a REWRITE !\n", tag);
						break;

					case 0x23:
						logError(1, "%s errorHandler: Lockdown REWRITE command format wrong !\n", tag);
						break;

					case 0x24:
						logError(1, "%s errorHandler: Lockdown Memory Corrupted! Please contact ST for support !\n", tag);
						break;
					default:
						logError(1, "%s errorHandler: No valid error type for LOCKDOWN ERROR! \n", tag);

				}
				break;

			default:
				logError(1, "%s errorHandler: No Action taken! \n", tag);
			break;
			
		}
		logError(1,"%s errorHandler: handling Finished! res = %08X\n",tag,res);
		return res;
	}else{
		logError(1,"%s errorHandler: event Null or not correct size! ERROR %08X \n",tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}


}























