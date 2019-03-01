/*

**************************************************************************
**                        STMicroelectronics 							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                     FTS Gesture Utilities								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"
#include "ftsError.h"
#include "ftsGesture.h"
#include "ftsIO.h"
#include "ftsTool.h"


static char tag[8] = "[ FTS ]\0";

static u8 gesture_mask[GESTURE_MASK_SIZE] = { 0 };
static u8 custom_gestures[GESTURE_CUSTOM_NUMBER][GESTURE_CUSTOM_POINTS];
static u8 custom_gesture_index[GESTURE_CUSTOM_NUMBER] = { 0 };
u16 gesture_coordinates_x[GESTURE_COORDS_REPORT_MAX] = {0};
u16 gesture_coordinates_y[GESTURE_COORDS_REPORT_MAX] = {0};
int gesture_coords_reported = ERROR_OP_NOT_ALLOW;
static int refreshGestureMask = 0;
struct mutex gestureMask_mutex;


int updateGestureMask(u8 *mask, int size, int en) {
	u8 temp;
	int i;

	if (mask != NULL) {
		if (size <= GESTURE_MASK_SIZE) {
			if (en==FEAT_ENABLE) {
				mutex_lock(&gestureMask_mutex);
				logError(0, "%s updateGestureMask: setting gesture mask to enable...\n", tag);
				if (mask != NULL) {
					for (i = 0; i < size;i++) {
						gesture_mask[i] = gesture_mask[i] | mask[i];						//back up of the gesture enabled
					}
				}
				refreshGestureMask = 1;
				logError(0, "%s updateGestureMask: gesture mask to enable SET! \n", tag);
				mutex_unlock(&gestureMask_mutex);
				return OK;
			}

			else if (en==FEAT_DISABLE) {
				mutex_lock(&gestureMask_mutex);
				logError(0, "%s updateGestureMask: setting gesture mask to disable...\n", tag);
				for (i = 0; i < size;i++) {
					temp = gesture_mask[i] ^ mask[i];			// enabled XOR disabled
					gesture_mask[i] = temp & gesture_mask[i];	// temp AND enabled				//disable the gestures that were enabled
				}
				logError(0, "%s updateGestureMask: gesture mask to disable SET! \n", tag);
				refreshGestureMask = 1;
				mutex_unlock(&gestureMask_mutex);
				return OK;
			}
			else {
				logError(1, "updateGestureMask: Enable parameter Invalid! %d != %d or %d ERROR %08X", tag, FEAT_DISABLE, FEAT_ENABLE, ERROR_OP_NOT_ALLOW);
				return ERROR_OP_NOT_ALLOW;
			}
		}
		else {
			logError(1, "%s updateGestureMask: Size not valid! %d > %d ERROR %08X \n", tag, size, GESTURE_MASK_SIZE);
			return ERROR_OP_NOT_ALLOW;
		}
	}
	else {
		logError(1, "%s updateGestureMask: Mask NULL! ERROR %08X \n", tag, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}


}

int enableGesture(u8 *mask, int size) {
	u8 cmd[GESTURE_MASK_SIZE + 2];
	u8 readData[FIFO_EVENT_SIZE];
	int i, res;
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_ENABLE };

	logError(0, "%s Trying to enable gesture... \n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_ENABLE;

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size;i++) {
				cmd[i + 2] = mask[i];
				gesture_mask[i] = gesture_mask[i] | mask[i];						//back up of the gesture enabled
			}
			while (i < GESTURE_MASK_SIZE) {
				cmd[i + 2] = gesture_mask[i];
				i++;
			}
		}
		else {
			for (i = 0; i < GESTURE_MASK_SIZE;i++) {
				cmd[i + 2] = gesture_mask[i];
			}
		}

		res = fts_writeFwCmd(cmd, GESTURE_MASK_SIZE + 2);
		if (res < OK) {
			logError(1, "%s enableGesture: ERROR %08X \n", tag, res);
			goto END;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
		if (res < OK) {
			logError(1, "%s enableGesture: pollForEvent ERROR %08X \n", tag, res);
			goto END;
		}

		if (readData[4] != 0x00) {
			logError(1, "%s enableGesture: ERROR %08X \n", tag, ERROR_GESTURE_ENABLE_FAIL);
			res= ERROR_GESTURE_ENABLE_FAIL;
			goto END;
		}

		logError(0, "%s enableGesture DONE! \n", tag);
		res = OK;

	END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	}
	else {
		logError(1, "%s enableGesture: Size not valid! %d > %d ERROR %08X \n", tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}

}


int disableGesture(u8 *mask, int size) {
	u8 cmd[2 + GESTURE_MASK_SIZE];
	u8 readData[FIFO_EVENT_SIZE];
	u8 temp;
	int i, res;
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, 0x00, GESTURE_DISABLE };

	logError(0, "%s Trying to disable gesture... \n", tag);
	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_DISABLE;

	if (size <= GESTURE_MASK_SIZE) {
		mutex_lock(&gestureMask_mutex);
		if (mask != NULL) {
			for (i = 0; i < size;i++) {
				cmd[i + 2] = mask[i];
				temp = gesture_mask[i] ^ mask[i];			// enabled XOR disabled
				gesture_mask[i] = temp & gesture_mask[i];	// temp AND enabled				//disable the gestures that were enabled
			}
			while (i < GESTURE_MASK_SIZE) {
				//cmd[i + 2] = gesture_mask[i];			//disable all the other gesture not specified
				//gesture_mask[i] = 0x00;

				cmd[i + 2] = 0x00;				//leave untouched the gestures not specified

				i++;
			}
		}
		else {
			for (i = 0; i < GESTURE_MASK_SIZE;i++) {
				//cmd[i + 2] = gesture_mask[i];
				cmd[i + 2] = 0xFF;			//if NULL is passed disable all the possible gestures
			}
		}

		res = fts_writeFwCmd(cmd, 2 + GESTURE_MASK_SIZE);
		if (res < OK) {
			logError(1, "%s disableGesture: ERROR %08X \n", tag, res);
			goto END;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
		if (res < OK) {
			logError(1, "%s disableGesture: pollForEvent ERROR %08X \n", tag, res);
			goto END;
		}

		if (readData[4] != 0x00) {
			logError(1, "%s disableGesture: ERROR %08X \n", tag, ERROR_GESTURE_ENABLE_FAIL);
			res = ERROR_GESTURE_ENABLE_FAIL;
			goto END;
		}

		logError(0, "%s disableGesture DONE! \n", tag);
		
		res = OK;

	END:
		mutex_unlock(&gestureMask_mutex);
		return res;
	}
	else {
		logError(1, "%s disableGesture: Size not valid! %d > %d ERROR %08X \n", tag, size, GESTURE_MASK_SIZE);
		return ERROR_OP_NOT_ALLOW;
	}
}

int startAddCustomGesture(u8 gestureID) {
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_START_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[4] = { EVENTID_GESTURE,EVENT_TYPE_ENB,gestureID,GESTURE_START_ADD };

	res = fts_writeFwCmd(cmd, 3);
	if (res < OK) {
		logError(1, "%s startAddCustomGesture: Impossible to start adding custom gesture ID = %02X! ERROR %08X \n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s startAddCustomGesture: start add event not found! ERROR %08X \n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {			//check of gestureID is redundant
		logError(1, "%s startAddCustomGesture: start add event status not OK! ERROR %08X \n", tag, readData[4]);
		return ERROR_GESTURE_START_ADD;
	}


	return OK;
}


int finishAddCustomGesture(u8 gestureID) {
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GESTURE_FINISH_ADD,  gestureID };
	int res;
	u8 readData[FIFO_EVENT_SIZE];
	int event_to_search[4] = { EVENTID_GESTURE,EVENT_TYPE_ENB,gestureID,GESTURE_FINISH_ADD };

	res = fts_writeFwCmd(cmd, 3);
	if (res < OK) {
		logError(1, "%s finishAddCustomGesture: Impossible to finish adding custom gesture ID = %02X! ERROR %08X \n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s finishAddCustomGesture: finish add event not found! ERROR %08X \n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {			//check of gestureID is redundant
		logError(1, "%s finishAddCustomGesture: finish add event status not OK! ERROR %08X \n", tag, readData[4]);
		return ERROR_GESTURE_FINISH_ADD;
	}

	return OK;

}

int loadCustomGesture(u8 *template, u8 gestureID) {
	int res, i;
	int remaining = GESTURE_CUSTOM_POINTS;
	int toWrite, offset = 0;
	u8 cmd[TEMPLATE_CHUNK + 5];
	int event_to_search[4] = { EVENTID_GESTURE,EVENT_TYPE_ENB,gestureID,GESTURE_DATA_ADD };
	u8 readData[FIFO_EVENT_SIZE];

	logError(0, "%s Starting adding custom gesture procedure... \n", tag);

	res = startAddCustomGesture(gestureID);
	if (res < OK) {
		logError(1, "%s loadCustomGesture: unable to start adding procedure! ERROR %08X \n", tag, res);
		return res;
	}

	cmd[0] = FTS_CMD_GESTURE_CMD;
	cmd[1] = GESTURE_DATA_ADD;
	cmd[2] = gestureID;
	while (remaining > 0) {
		if (remaining > TEMPLATE_CHUNK) {
			toWrite = TEMPLATE_CHUNK;
		}
		else {
			toWrite = remaining;
		}

		cmd[3] = toWrite;
		cmd[4] = offset;
		for (i = 0; i < toWrite; i++) {
			cmd[i + 5] = template[i];
		}

		res = fts_writeFwCmd(cmd, toWrite + 5);
		if (res < OK) {
			logError(1, "%s loadCustomGesture: unable to start adding procedure! ERROR %08X \n", tag, res);
			return res;
		}

		res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
		if (res < OK) {
			logError(1, "%s loadCustomGesture: add event not found! ERROR %08X \n", tag, res);
			return res;
		}

		if (readData[2] != gestureID || readData[4] != 0x00) {			//check of gestureID is redundant
			logError(1, "%s loadCustomGesture: add event status not OK! ERROR %08X \n", tag, readData[4]);
			return ERROR_GESTURE_DATA_ADD;
		}

		remaining -= toWrite;
		offset += toWrite / 2;
	}

	res = finishAddCustomGesture(gestureID);
	if (res < OK) {
		logError(1, "%s loadCustomGesture: unable to finish adding procedure! ERROR %08X \n", tag, res);
		return res;
	}

	logError(0, "%s Adding custom gesture procedure DONE! \n", tag);
	return OK;

}


int reloadCustomGesture() {
	int res, i;

	logError(0, "%s Starting reload Gesture Template... \n", tag);

	for (i = 0; i < GESTURE_CUSTOM_NUMBER; i++) {
		if (custom_gesture_index[i] == 1) {
			res = loadCustomGesture(custom_gestures[i], GESTURE_CUSTOM_OFFSET + i);
			if (res < OK) {
				logError(1, "%s reloadCustomGesture: Impossible to load custom gesture ID = %02X! ERROR %08X \n", tag, GESTURE_CUSTOM_OFFSET + i, res);
				return res;
			}
		}
	}

	logError(0, "%s Reload Gesture Template DONE! \n", tag);
	return OK;

}

int enterGestureMode(int reload) {
	u8 cmd = FTS_CMD_GESTURE_MODE;
	int res, ret;

	res = fts_disableInterrupt();
	if (res < OK) {
		logError(1, "%s enterGestureMode: ERROR %08X \n", tag, res | ERROR_DISABLE_INTER);
		return res | ERROR_DISABLE_INTER;
	}

	if (reload == 1 || refreshGestureMask == 1) {

		if (reload == 1) {
			res = reloadCustomGesture();
			if (res < OK) {
				logError(1, "%s enterGestureMode: impossible reload custom gesture! ERROR %08X \n", tag, res);
				goto END;
			}
		}

		/****** mandatory steps to set the correct gesture mask defined by the user ******/
		res = disableGesture(NULL, 0);			
		if (res < OK) {
			logError(1, "%s enterGestureMode: disableGesture ERROR %08X \n", tag, res);
			goto END;
		}

		res = enableGesture(NULL, 0);
		if (res < OK) {
			logError(1, "%s enterGestureMode: enableGesture ERROR %08X \n", tag, res);
			goto END;
		}

		refreshGestureMask = 0;
		/**********************************************************************************/
	}

	res = fts_writeFwCmd(&cmd, 1);
	if (res < OK) {
		logError(1, "%s enterGestureMode: enter gesture mode ERROR %08X \n", tag, res);
		goto END;
	}

	res = OK;
