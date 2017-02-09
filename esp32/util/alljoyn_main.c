/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpexception.h"
#include "py/runtime.h"
#include "py/nlr.h"
#include "mpirq.h"
#include "py/mphal.h"

#include "aj_debug.h"
#include "alljoyn.h"
#include "alljoyn_main.h"
#include "alljoyn_interface.h"

#define CONNECT_TIMEOUT    (1000 * 60)
#define UNMARSHAL_TIMEOUT  (1000 * 5)
#define METHOD_TIMEOUT     (100 * 10)
#define SLEEP_TIME         (1000 * 2)

static char fullServiceName[AJ_MAX_SERVICE_NAME_SIZE];

static AJ_Object *AppObjects = {NULL};

static void alljoyn_client_init(){
    
    AppObjects = getAlljoynObjects();
    
    AJ_Initialize();
    AJ_RegisterObjects(NULL, AppObjects);
    
}

static uint32_t getMessageId(alljoyn_obj_t *alljoyn_obj){
    
    return AJ_PRX_MESSAGE_ID(alljoyn_obj->obj_indices[0], alljoyn_obj->obj_indices[1], alljoyn_obj->obj_indices[2]);

}

void MakeMethodCall(AJ_BusAttachment* bus, uint32_t sessionId, alljoyn_obj_t *alljoyn_obj)
{
    AJ_Status status;
    AJ_Message msg;

    status = AJ_MarshalMethodCall(bus, &msg, getMessageId(alljoyn_obj), fullServiceName, sessionId, 0, METHOD_TIMEOUT);

    if (status == AJ_OK) {
        status = AJ_MarshalArgs(&msg, "s", "Hello ");
        status = AJ_MarshalArgs(&msg, "s", "Oliver");
    }

    if (status == AJ_OK) {
        status = AJ_DeliverMsg(&msg);
    }
   
    printf("MakeMethodCall() resulted in a status of 0x%04x.\n", status);
}

void alljoin_client_connect(alljoyn_obj_t *alljoyn_obj) {
    
    uint32_t prx_msg_id = 0;
    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint8_t done = FALSE;
    uint32_t sessionId = 0;
    
    prx_msg_id  = getMessageId(alljoyn_obj);  
    
    while (!done) {
        AJ_Message msg;

        if (!connected) {
            status = AJ_StartClientByName(&bus,
                                          NULL,
                                          CONNECT_TIMEOUT,
                                          FALSE,
                                          alljoyn_obj->service_name,
                                          alljoyn_obj->service_port,
                                          &sessionId,
                                          NULL,
                                          fullServiceName);

            if (status == AJ_OK) {
                printf("StartClient returned %d, sessionId=%u.\n", status, sessionId);
                connected = TRUE;

                MakeMethodCall(&bus, sessionId, alljoyn_obj);
            } else {
                printf("StartClient returned 0x%04x.\n", status);
                break;
            }
        }

        status = AJ_UnmarshalMsg(&bus, &msg, UNMARSHAL_TIMEOUT);

        if (AJ_ERR_TIMEOUT == status) {
            continue;
        }

        if (AJ_OK == status) {
            if  (msg.msgId == AJ_REPLY_ID(prx_msg_id)){ 
           
                if (msg.hdr->msgType == AJ_MSG_METHOD_RET) {
                    AJ_Arg arg;

                    status = AJ_UnmarshalArg(&msg, &arg);

                    if (AJ_OK == status) {
                        printf("'%s.%s' (path='%s') returned '%s'.\n", fullServiceName, "cat",
                                         "/sample", arg.val.v_string);
                        done = TRUE;
                    } else {
                        printf("AJ_UnmarshalArg() returned status %d.\n", status);
                        /* Try again because of the failure. */
                        MakeMethodCall(&bus, sessionId, alljoyn_obj);
                    }
                } else {
                    const char* info = "";
                    if (AJ_UnmarshalArgs(&msg, "s", &info) == AJ_OK) {
                        printf("Method call returned error %s (%s)\n", msg.error, info);
                    } else {
                        printf("Method call returned error %s\n", msg.error);
                    }
                    done = TRUE;
                }
            }
            else if (msg.msgId == AJ_SIGNAL_SESSION_LOST_WITH_REASON) {
                /* A session was lost so return error to force a disconnect. */
                {
                    uint32_t id, reason;
                    AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                    printf("Session lost. ID = %u, reason = %u\n", id, reason);
                }
                status = AJ_ERR_SESSION_LOST;
            }
            else{
                /* Pass to the built-in handlers. */
                status = AJ_BusHandleBusMessage(&msg);
            }
        }

        /* Messages MUST be discarded to free resources. */
        AJ_CloseMsg(&msg);

        if ((status == AJ_ERR_READ) || (status == AJ_ERR_WRITE) || (status == AJ_ERR_SESSION_LOST)) {
            printf("AllJoyn disconnect.\n");
            AJ_Disconnect(&bus);
            exit(0);
        }
    }

    printf("Basic client exiting with status %d.\n", status);
}

