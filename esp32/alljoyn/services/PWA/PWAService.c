#include <stdio.h>
#include <alljoyn.h>
#include "PWAService.h"
#include "PWAUart.h"
 
#define PWA_OBJECT_INDEX                 0

#define AJ_PWA_MESSAGE_ID(p, i, m)       AJ_ENCODE_MESSAGE_ID(AJAPP_OBJECTS_LIST_INDEX, p, i, m)
#define AJ_PWA_PROPERTY_ID(p, i, m)      AJ_ENCODE_PROPERTY_ID(AJAPP_OBJECTS_LIST_INDEX, p, i, m)
    
#define PWA_STATUS_MSG                    AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 0)
#define PWA_TIMEOFDAY_MSG                 AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 1)
#define PWA_SALT_LEVEL_MSG                AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 2)
#define PWA_SALT_ALARM_MSG                AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 3)
#define PWA_WATER_AVAILABLE_MSG           AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 4)
#define PWA_AVG_WATER_USE_MSG             AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 5)
#define PWA_WATER_USE_TODAY_MSG           AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 6)
#define PWA_TOTAL_WATER_USED_MSG          AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 7)
#define PWA_CURR_FLOW_MSG                 AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 8)
#define PWA_DAYS_POWERED_MSG              AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 9)
#define PWA_TOTAL_RECHARGES_MSG           AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 10)
#define PWA_DAYS_BTW_RECHARGES_MSG        AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 11)
#define PWA_ERROR_CODE_MSG                AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 12)
#define PWA_WATER_HARDNESS_MSG            AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 13)
#define PWA_RESIN_ALERT_MSG               AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 14)
#define PWA_RECHARGE_TIME_MSG             AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 15)

#define PWA_SET_TIMEOFDAY_MSG             AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 16)
#define PWA_SET_SALT_LEVEL_MSG            AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 17)
#define PWA_SET_DAYS_BT_REGEN_MSG         AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 18)
#define PWA_SET_RECHARGE_TIME_MSG         AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 19)
#define PWA_SET_RECHARGE_NOWORTONIGHT_MSG AJ_PWA_MESSAGE_ID(PWA_OBJECT_INDEX, 0, 20)

const char PWAServicePath[] = "/PWA";
const char PWAInterfaceName[] = "org.alljoyn.PWA";

static const char* const PWAInterface[] = {
    "$org.alljoyn.PWA",
    "?Status value>y status>y",
    "?TimeOfDay time>i status>y",
    "?SaltLevel level>y status>y",
    "?LowSaltAlarm flag>b status>y",
    "?WaterAvailable gallons>i status>y",
    "?AverageWaterUsed liters>u status>y",
    "?WaterUsedToday liters>u status>y",
    "?TotalWaterUsed liters>u status>y",
    "?CurrentFlowRate flow>i status>y",
    "?DaysPoweredUp days>i status>y",
    "?TotalRecharges value>i status>y",
    "?DaysBetweenRecharges value>i status>y",
    "?ErrorCode error>i status>y",
    "?WaterHardness hardness>q status>y",
    "?ResinCleanAlert flag>i status>y",
    "?RechargeTime time>i status>y",
    "?SetTimeOfDay time<i status>y",
    "?SetSaltLevel level<i status>y",
    "?SetDaysBtRegenerations days<i status>y",
    "?SetRechargeTime time<i status>y",
    "?SetRechargeNowOrTonight time<i status>y",
    NULL
};

static const AJ_InterfaceDescription PWAInterfaces[] = {
    PWAInterface,
    NULL
};

AJ_Object AJApp_ObjectList[] = {
    { PWAServicePath, PWAInterfaces, AJ_OBJ_FLAG_ANNOUNCED },
    { NULL }
};