END:
	ret = fts_enableInterrupt();
	if (ret < OK) {
		logError(1, "%s enterGestureMode: fts_enableInterrupt ERROR %08X \n", tag, res | ERROR_ENABLE_INTER);
		res |= ret | ERROR_ENABLE_INTER;
	}


	return res;
}

int addCustomGesture(u8 *data, int size, u8 gestureID) {
	int index, res, i;

	index = gestureID - GESTURE_CUSTOM_OFFSET;

	logError(0, "%s Starting Custom Gesture Adding procedure...\n", tag);
	if (size != GESTURE_CUSTOM_POINTS || (gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID != GES_ID_CUST5)) {
		logError(1, "%s addCustomGesture: Invalid size (%d) or Custom GestureID (%02X)! ERROR %08X \n", tag, size, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	for (i = 0; i < GESTURE_CUSTOM_POINTS; i++) {
		custom_gestures[index][i] = data[i];
	}

	res = loadCustomGesture(custom_gestures[index], gestureID);
	if (res < OK) {
		logError(1, "%s addCustomGesture: impossible to load the custom gesture! ERROR %08X \n", tag, res);
		return res;
	}

	custom_gesture_index[index] = 1;
	logError(0, "%s Custom Gesture Adding procedure DONE!\n", tag);
	return OK;
}

int removeCustomGesture(u8 gestureID) {
	int res, index;
	u8 cmd[3] = { FTS_CMD_GESTURE_CMD, GETURE_REMOVE_CUSTOM, gestureID };
	int event_to_search[4] = { EVENTID_GESTURE, EVENT_TYPE_ENB, gestureID, GETURE_REMOVE_CUSTOM };
	u8 readData[FIFO_EVENT_SIZE];

	index = gestureID - GESTURE_CUSTOM_OFFSET;

	logError(0, "%s Starting Custom Gesture Removing procedure...\n", tag);
	if (gestureID != GES_ID_CUST1 && gestureID != GES_ID_CUST2 && gestureID != GES_ID_CUST3 && gestureID != GES_ID_CUST4 && gestureID != GES_ID_CUST5) {
		logError(1, "%s removeCustomGesture: Invalid Custom GestureID (%02X)! ERROR %08X \n", tag, gestureID, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}

	res = fts_writeFwCmd(cmd, 3);					//when a gesture is removed, it is also disabled automatically
	if (res < OK) {
		logError(1, "%s removeCustomGesture: Impossible to remove custom gesture ID = %02X! ERROR %08X \n", tag, gestureID, res);
		return res;
	}

	res = pollForEvent(event_to_search, 4, readData, GENERAL_TIMEOUT);
	if (res < OK) {
		logError(1, "%s removeCustomGesture: remove event not found! ERROR %08X \n", tag, res);
		return res;
	}

	if (readData[2] != gestureID || readData[4] != 0x00) {			//check of gestureID is redundant
		logError(1, "%s removeCustomGesture: remove event status not OK! ERROR %08X \n", tag, readData[4]);
		return ERROR_GESTURE_REMOVE;
	}


	custom_gesture_index[index] = 0;
	logError(0, "%s Custom Gesture Remove procedure DONE!\n", tag);
	return OK;

}

int isAnyGestureActive(void) {
	int res = 0;

	while (res < (GESTURE_MASK_SIZE-1) && gesture_mask[res] == 0) {		//-1 because in any case the last gesture mask byte will be evaluated with the following if
		res++;
	}

	if (gesture_mask[res] != 0) {
		logError(0, "%s %s: Active Gestures Found! gesture_mask[%d] = %02X !\n", tag, __func__, res, gesture_mask[res]);
		return FEAT_ENABLE;
	}
	else {
		logError(0, "%s %s: All Gestures Disabled!\n", tag, __func__);
		return FEAT_DISABLE;
	}
}

int gestureIDtoGestureMask(u8 id, u8 *mask) {
	logError(0, "%s %s: Index = %d Position = %d !\n", tag, __func__, ((int)((id) / 8)), (id % 8));
	mask[((int)((id) / 8))] |= 0x01 << (id % 8);
	return OK;
}




int readGestureCoords(u8 *event){
    int i = 0;
    u8 rCmd[3] = {FTS_CMD_FRAMEBUFFER_R, 0x00, 0x00 };
    int res;
   	
    unsigned char val[GESTURE_COORDS_REPORT_MAX*4+1];			//the max coordinates to read are GESTURE_COORDS_REPORT_MAX*4(because each coordinate is a short(*2) and we have x and y) + dummy byte
  	
		
	if(event[0]==EVENTID_GESTURE && event[1] == EVENT_TYPE_GESTURE_DTC2)
	{
		
		
		rCmd[1] = event[4];    // Offset address L
		rCmd[2] = event[3];    // Offset address H 
		gesture_coords_reported = event[6];	    //number of coords reported L    
		gesture_coords_reported = (gesture_coords_reported << 8) | event[5]; //number of coords reported H
		if(gesture_coords_reported>GESTURE_COORDS_REPORT_MAX){
			logError(1, "%s %s:  FW reported more than %d points for the gestures! Decreasing to %d \n", tag, __func__, gesture_coords_reported,GESTURE_COORDS_REPORT_MAX);
			gesture_coords_reported = GESTURE_COORDS_REPORT_MAX;
		}

		logError(1,"%s %s: Offset: %02X %02X points = %d\n", tag,__func__, rCmd[1], rCmd[2], gesture_coords_reported);
		res=fts_readCmd(rCmd, 3, (unsigned char *)val, 1 + (gesture_coords_reported*2));
		if (res<OK) 
		{
			logError(1, "%s %s: Cannot read the coordinates! ERROR %08X  \n", tag, __func__, res);
			gesture_coords_reported = ERROR_OP_NOT_ALLOW;
			return res;
		}
	
		//all the points of the gesture are stored in val
		for(i = 0;i < gesture_coords_reported;i++)
		{	//ignore first byte data because it is a dummy byte
			gesture_coordinates_x[i] =  (((u16) val[i*2 + 1 + 1]) & 0x0F) << 8 | (((u16) val[i*2 + 1])& 0xFF);
			gesture_coordinates_y[i] =  (((u16) val[gesture_coords_reported*2 + i*2 + 1 + 1]) & 0x0F) << 8 | (((u16) val[gesture_coords_reported*2 + i*2 + 1]) & 0xFF);					
		}

	
		logError(1, "%s %s: Reading Gesture Coordinates DONE!  \n", tag, __func__);
		return OK;
	
	}else{
		logError(1, "%s %s: The event passsed as argument is invalid! ERROR %08X  \n", tag, __func__, ERROR_OP_NOT_ALLOW);
		return ERROR_OP_NOT_ALLOW;
	}	
	
}

int getGestureCoords(u16 *x, u16 *y){
	x = gesture_coordinates_x;
	y = gesture_coordinates_y;
	logError(1, "%s %s: Number of gesture coordinates returned = %d  \n", tag, __func__, gesture_coords_reported);
	return gesture_coords_reported;
}