void alljoyn_start_client(alljoyn_obj_t *alljoyn_obj){
     
    alljoyn_client_init();
    
    alljoin_client_connect(alljoyn_obj);
}

static void alljoyn_service_init(){
    
    AppObjects = getAlljoynObjects();
    
    AJ_Initialize();
    AJ_RegisterObjects(AppObjects, NULL);
    
}

void indirectCallback(void *arg){
    printf("Oliver this si called \n");
    alljoyn_obj_t *alljoyn_obj = (alljoyn_obj_t *)arg;
    mp_call_function_0(alljoyn_obj->callbacks[1]);
}

//void IRAM_ATTR mp_irq_queue_interrupt2(void (* handler)(void *), void *arg) {
//    mp_callback_obj_t cb = {.handler = handler, .arg = arg};
//    xQueueSendFromISR(callbackQueue, &cb, NULL);
//}

static void alljoyn_service_task(void *args){

    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint32_t sessionId = 0;
    alljoyn_obj_t *alljoyn_obj = (alljoyn_obj_t *)args;
    
    uint32_t msgId = AJ_APP_MESSAGE_ID(0, 0, 1);
    
    //printf("++++++++++++++++ SP: %d, SN: %s",alljoyn_obj.service_port, alljoyn_obj.service_name);
     
    while (TRUE) {
        AJ_Message msg;
           
        if (!connected) {
            status = AJ_StartService(&bus,
                                     NULL,
                                     CONNECT_TIMEOUT,
                                     FALSE,
                                     alljoyn_obj->service_port,
                                     alljoyn_obj->service_name,
                                     AJ_NAME_REQ_DO_NOT_QUEUE,
                                     NULL);

            if (status != AJ_OK) {
                continue;
            }

            printf("StartService returned %d, session_id=%u\n", status, sessionId);
            connected = TRUE;
        }

        status = AJ_UnmarshalMsg(&bus, &msg, UNMARSHAL_TIMEOUT);

        if (AJ_ERR_TIMEOUT == status) {
            continue;
        }

        if (AJ_OK == status) {
           
            if (msg.msgId == AJ_METHOD_ACCEPT_SESSION) {
                uint16_t port;
                char* joiner;
                AJ_UnmarshalArgs(&msg, "qus", &port, &sessionId, &joiner);
                status = AJ_BusReplyAcceptSession(&msg, TRUE);
                printf("Accepted session session_id=%u joiner=%s\n", sessionId, joiner);
            } else if (msg.msgId == msgId){
                
               //status = AppHandleCat(&msg);
                if (alljoyn_obj->callbacks[0] != MP_OBJ_NULL){
                    printf("+++++++++++++++++++++++++++++++++++++++++++++ calling\n");
                    nlr_buf_t nlr;
                    if (nlr_push(&nlr) == 0) {
                        mp_call_function_0(MP_OBJ_FROM_PTR(alljoyn_obj->callbacks[0]));
                        nlr_pop();
                    }
                   
                    printf("We are back hurray ++++++++++++++++++++++\n");
                    //callCallback();
                    //mp_irq_queue_interrupt(indirectCallback, (void *)alljoyn_obj);
                    //
                    //AJ_Sleep(100);
                }
            } else if (msg.msgId == AJ_SIGNAL_SESSION_LOST_WITH_REASON){
                uint32_t id, reason;
                AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                printf("Session lost. ID = %u, reason = %u\n", id, reason);
            } else {
                /* Pass to the built-in handlers. */
                status = AJ_BusHandleBusMessage(&msg);
            }
            
        }

        /* Messages MUST be discarded to free resources. */
        AJ_CloseMsg(&msg);

        if ((status == AJ_ERR_READ) || (status == AJ_ERR_WRITE)) {
            printf("AllJoyn disconnect.\n");
            AJ_Disconnect(&bus);
            connected = FALSE;

            /* Sleep a little while before trying to reconnect. */
            AJ_Sleep(SLEEP_TIME);
        }
    }

    printf("Basic service exiting with status %d.\n", status);
}


static void callBackTask(void *arg){
    
    alljoyn_obj_t *alljoyn_obj1 = (alljoyn_obj_t *)arg;
     
    while (true){
        
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
          mp_call_function_0(alljoyn_obj1->callbacks[1]);
          nlr_pop();
        }
        
        mp_hal_delay_ms(15000);
    }
    
}

STATIC void alljoyn_callback_handler(void *arg) {
    alljoyn_obj_t *self = arg;

    //if (self->handler != mp_const_none) {
        mp_call_function_0(self->callbacks[1]);
    //}
}

void alljoyn_start_service(alljoyn_obj_t *alljoyn_obj){
     
    //alljoyn_service_init();
    
    //mp_call_function_0(alljoyn_obj->callbacks[1]);
    //alljoyn_service_task((void *)alljoyn_obj);
    mp_irq_queue_interrupt(alljoyn_callback_handler, (void *)alljoyn_obj);
    //xTaskCreate(&callBackTask, "callback_task", 4096, (void *)alljoyn_obj, 5, NULL);
    //xTaskCreate(&alljoyn_service_task, "aj_service_task", 4096, (void *)alljoyn_obj, 5, NULL);
}