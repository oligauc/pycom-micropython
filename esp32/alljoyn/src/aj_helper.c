/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

/**
 * Per-module definition of the current module for debug logging.  Must be defined
 * prior to first inclusion of aj_debug.h
 */
#define AJ_MODULE HELPER

#include <alljoyn.h>
#include <aj_helper.h>
#include <aj_link_timeout.h>
#include <aj_debug.h>
#include <aj_config.h>
#include <aj_security.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include "py/mphal.h"

#include "alljoyn_interface.h"

/**
 * Turn on per-module debug printing by setting this variable to non-zero value
 * (usually in debugger).
 */
#ifndef NDEBUG
uint8_t dbgHELPER = 1;
#endif

#define ALLJOIN_TASK_STACK_SIZE			2048
#define ALLJOIN_TASK_PRIO			(configMAX_PRIORITIES - 4)
#define ALLJOIN_TASK_NAME			"alljoin_task"
#define ALLJOIN_DEAMON_NAME                     "alljoin_deamon"
#define ALLJOIN_CONNECT_TIMEOUT                  (1000 * 60)
#define ALLJOIN_SERVICE_PORT                     25
#define ALLJOIN_SERVICE_NAME                     "alljoyn.Bus.service"

#define    WIFI_CHANNEL_MAX                (13)
#define    WIFI_CHANNEL_SWITCH_INTERVAL    (500)

