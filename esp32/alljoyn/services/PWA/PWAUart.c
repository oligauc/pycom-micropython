#include <string.h>
#include <stdio.h>
#include <alljoyn.h>
#include "PWAUart.h"
#include "machuart.h"

extern mach_uart_obj_t mach_uart_obj[];

static unsigned checksum(void *buffer, size_t len, unsigned int seed)
{
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;

      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}

AJ_Status parseGetData(uint8_t cmd, uint16_t expected_id, char *reply, char *value)
{
    uint16_t received_id = (reply[1] << 8 ) + reply[2];
    
    printf("expected id %d received id %d, cmd %d", expected_id, received_id, cmd);
    if ((reply[0] == PWA_COMMAND_DATA) || (expected_id = received_id)){
        memcpy(value, &reply[3], PWA_RET_DATA_LEN);
        return AJ_OK;
    } 
    
    return AJ_ERR_FAILURE;
}

AJ_Status get_pwa_data(uint8_t cmd, uint16_t data_id, char *value, uint8_t *cmdStatus)
{
    int *errcode = NULL;
    int readChars = 0;
    char msg[PWA_GET_MSG_LEN] = {0};
    char reply[PWA_GET_DATA_LEN] = {0};
    uint32_t timeout = PWA_CMD_TIMEOUT;
    
    msg[0] = cmd;
    msg[1] = (data_id >> 8) & 0xFF;
    msg[2] = data_id & 0xFF;
    msg[3] = checksum(msg, PWA_GET_MSG_LEN - 1, PWA_CHECKSUM_SEED);
    
    printf("get msg: %d, %d, %d, %d\n", msg[0], msg[1], msg[2], msg[3]);
    
    if (!uart_tx_strn(&mach_uart_obj[PWA_UART_ID], msg, PWA_GET_MSG_LEN)) {
        return AJ_ERR_FAILURE;
    }
    
    while (1){
        if (uart_rx_any(&mach_uart_obj[PWA_UART_ID])){
            reply[readChars] = uart_rx_char(&mach_uart_obj[PWA_UART_ID]);
            if ((reply[0] == PWA_COMMAND_DATA) && (++readChars == PWA_GET_DATA_LEN)){
                printf("Received: %d %d %d %d %d %d %d %d\n", reply[0],reply[1],reply[2],reply[3],reply[4],reply[5],reply[6],reply[7]);
                *cmdStatus = reply[0];
                return parseGetData(cmd, data_id, reply, value);
            } else if ((reply[0] == PWA_RET_NFND) || (reply[0] == PWA_RET_NTAL) || (reply[0] == PWA_RET_DVOL) || (reply[0] == PWA_RET_BAD)) {
                *cmdStatus = reply[0];
                return AJ_OK;
            }
        } else {
            AJ_Sleep(1);
            timeout--;
            
            if (timeout < 0){
                break;
            } 
        }
    }
   
    return AJ_ERR_FAILURE;
}

AJ_Status set_pwa_data(uint8_t cmd, uint16_t data_id, int32_t value, uint8_t *cmdStatus)
{
    int readChars = 0;
    char msg[PWA_SET_MSG_LEN] = {0};
    char reply[PWA_GET_DATA_LEN] = {0};
    uint32_t timeout = PWA_CMD_TIMEOUT;
    
    msg[0] = cmd;
    msg[1] = (data_id >> 8) & 0xFF;
    msg[2] = data_id & 0xFF;
    msg[3] = (value >> 24) & 0xFF; 
    msg[4] = (value >> 16) & 0xFF;
    msg[5] = (value >> 8) & 0xFF;
    msg[6] = value & 0xFF;
    msg[7] = checksum(msg, PWA_SET_MSG_LEN - 1, PWA_CHECKSUM_SEED);
    
    printf("set msg: %d, %d, %d, %d, %d, %d, %d, %d\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
    
    if (!uart_tx_strn(&mach_uart_obj[PWA_UART_ID], msg, PWA_SET_MSG_LEN)) {
        return AJ_ERR_FAILURE;
    }
    
    while (1){
        if (uart_rx_any(&mach_uart_obj[PWA_UART_ID])){
            reply[readChars] = uart_rx_char(&mach_uart_obj[PWA_UART_ID]);
            if (((reply[0] == PWA_RET_DONE) || (reply[0] == PWA_RET_NFND) || (reply[0] == PWA_RET_NTAL) || (reply[0] == PWA_RET_DVOL) || (reply[0] == PWA_RET_BAD)) 
                    && (++readChars == PWA_SET_REPLY_MSG_LEN)){
                *cmdStatus = reply[0];
                return AJ_OK;
            } 
        } else {
            AJ_Sleep(1);
            timeout--;
            
            if (timeout < 0){
                break;
            } 
        }
    }
   
    return AJ_ERR_FAILURE;
}

