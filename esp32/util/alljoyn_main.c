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

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

#define CONNECT_TIMEOUT    (1000 * 60)
#define UNMARSHAL_TIMEOUT  (1000 * 5)
#define METHOD_TIMEOUT     (100 * 10)
#define SLEEP_TIME         (1000 * 2)

static char fullServiceName[AJ_MAX_SERVICE_NAME_SIZE];

static AJ_Object *AppObjects = {NULL};

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void alljoyn_client_init();
static void alljoyn_service_init();
static void alljoyn_callback_handler(void *arg);
static void alljoyn_service_task(void *args);
static uint32_t getMessageId(alljoyn_obj_t *alljoyn_obj);
static void prepareServiceCallback(alljoyn_obj_t *alljoyn_obj);
static void prepareClientCallback(alljoyn_obj_t *alljoyn_obj);
static void alljoin_client_connect(alljoyn_obj_t *alljoyn_obj);
static bool isClientCall(alljoyn_obj_t *alljoyn_obj, uint32_t msgId);
static void prepareCallbackArguments(alljoyn_obj_t *alljoyn_obj, char *args_in, uint8_t num_args_in);
static void MakeMethodCall(AJ_BusAttachment* bus, uint32_t sessionId, alljoyn_obj_t *alljoyn_obj);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void alljoyn_start_client(alljoyn_obj_t *alljoyn_obj){
     
    alljoyn_client_init();
    
    alljoin_client_connect(alljoyn_obj);
}

void alljoyn_start_service(alljoyn_obj_t *alljoyn_obj){
     
    alljoyn_service_init();
   
    xTaskCreate(&alljoyn_service_task, "aj_service_task", 4096, (void *)alljoyn_obj, 5, NULL);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static void alljoyn_client_init(){
    
    AppObjects = getAlljoynObjects();
    
    AJ_Initialize();
    AJ_RegisterObjects(NULL, AppObjects);
    
}

static uint32_t getMessageId(alljoyn_obj_t *alljoyn_obj){
    
    return AJ_PRX_MESSAGE_ID(alljoyn_obj->obj_indices[0], alljoyn_obj->obj_indices[1], alljoyn_obj->obj_indices[2]);
}

static void MakeMethodCall(AJ_BusAttachment* bus, uint32_t sessionId, alljoyn_obj_t *alljoyn_obj)
{
    AJ_Status status;
    AJ_Message msg;

    status = AJ_MarshalMethodCall(bus, &msg, getMessageId(alljoyn_obj), fullServiceName, sessionId, 0, METHOD_TIMEOUT);

    if (status == AJ_OK) {
        for (int idx = 0; idx < alljoyn_obj->number_args; idx++){
            status = AJ_MarshalArg(&msg, &alljoyn_obj->aj_args[idx]);
            if (status != AJ_OK){
                break;
            }
        }
    }

    if (status == AJ_OK) {
        status = AJ_DeliverMsg(&msg);
    }
   
    printf("MakeMethodCall() resulted in a status of 0x%04x.\n", status);
}

static void alljoin_client_connect(alljoyn_obj_t *alljoyn_obj) {
    
    uint32_t prx_msg_id = 0;
    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint8_t done = FALSE;
    uint32_t sessionId = 0;
    
    prx_msg_id  = getMessageId(alljoyn_obj);
    
    printf("Connecting to alljoyn router ....\n");
    
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
                  
                    if (AJ_OK == status) {
                        
                        alljoyn_obj->msg = msg;
                        prepareClientCallback(alljoyn_obj);               
                       
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

    freeAjInterface();
    alljoyn_free_client();
    printf("Basic client exiting with status %d.\n", status);
}

static void alljoyn_service_init(){
    
    AppObjects = getAlljoynObjects();
    
    AJ_Initialize();
    AJ_RegisterObjects(AppObjects, NULL);
    
}

static void alljoyn_callback_handler(void *arg) {
    alljoyn_obj_t *self = arg;
    if (self->callback != MP_OBJ_NULL){
        mp_call_function_n_kw(self->callback,self->nparams,0, self->pyargs);
        free(self->pyargs);
    }
}

static void prepareCallbackArguments(alljoyn_obj_t *alljoyn_obj, char *args, uint8_t num_args){
    
    alljoyn_obj->nparams = num_args;
    alljoyn_obj->pyargs = (mp_obj_t *)malloc(sizeof(mp_obj_t) * num_args);
    
    for (uint8_t idx = 0; idx < num_args; idx++){
        
        if (args[idx] == AJ_ARG_STRING){
            char *str = NULL;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "s", &str);
            alljoyn_obj->pyargs[idx] = mp_obj_new_str(str, strlen(str), false);
        } else if (args[idx] == AJ_ARG_INT32) {
            int value = 0;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "i", &value);
            alljoyn_obj->pyargs[idx] = mp_obj_new_int(value);
        } else if (args[idx] == AJ_ARG_INT32 ){
            int value = 0;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "n", &value);
            alljoyn_obj->pyargs[idx] = mp_obj_new_int(value);
        } else if (args[idx] == AJ_ARG_UINT16) {
            unsigned int value = 0;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "q", &value);
            alljoyn_obj->pyargs[idx] = mp_obj_new_int_from_uint(value);
        } else if (args[idx] == AJ_ARG_UINT32){
            unsigned int value = 0;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "u", &value);
            alljoyn_obj->pyargs[idx] = mp_obj_new_int_from_uint(value);
        } else if (args[idx] == AJ_ARG_BOOLEAN){
            bool value = 0;
            AJ_UnmarshalArgs(&alljoyn_obj->msg, "b", &value);
            alljoyn_obj->pyargs[idx] = mp_obj_new_bool(value);
        } else {
            mp_raise_ValueError("Unsupported type\n");
        }
        
    }
}