typedef struct {
    unsigned frame_ctrl:16;
    unsigned duration_id:16;
    uint8_t addr1[6]; /* receiver address */
    uint8_t addr2[6]; /* sender address */
    uint8_t addr3[6]; /* filtering address */
    unsigned sequence_ctrl:16;
    uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
    wifi_ieee80211_mac_hdr_t hdr;
    uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;


/**
 *  Type to describe pending timers
 */
typedef struct {
    TimeoutHandler handler; /**< The callback handler */
    void* context;          /**< A context pointer passed in by the user */
    uint32_t abs_time;      /**< The absolute time when this timer will fire */
    uint32_t repeat;        /**< The amount of time between timer events */
} Timer;

static Timer Timers[AJ_MAX_TIMERS] = {
    { NULL }
};

//////////////////////////////////////////////////////////////////
/*static const char ServiceName[] = "org.alljoyn.Bus.sample";
static const char ServicePath[] = "/sample";
static const uint16_t ServicePort = 25;

static char fullServiceName[AJ_MAX_SERVICE_NAME_SIZE];

uint8_t dbgBASIC_CLIENT = 1;

static const char* const sampleInterface[] = {
    "org.alljoyn.Bus.sample",   
    "?Dummy foo<i",             
    "?Dummy2 fee<i",            
    "?cat inStr1<s inStr2<s outStr>s", 
    NULL
};


static const AJ_InterfaceDescription sampleInterfaces[] = {
    sampleInterface,
    NULL
};

static const AJ_Object AppObjects[] = {
    { ServicePath, sampleInterfaces },
    { NULL }
};

AJ_Object *AppObjects = NULL;

char* sampleInterface2[] = {   
    "?Dummy foo<i",             
    "?Dummy2 fee<i",            
    "?cat inStr1<s inStr2<s outStr>s", 
};



#define BASIC_CLIENT_CAT AJ_PRX_MESSAGE_ID(0, 0, 2)

#define CONNECT_TIMEOUT    (1000 * 60)
#define UNMARSHAL_TIMEOUT  (1000 * 5)
#define METHOD_TIMEOUT     (100 * 10)

void MakeMethodCall(AJ_BusAttachment* bus, uint32_t sessionId)
{
    AJ_Status status;
    AJ_Message msg;

    status = AJ_MarshalMethodCall(bus, &msg, BASIC_CLIENT_CAT, fullServiceName, sessionId, 0, METHOD_TIMEOUT);
    printf("+++++++ AJ_MarshalMethodCall Nok: %d\n", status);
    
    if (status == AJ_OK) {
        status = AJ_MarshalArgs(&msg, "ss", "Hello ", "World!");
        printf("+++++++ AJ_MarshalArgs ok: %d\n", status);
    }

    if (status == AJ_OK) {
        status = AJ_DeliverMsg(&msg);
        printf("+++++ MakeMethodCall status: %d\n", status);
    }

    printf("MakeMethodCall() resulted in a status of 0x%04x.\n", status);
}

static void alljoin_task()
{
    //addObject("/sample", "org.alljoyn.Bus.sample",sampleInterface2, 3);
    AppObjects = getAlljoynObjects();
     
    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint8_t done = FALSE;
    uint32_t sessionId = 0;

    AJ_Initialize();
    AJ_PrintXML(AppObjects);
    AJ_RegisterObjects(NULL, AppObjects);

    while (!done) {
        AJ_Message msg;

        if (!connected) {
            printf("++++++++++ Connected\n");
            status = AJ_StartClientByName(&bus,
                                          NULL,
                                          CONNECT_TIMEOUT,
                                          FALSE,
                                          ServiceName,
                                          ServicePort,
                                          &sessionId,
                                          NULL,
                                          fullServiceName);

            if (status == AJ_OK) {
                printf("StartClient returned %d, sessionId=%u.\n", status, sessionId);
                connected = TRUE;
                MakeMethodCall(&bus, sessionId);
            } else {
                printf("++++++++++ Client start error\n");
                AJ_InfoPrintf(("StartClient returned 0x%04x.\n", status));
                break;
            }
        }

        status = AJ_UnmarshalMsg(&bus, &msg, UNMARSHAL_TIMEOUT);

        if (AJ_ERR_TIMEOUT == status) {
            continue;
        }

        if (AJ_OK == status) {
            switch (msg.msgId) {
            case AJ_REPLY_ID(BASIC_CLIENT_CAT):
                if (msg.hdr->msgType == AJ_MSG_METHOD_RET) {
                    AJ_Arg arg;

                    status = AJ_UnmarshalArg(&msg, &arg);

                    if (AJ_OK == status) {
                        printf("%s.%s (path=%s) returned %s.\n", fullServiceName, "cat",
                                         ServicePath, arg.val.v_string);
                        done = TRUE;
                    } else {
                        printf("AJ_UnmarshalArg() returned status %d.\n", status);
                        
                        MakeMethodCall(&bus, sessionId);
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
                break;

            case AJ_SIGNAL_SESSION_LOST_WITH_REASON:
                
                {
                    uint32_t id, reason;
                    AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                    printf("Session lost. ID = %u, reason = %u\n", id, reason);
                }
                status = AJ_ERR_SESSION_LOST;
                break;

            default:
                
                status = AJ_BusHandleBusMessage(&msg);
                break;
            }
        }

        
        AJ_CloseMsg(&msg);

        if ((status == AJ_ERR_READ) || (status == AJ_ERR_WRITE) || (status == AJ_ERR_SESSION_LOST)) {
            printf("AllJoyn disconnect.\n");
            AJ_Disconnect(&bus);
            exit(0);
        }
    }

    printf("Basic client exiting with status %d.\n", status);
}

///////////////////////////////////////////////////
#define CONNECT_ATTEMPTS   10
static const char ServiceName[] = "org.alljoyn.Bus.sample";
static const char ServicePath[] = "/sample";
static const uint16_t ServicePort = 25;

uint8_t dbgBASIC_SERVICE = 0;

static const char* const sampleInterface[] = {
    "org.alljoyn.Bus.sample",   
    "?Dummy foo<i",             
    "?cat inStr1<s inStr2<s outStr>s", 
    NULL
};

static const AJ_InterfaceDescription sampleInterfaces[] = {
    sampleInterface,
    NULL
};

static const AJ_Object AppObjects[] = {
    { ServicePath, sampleInterfaces },
    { NULL }
};


#define BASIC_SERVICE_CAT AJ_APP_MESSAGE_ID(0, 0, 1)


static uint8_t asyncForm = TRUE;

static AJ_Status AppHandleCat(AJ_Message* msg)
{
    const char* string0;
    const char* string1;
    char buffer[256];
    AJ_Message reply;
    AJ_Arg replyArg;

    AJ_UnmarshalArgs(msg, "ss", &string0, &string1);

    strncpy(buffer, string0, ArraySize(buffer));
    buffer[ArraySize(buffer) - 1] = '\0';
    strncat(buffer, string1, ArraySize(buffer) - strlen(buffer) - 1);
    if (asyncForm) {
        AJ_MsgReplyContext replyCtx;
        AJ_CloseMsgAndSaveReplyContext(msg, &replyCtx);
        AJ_MarshalReplyMsgAsync(&replyCtx, &reply);
    } else {
        AJ_MarshalReplyMsg(msg, &reply);
    }
    AJ_InitArg(&replyArg, AJ_ARG_STRING, 0, buffer, 0);
    AJ_MarshalArg(&reply, &replyArg);

    return AJ_DeliverMsg(&reply);
}

#define CONNECT_TIMEOUT     (1000 * 60)
#define UNMARSHAL_TIMEOUT   (1000 * 5)
#define SLEEP_TIME          (1000 * 2)

static void alljoin_task(void *arg)
{
    AJ_Status status = AJ_OK;
    AJ_BusAttachment bus;
    uint8_t connected = FALSE;
    uint32_t sessionId = 0;

   
    AJ_Initialize();

   
    AJ_PrintXML(AppObjects);

    AJ_RegisterObjects(AppObjects, NULL);

    while (TRUE) {
        AJ_Message msg;

        if (!connected) {
            status = AJ_StartService(&bus,
                                     NULL,
                                     CONNECT_TIMEOUT,
                                     FALSE,
                                     ServicePort,
                                     ServiceName,
                                     AJ_NAME_REQ_DO_NOT_QUEUE,
                                     NULL);

            if (status != AJ_OK) {
                continue;
            }

            AJ_InfoPrintf(("StartService returned %d, session_id=%u\n", status, sessionId));
            connected = TRUE;
        }

        status = AJ_UnmarshalMsg(&bus, &msg, UNMARSHAL_TIMEOUT);

        if (AJ_ERR_TIMEOUT == status) {
            continue;
        }

        if (AJ_OK == status) {
            switch (msg.msgId) {
            case AJ_METHOD_ACCEPT_SESSION:
                {
                    uint16_t port;
                    char* joiner;
                    AJ_UnmarshalArgs(&msg, "qus", &port, &sessionId, &joiner);
                    status = AJ_BusReplyAcceptSession(&msg, TRUE);
                    AJ_InfoPrintf(("Accepted session session_id=%u joiner=%s\n", sessionId, joiner));
                }
                break;

            case BASIC_SERVICE_CAT:
                status = AppHandleCat(&msg);
                break;

            case AJ_SIGNAL_SESSION_LOST_WITH_REASON:
                {
                    uint32_t id, reason;
                    AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                    AJ_AlwaysPrintf(("Session lost. ID = %u, reason = %u\n", id, reason));
                }
                break;

            default:
                
                status = AJ_BusHandleBusMessage(&msg);
                break;
            }
        }

        
        AJ_CloseMsg(&msg);

        if ((status == AJ_ERR_READ) || (status == AJ_ERR_WRITE)) {
            AJ_AlwaysPrintf(("AllJoyn disconnect.\n"));
            AJ_Disconnect(&bus);
            connected = FALSE;

            AJ_Sleep(SLEEP_TIME);
        }
    }

    AJ_AlwaysPrintf(("Basic service exiting with status %d.\n", status));

} */

/*void wifi_sniffer_set_channel(uint8_t channel)
{
    
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

const char *
wifi_sniffer_packet_type2str(wifi_promiscuous_pkt_type_t type)
{
    switch(type) {
    case WIFI_PKT_CTRL: return "CTRL";
    case WIFI_PKT_MGMT: return "MGMT";
    case WIFI_PKT_DATA: return "DATA";
    default:    
    case WIFI_PKT_MISC: return "MISC";
    }
}

void
wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type)
{

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buff;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    
   
    printf("PACKET TYPE=%s, CHAN=%02d, RSSI=%02d,"
        " ADDR1=%02x:%02x:%02x:%02x:%02x:%02x,"
        " ADDR2=%02x:%02x:%02x:%02x:%02x:%02x,"
        " ADDR3=%02x:%02x:%02x:%02x:%02x:%02x\n", 
        wifi_sniffer_packet_type2str(type),
        ppkt->rx_ctrl.channel,
        ppkt->rx_ctrl.rssi,
        
        hdr->addr1[0],hdr->addr1[1],hdr->addr1[2],
        hdr->addr1[3],hdr->addr1[4],hdr->addr1[5],
        
        hdr->addr2[0],hdr->addr2[1],hdr->addr2[2],
        hdr->addr2[3],hdr->addr2[4],hdr->addr2[5],
        
        hdr->addr3[0],hdr->addr3[1],hdr->addr3[2],
        hdr->addr3[3],hdr->addr3[4],hdr->addr3[5]
    );
}

void sniffpacket(void *arg){
    uint8_t channel = 1;
    while (true) {
        vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
        wifi_sniffer_set_channel(channel);
        channel = (channel % WIFI_CHANNEL_MAX) + 1;
    }
    
}*/
///////////////////////////////////////////////////////////
/////////////////////////////////////////

AJ_Status init_allJoin(void) {
    
    //mp_hal_delay_ms(8000);
    
    AJ_Status status = AJ_OK;
    
     //esp_wifi_set_promiscuous(true);
    //esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
   
    //xTaskCreate(&alljoin_task, ALLJOIN_TASK_NAME, 2048, NULL, 5, NULL);
    //xTaskCreate(&sniffpacket, ALLJOIN_TASK_NAME, 4096, NULL, 5, NULL);
    //alljoin_task();
    
    return status;
}

static uint32_t RunExpiredTimers(uint32_t now)
{
    uint32_t i = 0;
    uint32_t next = (uint32_t) -1;

    for (; i < AJ_MAX_TIMERS; ++i) {
        Timer* timer = Timers + i;
        if (timer->handler != NULL && timer->abs_time <= now) {
            (timer->handler)(timer->context);

            if (timer->repeat) {
                timer->abs_time += timer->repeat;
            } else {
                memset(timer, 0, sizeof(Timer));
            }
        }

        // track the next time we need to run
        if (timer->handler != NULL && next > timer->abs_time) {
            next = timer->abs_time;
        }
    }

    // return the next timeout that will run
    return next;
}

uint32_t AJ_SetTimer(uint32_t relative_time, TimeoutHandler handler, void* context, uint32_t repeat)
{
    uint32_t i;
    for (i = 0; i < AJ_MAX_TIMERS; ++i) {
        Timer* timer = Timers + i;
        // need to find an available timer slot
        if (timer->handler == NULL) {
            AJ_Time start = { 0, 0 };
            uint32_t now = AJ_GetElapsedTime(&start, FALSE);
            timer->handler = handler;
            timer->context = context;
            timer->repeat = repeat;
            timer->abs_time = now + relative_time;
            return i + 1;
        }
    }

    // available slot not found!
    AJ_ErrPrintf(("AJ_SetTimer(): Slot not found\n"));
    return 0;
}

void AJ_CancelTimer(uint32_t id)
{
    Timer* timer = Timers + (id - 1);
    AJ_ASSERT(id > 0 && id <= AJ_MAX_TIMERS);
    memset(timer, 0, sizeof(Timer));
}


AJ_Status AJ_RunAllJoynService(AJ_BusAttachment* bus, AllJoynConfiguration* config)
{
    printf("++++++++ AJ_RunAllJoynService\n");
    
    uint8_t connected = FALSE;
    AJ_Status status = AJ_OK;
    AJ_InfoPrintf(("AJ_RunAllJoynService(bus=0x%p, config=0x%p)\n", bus, config));

    while (TRUE) {
        AJ_Time start = { 0, 0 };
        AJ_Message msg;
        uint32_t now;
        // get the next timeout
        uint32_t next;
        // wait forever
        uint32_t timeout = (uint32_t) -1;

        if (!connected) {
            status = AJ_StartService(
                bus,
                config->daemonName, config->connect_timeout, config->connected,
                config->session_port, config->service_name, config->flags, config->opts);
            if (status != AJ_OK) {
                AJ_ErrPrintf(("AJ_RunAllJoynService(): status=%s.\n", AJ_StatusText(status)));
                continue;
            }

            AJ_InfoPrintf(("AJ_RunAllJoynService(): connected to daemon: \"%s\"\n", AJ_GetUniqueName(bus)));

            connected = TRUE;

            /* Register a callback for providing bus authentication password */
            AJ_BusSetPasswordCallback(bus, config->password_callback);

            /* Register a callback for handling factory reset requests */
            AJ_BusSetFactoryResetCallback(bus, config->factory_reset_callback);

            /* Register a callback for handling policy change notifications */
            AJ_BusSetPolicyChangedCallback(bus, config->policy_changed_callback);

            /* Configure timeout for the link to the daemon bus */
            AJ_SetBusLinkTimeout(bus, config->link_timeout);

            if (config->connection_handler != NULL) {
                (config->connection_handler)(connected);
            }
        }

        // absolute time in milliseconds
        now = AJ_GetElapsedTime(&start, FALSE);
        next = RunExpiredTimers(now);

        if (next != (uint32_t) -1) {
            // if no timers running, wait forever
            timeout = next;
        }

        status = AJ_UnmarshalMsg(bus, &msg, min(500, timeout));
        if (AJ_ERR_TIMEOUT == status && AJ_ERR_LINK_TIMEOUT == AJ_BusLinkStateProc(bus)) {
            AJ_ErrPrintf(("AJ_RunAllJoynService(): AJ_ERR_READ\n"));
            status = AJ_ERR_READ;
        }

        if (status == AJ_ERR_TIMEOUT) {
            // go back around and handle the expired timers
            continue;
        }

        if (status == AJ_OK) {
            uint8_t handled = FALSE;
            const MessageHandlerEntry* message_entry = config->message_handlers;
            const PropHandlerEntry* prop_entry = config->prop_handlers;

            // check the user's handlers first.  ANY message that AllJoyn can handle is override-able.
            while (handled != TRUE && message_entry->msgid != 0) {
                if (message_entry->msgid == msg.msgId) {
                    if (msg.hdr->msgType == AJ_MSG_METHOD_CALL) {
                        // build a method reply
                        AJ_Message reply;
                        status = AJ_MarshalReplyMsg(&msg, &reply);

                        if (status == AJ_OK) {
                            status = (message_entry->handler)(&msg, &reply);
                        }

                        if (status == AJ_OK) {
                            status = AJ_DeliverMsg(&reply);
                        }
                    } else {
                        // call the handler!
                        status = (message_entry->handler)(&msg, NULL);
                    }

                    handled = TRUE;
                }

                ++message_entry;
            }

            // we need to check whether this is a property getter or setter.
            // these are stored in an array because multiple getters and setters can exist if running more than one bus object
            while (handled != TRUE && prop_entry->msgid != 0) {
                if (prop_entry->msgid == msg.msgId) {
                    // extract the method from the ID; GetProperty or SetProperty
                    uint32_t method = prop_entry->msgid & 0x000000FF;
                    if (method == AJ_PROP_GET) {
                        status = AJ_BusPropGet(&msg, prop_entry->callback, prop_entry->context);
                    } else if (method == AJ_PROP_SET) {
                        status = AJ_BusPropSet(&msg, prop_entry->callback, prop_entry->context);
                    } else {
                        // this should never happen!!!
                        AJ_ASSERT(!"Invalid property method");
                    }

                    handled = TRUE;
                }

                ++prop_entry;
            }

            // handler not found!
            if (handled == FALSE) {
                if (msg.msgId == AJ_METHOD_ACCEPT_SESSION) {
                    uint8_t accepted = (config->acceptor)(&msg);
                    status = AJ_BusReplyAcceptSession(&msg, accepted);
                } else {
                    AJ_InfoPrintf(("AJ_RunAllJoynService(): AJ_BusHandleBusMessage()\n"));
                    status = AJ_BusHandleBusMessage(&msg);
                }
            }

            // Any received packets indicates the link is active, so call to reinforce the bus link state
            AJ_NotifyLinkActive();
        }
        /*
         * Unarshaled messages must be closed to free resources
         */
        AJ_CloseMsg(&msg);

        if ((status == AJ_ERR_READ) || (status == AJ_ERR_WRITE) || (status == AJ_ERR_LINK_DEAD)) {
            AJ_InfoPrintf(("AJ_RunAllJoynService(): AJ_Disconnect(): daemon \"%s\"\n", AJ_GetUniqueName(bus)));
            AJ_Disconnect(bus);
            connected = FALSE;
            if (config->connection_handler != NULL) {
                (config->connection_handler)(connected);
            }
            /*
             * Sleep a little while before trying to reconnect
             */
            AJ_InfoPrintf(("AJ_RunAllJoynService(): AJ_Sleep()\n"));
            AJ_Sleep(10 * 1000);
        }
    }

    // this will never actually return!
    return AJ_OK;
}

AJ_Status AJ_StartService(AJ_BusAttachment* bus,
                          const char* daemonName,
                          uint32_t timeout,
                          uint8_t connected,
                          uint16_t port,
                          const char* name,
                          uint32_t flags,
                          const AJ_SessionOpts* opts
                          )
{
    AJ_Status status;
    AJ_Time timer;
    uint8_t serviceStarted = FALSE;
    uint32_t disposition;
    uint16_t retport;

    AJ_InfoPrintf(("AJ_StartService(bus=0x%p, daemonName=\"%s\", timeout=%d., connected=%d., port=%d., name=\"%s\", flags=0x%x, opts=0x%p)\n",
                   bus, daemonName, timeout, connected, port, name, flags, opts));

    AJ_InitTimer(&timer);

    while (TRUE) {
        if (AJ_GetElapsedTime(&timer, TRUE) > timeout) {
            return AJ_ERR_TIMEOUT;
        }
        
        printf("+++Main loop\n");
        
        if (!connected) {
            printf("+++Not connected\n");
            AJ_InfoPrintf(("AJ_StartService(): AJ_FindBusAndConnect()\n"));
            status = AJ_FindBusAndConnect(bus, daemonName, AJ_CONNECT_TIMEOUT);
            if (status != AJ_OK) {
                printf("+++Status not ok\n");
                AJ_WarnPrintf(("AJ_StartService(): connect failed: sleeping for %d seconds\n", AJ_CONNECT_PAUSE / 1000));
                AJ_Sleep(AJ_CONNECT_PAUSE);
                continue;
            }
            AJ_InfoPrintf(("AJ_StartService(): connected to bus\n"));
        }
        /*
         * Kick things off by binding a session port
         */
        AJ_InfoPrintf(("AJ_StartService(): AJ_BindSessionPort()\n"));
        status = AJ_BusBindSessionPort(bus, port, opts, 0);
        if (status == AJ_OK) {
            break;
        }
        printf("+++Disconnect\n");
        AJ_ErrPrintf(("AJ_StartService(): AJ_Disconnect(): status=%s.\n", AJ_StatusText(status)));
        AJ_Disconnect(bus);
    }

    printf("+++Looping before Not started loop\n");
    
    while (!serviceStarted && (status == AJ_OK)) {
        AJ_Message msg;

        printf("Looping in Not started loop\n");
        status = AJ_UnmarshalMsg(bus, &msg, AJ_UNMARSHAL_TIMEOUT);
        if (status == AJ_ERR_NO_MATCH) {
            // Ignore unknown messages
            status = AJ_OK;
            continue;
        }
        if (status != AJ_OK) {
            AJ_ErrPrintf(("AJ_StartService(): status=%s.\n", AJ_StatusText(status)));
            break;
        }

        switch (msg.msgId) {
        case AJ_REPLY_ID(AJ_METHOD_BIND_SESSION_PORT):
            if (msg.hdr->msgType == AJ_MSG_ERROR) {
                AJ_ErrPrintf(("AJ_StartService(): AJ_METHOD_BIND_SESSION_PORT: %s\n", msg.error));
                status = AJ_ERR_FAILURE;
                break;
            }
            status = AJ_UnmarshalArgs(&msg, "uq", &disposition, &retport);
            if (AJ_OK != status) {
                break;
            }
            if (retport == port) {
                if (AJ_BINDSESSIONPORT_REPLY_SUCCESS == disposition) {
                    AJ_InfoPrintf(("AJ_StartService(): AJ_BusRequestName()\n"));
                    status = AJ_BusRequestName(bus, name, flags);
                } else {
                    status = AJ_ERR_FAILURE;
                    AJ_InfoPrintf(("AJ_StartService(bus=%p): AJ_METHOD_BIND_SESSION_PORT: disposition %d\n", bus, disposition));
                    break;
                }
            }
            break;

        case AJ_REPLY_ID(AJ_METHOD_REQUEST_NAME):
            if (msg.hdr->msgType == AJ_MSG_ERROR) {
                AJ_ErrPrintf(("AJ_StartService(): AJ_METHOD_REQUEST_NAME: %s\n", msg.error));
                status = AJ_ERR_FAILURE;
            } else {
                AJ_InfoPrintf(("AJ_StartService(): AJ_BusAdvertiseName()\n"));
                status = AJ_BusAdvertiseName(bus, name, (opts != NULL) ? opts->transports : AJ_TRANSPORT_ANY, AJ_BUS_START_ADVERTISING, 0);
            }
            break;

        case AJ_REPLY_ID(AJ_METHOD_ADVERTISE_NAME):
            if (msg.hdr->msgType == AJ_MSG_ERROR) {
                AJ_ErrPrintf(("AJ_StartService(): AJ_METHOD_ADVERTISE_NAME: %s\n", msg.error));
                status = AJ_ERR_FAILURE;
            } else {
                serviceStarted = TRUE;
            }
            break;

        default:
            /*
             * Pass to the built-in bus message handlers
             */
            AJ_InfoPrintf(("AJ_StartService(): AJ_BusHandleBusMessage()\n"));
            status = AJ_BusHandleBusMessage(&msg);
            break;
        }
        AJ_CloseMsg(&msg);
    }

    if (AJ_OK != status) {
        AJ_WarnPrintf(("AJ_StartService(): AJ_Disconnect(): status=%s\n", AJ_StatusText(status)));
        AJ_Disconnect(bus);
        return status;
    }
    status = AJ_AboutInit(bus, port);
    if (AJ_OK != status) {
        AJ_WarnPrintf(("AJ_StartService(): AJ_AboutInit returned status=%s\n", AJ_StatusText(status)));
        AJ_Disconnect(bus);
        return status;
    }

    return status;
}

static
AJ_Status StartClient(AJ_BusAttachment* bus,
                      const char* daemonName,
                      uint32_t timeout,
                      uint8_t connected,
                      const char* name,
                      uint16_t port,
                      const char** interfaces,
                      uint32_t* sessionId,
                      char* serviceName,
                      const AJ_SessionOpts* opts,
                      char* fullName)
{
    AJ_Status status = AJ_OK;
    AJ_Time timer;
    uint8_t found = FALSE;
    uint8_t clientStarted = FALSE;
    uint32_t elapsed = 0;
    char* rule;
    size_t ruleLen;
    const char* base = "interface='org.alljoyn.About',sessionless='t'";
    const char* impl = ",implements='";
    const char** ifaces;

    AJ_InfoPrintf(("AJ_StartClient(bus=0x%p, daemonName=\"%s\", timeout=%d., connected=%d., interface=\"%p\", sessionId=0x%p, serviceName=0x%p, opts=0x%p)\n",
                   bus, daemonName, timeout, connected, interfaces, sessionId, serviceName, opts));

    AJ_InitTimer(&timer);

    if ((name != NULL) && (interfaces != NULL)) {
        return AJ_ERR_INVALID;
    }

    while (elapsed < timeout) {
        if (!connected) {
            printf("-------StartClient - not connected\n");
            status = AJ_FindBusAndConnect(bus, daemonName, AJ_CONNECT_TIMEOUT);
            elapsed = AJ_GetElapsedTime(&timer, TRUE);
            if (status != AJ_OK) {
                printf("-------StartClient - connect nok\n");
                elapsed += AJ_CONNECT_PAUSE;
                if (elapsed > timeout) {
                    break;
                }
                AJ_WarnPrintf(("AJ_StartClient(): Failed to connect to bus, sleeping for %d seconds\n", AJ_CONNECT_PAUSE / 1000));
                AJ_Sleep(AJ_CONNECT_PAUSE);
                continue;
            }
            printf("-------StartClient - connect - AJ_OK\n");
            AJ_InfoPrintf(("AJ_StartClient(): AllJoyn client connected to bus\n"));
        }
        
        printf("-------StartClient - cont - 1\n");
        
        if (name != NULL) {
            /*
             * Kick things off by finding the service names
             */
            printf("-------StartClient - bus ads\n");
            status = AJ_BusFindAdvertisedName(bus, name, AJ_BUS_START_FINDING);
            AJ_InfoPrintf(("AJ_StartClient(): AJ_BusFindAdvertisedName()\n"));
        } else {
            /*
             * Kick things off by registering for the Announce signal.
             * Optionally add the implements clause per given interface
             */
            ruleLen = strlen(base) + 1;
            if (interfaces != NULL) {
                ifaces = interfaces;
                while (*ifaces != NULL) {
                    ruleLen += strlen(impl) + strlen(*ifaces) + 1;
                    ifaces++;
                }
            }
            rule = (char*) AJ_Malloc(ruleLen);
            if (rule == NULL) {
                status = AJ_ERR_RESOURCES;
                break;
            }
            strcpy(rule, base);
            if (interfaces != NULL) {
                ifaces = interfaces;
                while (*ifaces != NULL) {
                    strcat(rule, impl);
                    if ((*ifaces)[0] == '$') {
                        strcat(rule, &(*ifaces)[1]);
                    } else {
                        strcat(rule, *ifaces);
                    }
                    strcat(rule, "'");
                    ifaces++;
                }
            }
            status = AJ_BusSetSignalRule(bus, rule, AJ_BUS_SIGNAL_ALLOW);
            AJ_InfoPrintf(("AJ_StartClient(): Client SetSignalRule: %s\n", rule));
            AJ_Free(rule);
            printf("-------StartClient - bus signal\n");
        }
        if (status == AJ_OK) {
            break;
        }
        if (!connected) {
            AJ_WarnPrintf(("AJ_StartClient(): Client disconnecting from bus: status=%s.\n", AJ_StatusText(status)));
            AJ_Disconnect(bus);
        }
    }
    
    printf("-------StartClient - cont - 2\n");
    if (elapsed > timeout) {
        AJ_WarnPrintf(("AJ_StartClient(): Client timed-out trying to connect to bus: status=%s.\n", AJ_StatusText(status)));
        return AJ_ERR_TIMEOUT;
    }
    timeout -= elapsed;

    if (status != AJ_OK) {
        return status;
    }

    *sessionId = 0;
    if (serviceName != NULL) {
        *serviceName = '\0';
    }

    while (!clientStarted && (status == AJ_OK)) {
        AJ_Message msg;
        const uint32_t timeout2 = min(AJ_UNMARSHAL_TIMEOUT, timeout);
        printf("-------StartClient - client not started\n");
        status = AJ_UnmarshalMsg(bus, &msg, timeout2);
        if ((status == AJ_ERR_TIMEOUT) && !found) {
            /*
             * Timeouts are expected until we find a name or service
             */
            if (timeout <= timeout2) {
                return status;
            }
            timeout -= timeout2;
            status = AJ_OK;
            continue;
        }
        if (status == AJ_ERR_NO_MATCH) {
            // Ignore unknown messages
            status = AJ_OK;
            continue;
        }
        if (status != AJ_OK) {
            AJ_ErrPrintf(("AJ_StartClient(): status=%s\n", AJ_StatusText(status)));
            break;
        }
        switch (msg.msgId) {

        case AJ_REPLY_ID(AJ_METHOD_FIND_NAME):
        case AJ_REPLY_ID(AJ_METHOD_FIND_NAME_BY_TRANSPORT):
        {
            if (msg.hdr->msgType == AJ_MSG_ERROR) {
                AJ_ErrPrintf(("AJ_StartClient(): AJ_METHOD_FIND_NAME: %s\n", msg.error));
                status = AJ_ERR_FAILURE;
            } else {
                uint32_t disposition;
                AJ_UnmarshalArgs(&msg, "u", &disposition);
                if ((disposition != AJ_FIND_NAME_STARTED) && (disposition != AJ_FIND_NAME_ALREADY)) {
                    AJ_ErrPrintf(("AJ_StartClient(): AJ_ERR_FAILURE\n"));
                    status = AJ_ERR_FAILURE;
                }
            }
            printf("-------StartClient - AJ_REPLY_ID\n");
        }
            break;

        case AJ_SIGNAL_FOUND_ADV_NAME:
            {
                AJ_Arg arg;
                AJ_UnmarshalArg(&msg, &arg);
                AJ_InfoPrintf(("FoundAdvertisedName(%s)\n", arg.val.v_string));
                if (!found) {
                    if (fullName) {
                        strncpy(fullName, arg.val.v_string, arg.len);
                        fullName[arg.len] = '\0';
                    }
                    found = TRUE;
                    status = AJ_BusJoinSession(bus, arg.val.v_string, port, opts);
                }
                printf("-------StartClient - J_SIGNAL_FOUND_ADV_NAME\n");
            }
            break;

        case AJ_SIGNAL_ABOUT_ANNOUNCE:
            {
                printf("-------StartClient - AJ_SIGNAL_ABOUT_ANNOUNCE\n");
                uint16_t aboutVersion, aboutPort;
#ifdef ANNOUNCE_BASED_DISCOVERY
                status = AJ_AboutHandleAnnounce(&msg, &aboutVersion, &aboutPort, serviceName, &found);
                if (interfaces != NULL) {
                    found = TRUE;
                }
                if ((status == AJ_OK) && (found == TRUE)) {
                    AJ_InfoPrintf(("AJ_StartClient(): AboutAnnounce from (%s) About Version: %d Port: %d\n", msg.sender, aboutVersion, aboutPort));
#else
                AJ_InfoPrintf(("AJ_StartClient(): AboutAnnounce from (%s)\n", msg.sender));
                if (!found) {
                    found = TRUE;
                    AJ_UnmarshalArgs(&msg, "qq", &aboutVersion, &aboutPort);
                    if (serviceName != NULL) {
                        strncpy(serviceName, msg.sender, AJ_MAX_NAME_SIZE);
                        serviceName[AJ_MAX_NAME_SIZE] = '\0';
                    }
#endif
                    /*
                     * Establish a session with the provided port.
                     * If port value is 0 use the About port unmarshalled from the Announcement instead.
                     */
                    if (port == 0) {
                        status = AJ_BusJoinSession(bus, msg.sender, aboutPort, opts);
                    } else {
                        status = AJ_BusJoinSession(bus, msg.sender, port, opts);
                    }
                    if (status != AJ_OK) {
                        AJ_ErrPrintf(("AJ_StartClient(): BusJoinSession failed (%s)\n", AJ_StatusText(status)));
                    }
                }
            }
            break;

        case AJ_REPLY_ID(AJ_METHOD_JOIN_SESSION):
            {
                printf("-------StartClient - AJ_REPLY_ID(AJ_METHOD_JOIN_SESSION)\n");
                uint32_t replyCode;

                if (msg.hdr->msgType == AJ_MSG_ERROR) {
                    AJ_ErrPrintf(("AJ_StartClient(): AJ_METHOD_JOIN_SESSION: %s\n", msg.error));
                    status = AJ_ERR_FAILURE;
                } else {
                    status = AJ_UnmarshalArgs(&msg, "uu", &replyCode, sessionId);
                    if (replyCode == AJ_JOINSESSION_REPLY_SUCCESS) {
                        clientStarted = TRUE;
                    } else {
                        AJ_ErrPrintf(("AJ_StartClient(): AJ_METHOD_JOIN_SESSION reply (%d)\n", replyCode));
                        status = AJ_ERR_FAILURE;
                    }
                }
            }
            break;

        case AJ_SIGNAL_SESSION_LOST_WITH_REASON:
            /*
             * Force a disconnect
             */
            {
                printf("-------StartClient - AJ_SIGNAL_SESSION_LOST_WITH_REASON\n");
                uint32_t id, reason;
                AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                AJ_InfoPrintf(("Session lost. ID = %u, reason = %u", id, reason));
            }
            AJ_ErrPrintf(("AJ_StartClient(): AJ_SIGNAL_SESSION_LOST_WITH_REASON: AJ_ERR_READ\n"));
            status = AJ_ERR_READ;
            break;

        default:
            /*
             * Pass to the built-in bus message handlers
             */
            printf("-------StartClient - default\n");
            AJ_InfoPrintf(("AJ_StartClient(): AJ_BusHandleBusMessage()\n"));
            status = AJ_BusHandleBusMessage(&msg);
            break;
        }
        AJ_CloseMsg(&msg);
    }

    if ((AJ_OK != status) && !connected) {
        AJ_WarnPrintf(("AJ_StartClient(): Client disconnecting from bus: status=%s\n", AJ_StatusText(status)));
        AJ_Disconnect(bus);
        printf("-------StartClient - disconnect\n");
        return status;
    }

    return status;
}

AJ_Status AJ_StartClientByName(AJ_BusAttachment* bus,
                               const char* daemonName,
                               uint32_t timeout,
                               uint8_t connected,
                               const char* name,
                               uint16_t port,
                               uint32_t* sessionId,
                               const AJ_SessionOpts* opts,
                               char* fullName)
{
    return StartClient(bus, daemonName, timeout, connected, name, port, NULL, sessionId, NULL, opts, fullName);
}

AJ_Status AJ_StartClient(AJ_BusAttachment* bus,
                         const char* daemonName,
                         uint32_t timeout,
                         uint8_t connected,
                         const char* name,
                         uint16_t port,
                         uint32_t* sessionId,
                         const AJ_SessionOpts* opts)
{
    AJ_WarnPrintf(("AJ_StartClient(): This function is deprecated. Please use AJ_StartClientByName() instead\n"));
    return StartClient(bus, daemonName, timeout, connected, name, port, NULL, sessionId, NULL, opts, NULL);
}

AJ_Status AJ_StartClientByInterface(AJ_BusAttachment* bus,
                                    const char* daemonName,
                                    uint32_t timeout,
                                    uint8_t connected,
                                    const char** interfaces,
                                    uint32_t* sessionId,
                                    char* uniqueName,
                                    const AJ_SessionOpts* opts)
{
    return StartClient(bus, daemonName, timeout, connected, NULL, 0, interfaces, sessionId, uniqueName, opts, NULL);
}

#ifdef ANNOUNCE_BASED_DISCOVERY
AJ_Status AJ_StartClientByPeerDescription(AJ_BusAttachment* bus,
                                          const char* daemonName,
                                          uint32_t timeout,
                                          uint8_t connected,
                                          const AJ_AboutPeerDescription* peerDesc,
                                          uint16_t port,
                                          uint32_t* sessionId,
                                          char* uniqueName,
                                          const AJ_SessionOpts* opts)
{
    if (peerDesc != NULL) {
        AJ_AboutRegisterAnnounceHandlers(peerDesc, 1);
    }
    return StartClient(bus, daemonName, timeout, connected, NULL, port, NULL, sessionId, uniqueName, opts, NULL);
}
#endif