AJ_Status AJ_GetStatus(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char PWAStatus[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, PWA_GET_STATUS, PWAStatus, &cmdStatus); 
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_MarshalArgs(&reply, "yy", PWAStatus[0], cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetTimeOfDay(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char TimeOfDay[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, PWA_GET_TIMEOFDAY, TimeOfDay, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    int32_t currentTime = (TimeOfDay[3] << 24 ) + (TimeOfDay[2] << 16 ) + (TimeOfDay[1] << 8 ) + TimeOfDay[0];
    printf("Time of day %d %d %d %d %d\n", (int)TimeOfDay[3],(int)TimeOfDay[2], (int)TimeOfDay[1], (int)TimeOfDay[0], currentTime);
    status = AJ_MarshalArgs(&reply, "iy", currentTime, cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetSaltLevel(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char level[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, PWA_GET_SALT_LEVEL, level, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    //printf("Level: %d\n", level[4]);
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    //printf("Level %d %d %d %d\n", level[0], level[1],level[2],level[3]);
    status = AJ_MarshalArgs(&reply, "yy", level[0], cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetSaltAlarm(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char alarm[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, PWA_GET_SALT_ALARM, alarm, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_MarshalArgs(&reply, "by", alarm[0], cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetPWAUInt32(AJ_Message* msg, uint16_t getCommand)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char value[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, getCommand, value, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    uint32_t uint32Value = (value[3] << 24 ) + (value[3] << 16 ) + (value[1] << 8 ) + value[0];
    status = AJ_MarshalArgs(&reply, "uy", uint32Value, cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetPWAInt32(AJ_Message* msg, uint16_t getCommand)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char value[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, getCommand, value, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    int32_t int32Value = (value[3] << 24 ) + (value[2] << 16 ) + (value[1] << 8 ) + value[0];
    printf("AJ_GetPWAInt32: %u\n", int32Value);
    status = AJ_MarshalArgs(&reply, "iy", int32Value, cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_GetPWAUInt16(AJ_Message* msg, uint16_t getCommand)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    char value[PWA_RET_DATA_LEN] = {0};
    uint8_t cmdStatus = 0;
    
    status = get_pwa_data(PWA_COMMAND_GET1, getCommand, value, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
  
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    uint16_t int16Value =  (value[1] << 8 ) + value[0];
    status = AJ_MarshalArgs(&reply, "qy", int16Value, cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJ_Status AJ_SetPWAInt32(AJ_Message* msg, uint16_t setCommand)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    int32_t value = 0;
    uint8_t cmdStatus = 0;
   
    status = AJ_UnmarshalArgs(msg, "i", &value);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
   
    status = set_pwa_data(PWA_COMMAND_PUT1, setCommand, value, &cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_MarshalArgs(&reply, "y", cmdStatus);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    
ErrorExit:
    return status;
}

AJSVC_ServiceStatus PWA_MessageProcessor(AJ_BusAttachment* busAttachment, AJ_Message* msg, AJ_Status* msgStatus)
{    
    AJSVC_ServiceStatus serviceStatus = AJSVC_SERVICE_STATUS_HANDLED;

    if (*msgStatus == AJ_OK) {
        switch (msg->msgId) {
            
            case PWA_SALT_ALARM_MSG:
                printf("AJApp_MessageProcessor - Salt alarm\n");
                *msgStatus = AJ_GetSaltAlarm(msg);
                break;
                
            case PWA_STATUS_MSG:
                printf("AJApp_MessageProcessor - Status\n");
                *msgStatus = AJ_GetStatus(msg);
                break;
                
            case PWA_TIMEOFDAY_MSG:
                printf("AJApp_MessageProcessor - Time of day\n");
                *msgStatus = AJ_GetTimeOfDay(msg);
                break;
                
            case PWA_SALT_LEVEL_MSG:
                printf("AJApp_MessageProcessor - Salt level\n");
                *msgStatus = AJ_GetSaltLevel(msg);
                break;
            
            case PWA_WATER_AVAILABLE_MSG:
                printf("AJApp_MessageProcessor - Water Available\n");
                *msgStatus = AJ_GetPWAInt32(msg, PWA_GET_WATER_AVAILABLE);
                break;
                
            case PWA_AVG_WATER_USE_MSG:
                printf("AJApp_MessageProcessor - Average water used\n");
                *msgStatus = AJ_GetPWAUInt32(msg,PWA_GET_AVG_WATER_USED);
                break;
                
            case PWA_WATER_USE_TODAY_MSG:
                printf("AJApp_MessageProcessor - Water used today\n");
                *msgStatus = AJ_GetPWAUInt32(msg, PWA_GET_WATER_USED_TODAY);
                break;
                
            case PWA_TOTAL_WATER_USED_MSG:
                printf("AJApp_MessageProcessor - Total water used\n");
                *msgStatus = AJ_GetPWAUInt32(msg, PWA_GET_TOTAL_WATER_USED);
                break;
                
            case PWA_CURR_FLOW_MSG:
                printf("AJApp_MessageProcessor - Current Flow\n");
                *msgStatus = AJ_GetPWAInt32(msg, PWA_GET_FLOW_RATE);
                break;
                
            case PWA_DAYS_POWERED_MSG :
                printf("AJApp_MessageProcessor - Days powered up\n");
                *msgStatus = AJ_GetPWAInt32(msg, PWA_GET_DAYS_POWERED_UP);
                break;
                
            case PWA_TOTAL_RECHARGES_MSG:
                printf("AJApp_MessageProcessor - Total Recharges\n");
                *msgStatus = AJ_GetPWAInt32(msg, PWA_GET_TOTAL_RECHARGES);
                break;
                
            case PWA_DAYS_BTW_RECHARGES_MSG:
                printf("AJApp_MessageProcessor - Days between recharges\n");
                *msgStatus = AJ_GetPWAInt32(msg,PWA_GET_DAYS_BT_RECHARGES);
                break;
            
            case PWA_WATER_HARDNESS_MSG:
                printf("AJApp_MessageProcessor - Water Hardeness\n");
                *msgStatus = AJ_GetPWAUInt16(msg,PWA_GET_WATER_HARDNESS);
                break;
                
            case PWA_ERROR_CODE_MSG :
                printf("AJApp_MessageProcessor - Error Code\n");
                *msgStatus = AJ_GetPWAInt32(msg,PWA_GET_ERROR_CODE);
                break;
                
            case PWA_RESIN_ALERT_MSG:
                printf("AJApp_MessageProcessor - Resin Alert\n");
                *msgStatus = AJ_GetPWAInt32(msg,PWA_GET_RESIN_ALERT);
                break;
                
            case PWA_RECHARGE_TIME_MSG:
                printf("AJApp_MessageProcessor - Recharge time\n");
                *msgStatus = AJ_GetPWAInt32(msg,PWA_GET_RECHARGE_TIME);
                break;
                
            case PWA_SET_TIMEOFDAY_MSG:
                printf("AJApp_MessageProcessor - Set time of day\n");
                *msgStatus = AJ_SetPWAInt32(msg, PWA_SET_PRESENT_TIME_OF_DAY);
                break;
            
            case PWA_SET_SALT_LEVEL_MSG:
                printf("AJApp_MessageProcessor - Set Salt level\n");
                *msgStatus = AJ_SetPWAInt32(msg, PWA_SET_SALT_LEVEL);
                break;
                
            case PWA_SET_DAYS_BT_REGEN_MSG:
                printf("AJApp_MessageProcessor - Set Days between regenerations\n");
                *msgStatus = AJ_SetPWAInt32(msg, PWA_SET_MAX_DAYS_BT_REGEN);
                break;
                
            case PWA_SET_RECHARGE_TIME_MSG:
                printf("AJApp_MessageProcessor - Set Recharge time\n");
                *msgStatus = AJ_SetPWAInt32(msg, PWA_SET_RECHARGE_TIME);
                break;
                
            case PWA_SET_RECHARGE_NOWORTONIGHT_MSG:
                printf("AJApp_MessageProcessor - Set Recharge now or tonight\n");
                *msgStatus = AJ_SetPWAInt32(msg, PWA_SET_RECHARGE_NOWORTONIGTH);
                break;
                
            default:
                printf("AJApp_MessageProcessor AJSVC_SERVICE_STATUS_NOT_HANDLED\n");
                serviceStatus = AJSVC_SERVICE_STATUS_NOT_HANDLED;
                break;
        }
    } else {
        serviceStatus = AJSVC_SERVICE_STATUS_NOT_HANDLED;
    }

    return serviceStatus;
}

AJ_Status AJAPP_ConnectedHandler(AJ_BusAttachment* bus)
{
    AJ_Status status = AJ_OK;
    return status;
}