static void prepareServiceCallback(alljoyn_obj_t *alljoyn_obj){
    
    char *method = NULL;
    uint8_t num_args_in = 0;
    uint8_t num_args_out = 0;
    char args_in[MAX_NUMBER_ARGS] = { 0 };
    
    memset(alljoyn_obj->replyArgs,0,MAX_NUMBER_ARGS);
    
    getParametersFromMsgId(alljoyn_obj->msg.msgId, &method, &alljoyn_obj->callback, true);
    parse_method_arguments(method, args_in, &num_args_in, alljoyn_obj->replyArgs, &num_args_out);
    prepareCallbackArguments(alljoyn_obj, args_in, num_args_in);
    
    mp_irq_queue_interrupt(alljoyn_callback_handler, (void *)alljoyn_obj);
}

static bool isClientCall(alljoyn_obj_t *alljoyn_obj, uint32_t msgId){
    
    for (int idx = 0; idx < alljoyn_obj->number_msgIds; idx++){
        if (alljoyn_obj->msgIds[idx] == msgId){
            return true;
        }
    }
    
    return false;
}

static void prepareClientCallback(alljoyn_obj_t *alljoyn_obj){
    
    char *method = NULL;
    uint8_t num_args_in = 0;
    uint8_t num_args_out = 0;
    char args_in[MAX_NUMBER_ARGS] = { 0 };
    char args_out[MAX_NUMBER_ARGS] = { 0 };
    
    getParametersFromMsgId(alljoyn_obj->msg.msgId, &method, &alljoyn_obj->callback, false);
    parse_method_arguments(method, args_in, &num_args_in, args_out, &num_args_out);
    prepareCallbackArguments(alljoyn_obj, args_out, num_args_out);
    
    mp_irq_queue_interrupt(alljoyn_callback_handler, (void *)alljoyn_obj);
}

static void alljoyn_service_task(void *args){

    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint32_t sessionId = 0;
    alljoyn_obj_t *alljoyn_obj = (alljoyn_obj_t *)args;
      
    printf("Creating alljoyn service ..... \n");
    
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
            } else if (msg.msgId == AJ_SIGNAL_SESSION_LOST_WITH_REASON){
                uint32_t id, reason;
                AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                printf("Session lost. ID = %u, reason = %u\n", id, reason);
            } else if (isClientCall(alljoyn_obj, msg.msgId)){
               
                printf("Received message with Id: %u\n", msg.msgId);
                
                alljoyn_obj->msg = msg;
                prepareServiceCallback(alljoyn_obj);
               
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

    freeAjInterface();
    alljoyn_free_service();
    printf("Basic service exiting with status %d.\n", status);
}