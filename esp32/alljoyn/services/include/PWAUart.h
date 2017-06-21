/* 
 * File:   PWAUart.h
 * Author: gauci
 *
 * Created on June 13, 2017, 1:27 PM
 */

#ifndef PWAUART_H
#define	PWAUART_H

#include <stdint.h>
#include "py/obj.h"

#define PWA_UART_ID                   0x01
#define PWA_CMD_TIMEOUT               0x7D0

#define PWA_GET_MSG_LEN               0x04
#define PWA_SET_MSG_LEN               0x08
#define PWA_SET_DATA_LEN              0x04
#define PWA_RET_DATA_LEN              0x04 
#define PWA_GET_DATA_LEN              0x08
#define PWA_SET_REPLY_MSG_LEN         0x02
#define PWA_GET_ERR_REPLY_LEN         0x02
#define PWA_CHECKSUM_SEED             0xA5


/* PWA Command List*/
#define PWA_COMMAND_GET1              0x30
#define PWA_COMMAND_GETALL            0x31
#define PWA_COMMAND_GETWORD           0x32
#define PWA_COMMAND_DATA              0x33
#define PWA_COMMAND_WORD              0x34
#define PWA_COMMAND_PUT1              0x3B
#define PWA_COMMAND_DONE              0x37

/* PWA Return Messages */
#define PWA_RET_NFND                  0x35
#define PWA_RET_NTAL                  0x36
#define PWA_RET_DONE                  0x37
#define PWA_RET_BAD                   0x38
#define PWA_RET_DVOL                  0x3D

/* PWA Get */
#define PWA_GET_SALT_ALARM            01
#define PWA_GET_STATUS                02
#define PWA_GET_AVG_WATER_USED        07
#define PWA_GET_SALT_LEVEL            11
#define PWA_GET_WATER_HARDNESS        16
#define PWA_GET_WATER_USED_TODAY      18
#define PWA_GET_TOTAL_WATER_USED      27
#define PWA_GET_RECHARGE_TIME         28
#define PWA_GET_WATER_AVAILABLE       10029
#define PWA_GET_DAYS_BT_RECHARGES     10040
#define PWA_GET_TOTAL_RECHARGES       10045
#define PWA_GET_DAYS_POWERED_UP       10051
#define PWA_GET_TIMEOFDAY             10052
#define PWA_GET_ERROR_CODE            10055
#define PWA_GET_FLOW_RATE             10080
#define PWA_GET_RESIN_ALERT           10094

/* PWA Set */
#define PWA_SET_RECHARGE_NOWORTONIGTH 02 
#define PWA_SET_SALT_LEVEL            11
#define PWA_SET_RECHARGE_TIME         28
#define PWA_SET_MAX_DAYS_BT_REGEN     10040
#define PWA_SET_PRESENT_TIME_OF_DAY   10052

AJ_Status get_pwa_data(uint8_t cmd, uint16_t data_id, char *value, uint8_t *cmdStatus);

AJ_Status set_pwa_data(uint8_t cmd, uint16_t data_id, int32_t value, uint8_t *cmdStatus);


#endif	/* PWAUART_H */

