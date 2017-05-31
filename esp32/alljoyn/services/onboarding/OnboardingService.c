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
 * prior to first inclusion of aj_debug.h.
 * The corresponding flag dbgAJOBS is defined in OnboardinService.h and implemented below.
 */
#define AJ_MODULE AJOBS
#include <aj_debug.h>

#include <OnboardingService.h>
#include <OnboardingControllerAPI.h>
#include <aj_nvram.h>
#include <aj_link_timeout.h>
#include <aj_wifi_ctrl.h>

/**
 * Turn on per-module debug printing by setting this variable to non-zero value
 * (usually in debugger).
 */
#ifndef NDEBUG
#ifndef ER_DEBUG_AJSVCALL
#define ER_DEBUG_AJSVCALL 0
#endif
#ifndef ER_DEBUG_AJOBS
#define ER_DEBUG_AJOBS 0
#endif
AJ_EXPORT uint8_t dbgAJOBS = ER_DEBUG_AJOBS || ER_DEBUG_AJSVCALL;
#endif

/**
 * Published Onboarding BusObjects and Interfaces.
 */

static const char* const OnboardingInterface[] = {
    "$org.alljoyn.Onboarding",
    "@Version>q",
    "@State>n",
    "@LastError>(ns)",
    "?ConfigureWiFi SSID<s passphrase<s authType<n status>n",
    "?Connect",
    "?Offboard",
    "?GetScanInfo age>q scanList>a(sn)",
    "!ConnectionResult >(ns)",
    NULL
};

static const uint16_t AJSVC_OnboardingVersion = 1;

static const AJ_InterfaceDescription AJSVC_OnboardingInterfaces[] = {
    AJ_PropertiesIface,
    OnboardingInterface,
    NULL
};

AJ_Object AJOBS_ObjectList[] = {
    { "/Onboarding",           AJSVC_OnboardingInterfaces, AJ_OBJ_FLAG_ANNOUNCED },
    { NULL }
};

/*
 * Message identifiers for the method calls this service implements
 */

#define AJ_SVC_MESSAGE_ID(p, i, m)                              AJ_ENCODE_MESSAGE_ID(AJOBS_OBJECT_LIST_INDEX, p, i, m)
#define AJ_SVC_PROPERTY_ID(p, i, m)                             AJ_ENCODE_PROPERTY_ID(AJOBS_OBJECT_LIST_INDEX, p, i, m)

#define OBS_OBJECT_INDEX       0                                                    /**< number of pre onboarding objects */

#define OBS_GET_PROP           AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 0, AJ_PROP_GET)  /**< property get */
#define OBS_SET_PROP           AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 0, AJ_PROP_SET)  /**< property set */

#define OBS_VERSION_PROP       AJ_SVC_PROPERTY_ID(OBS_OBJECT_INDEX, 1, 0)           /**< version property index */
#define OBS_STATE_PROP         AJ_SVC_PROPERTY_ID(OBS_OBJECT_INDEX, 1, 1)           /**< state property */
#define OBS_LASTERROR_PROP     AJ_SVC_PROPERTY_ID(OBS_OBJECT_INDEX, 1, 2)           /**< last error property */
#define OBS_CONFIGURE_WIFI     AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 1, 3)            /**< configure wifi */
#define OBS_CONNECT            AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 1, 4)            /**< connect */
#define OBS_OFFBOARD           AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 1, 5)            /**< offboard */
#define OBS_GET_SCAN_INFO      AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 1, 6)            /**< get scan info */
#define OBS_CONNECTION_RESULT  AJ_SVC_MESSAGE_ID(OBS_OBJECT_INDEX, 1, 7)            /**< connection result */

AJ_Status AJOBS_RegisterObjectList()
{
    AJOBS_ObjectList[OBS_OBJECT_INDEX].flags &= ~(AJ_OBJ_FLAG_HIDDEN | AJ_OBJ_FLAG_DISABLED);
    AJOBS_ObjectList[OBS_OBJECT_INDEX].flags |= AJ_OBJ_FLAG_ANNOUNCED;

    return AJ_RegisterObjectList(AJOBS_ObjectList, AJOBS_OBJECT_LIST_INDEX);
}

/*
 * Modify these variables to change the service's behavior
 */
/*
   //will be used in future versions
   #define CONNECTION_RESULT_TTL 0
   static uint16_t obsSessionId = 0;
 */

/*
 * Handles a property GET request so marshals the property value to return
 */
AJ_Status AJOBS_PropGetHandler(AJ_Message* replyMsg, uint32_t propId, void* context)
{
    AJ_Status status = AJ_OK;
    const AJOBS_Error* obError = AJOBS_GetError();

    if (propId == OBS_VERSION_PROP) {
        status = AJ_MarshalArgs(replyMsg, "q", AJSVC_OnboardingVersion);
    } else if (propId == OBS_STATE_PROP) {
        status = AJ_MarshalArgs(replyMsg, "n", AJOBS_GetState());
    } else if (propId == OBS_LASTERROR_PROP) {
        status = AJ_MarshalArgs(replyMsg, "(ns)", obError->code, obError->message);
    } else {
        status = AJ_ERR_UNEXPECTED;
    }

    return status;
}

/*
 * Handles a property SET request and returns an unexpected error as all properties are declared as readonly
 */
AJ_Status AJOBS_PropSetHandler(AJ_Message* replyMsg, uint32_t propId, void* context)
{
    return AJ_ERR_UNEXPECTED;
}

AJ_Status AJOBS_ConfigureWiFiHandler(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    const char* ssid;
    const char* pc;
    int16_t authType;
    int16_t retVal;

    // Set provided network configuration
    AJ_InfoPrintf(("Handling ConfigureWiFi request\n"));

    status = AJ_UnmarshalArgs(msg, "ssn", &ssid, &pc, &authType);
    if (status != AJ_OK) {
        goto ErrorExit;
    }

    AJ_InfoPrintf(("Got new info for %s with passcode=%s and auth=%d\n", ssid, pc, authType));
    if (!AJOBS_IsInfoValid(ssid, pc, authType)) {
        AJ_InfoPrintf(("New info is invalid!\n"));
        status = AJ_MarshalErrorMsg(msg, &reply, AJSVC_ERROR_INVALID_VALUE);
        if (status != AJ_OK) {
            goto ErrorExit;
        }
        status = AJ_DeliverMsg(&reply);
        goto ErrorExit;
    }

    retVal = 1;
    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    status = AJ_MarshalArgs(&reply, "n", retVal);
    if (status != AJ_OK) {
        goto ErrorExit;
    }
    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        goto ErrorExit;
    }

    AJOBS_UpdateInfo(ssid, pc, authType);

ErrorExit:
    return status;
}

AJ_Status AJOBS_ConnectWiFiHandler(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;

    AJ_InfoPrintf(("Handling ConnectWiFi request\n"));
    if (!AJOBS_ControllerAPI_IsConfigured()) {
        AJ_WarnPrintf(("No current configuration to connect with\n"));
        return status;
    }

    return AJ_ERR_RESTART;     // Force disconnect of AJ and services and reconnection of Wi-Fi on restart of message loop
}

AJ_Status AJOBS_OffboardWiFiHandler(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;

    AJ_InfoPrintf(("Handling Offboard request\n"));
    status = AJOBS_ControllerAPI_DoOffboardWiFi();
    if (status != AJ_OK) {
        return status;
    }
    status = AJ_ERR_RESTART;     // Force disconnect of AJ and services and reconnection of Wi-Fi on restart on restart of message loop

    return status;
}

AJ_Status AJOBS_GetScanInfoHandler(AJ_Message* msg)
{
    AJ_Status status = AJ_OK;
    AJ_Message reply;
    AJ_Arg array;
    uint32_t elapsed;
    int i = 0;

    AJ_InfoPrintf(("Handling GetScanInfo request\n"));

    status = AJ_MarshalReplyMsg(msg, &reply);
    if (status != AJ_OK) {
        return status;
    }

    elapsed = AJ_GetElapsedTime((AJ_Time *)AJOBS_GetLastScanTime(), TRUE);
    if (elapsed > 0) {
        elapsed /= 60000;
    }
    status = AJ_MarshalArgs(&reply, "q", (uint16_t) elapsed);
    if (status != AJ_OK) {
        return status;
    }

    status = AJ_MarshalContainer(&reply, &array, AJ_ARG_ARRAY);
    if (status != AJ_OK) {
        return status;
    }
    for (; i < AJOBS_GetScanInfoCount(); ++i) {
        status = AJ_MarshalArgs(&reply, "(sn)", (AJOBS_GetScanInfos())[i].ssid, (AJOBS_GetScanInfos())[i].authType);
        if (status != AJ_OK) {
            return status;
        }
    }
    status = AJ_MarshalCloseContainer(&reply, &array);
    if (status != AJ_OK) {
        return status;
    }

    status = AJ_DeliverMsg(&reply);
    if (status != AJ_OK) {
        return status;
    }

    return status;
}

/*
   //will be used in future versions
   AJ_Status AJOBS_SendConnectionResult(AJ_BusAttachment* bus)
   {
    AJ_Status status = AJ_OK;
    const AJOBS_Error* obError = AJOBS_GetError();
    AJ_Message out;

    AJ_InfoPrintf(("Sending ConnectionResult signal\n"));
    status = AJ_MarshalSignal(bus, &out, OBS_CONNECTION_RESULT, NULL, obsSessionId, AJ_FLAG_GLOBAL_BROADCAST, AJOBS_CONNECTION_RESULT_TTL);
    if (status != AJ_OK) {
        return status;
    }
    status = AJ_MarshalArgs(&out, "(ns)", obError->code, obError->message);
    if (status != AJ_OK) {
        return status;
    }
    AJ_DeliverMsg(&out);
    AJ_CloseMsg(&out);

    return status;
   }
 */

AJ_Status AJOBS_ConnectedHandler(AJ_BusAttachment* bus)
{
    AJ_Status status = AJ_OK;
    return status;
}

AJSVC_ServiceStatus AJOBS_MessageProcessor(AJ_BusAttachment* busAttachment, AJ_Message* msg, AJ_Status* msgStatus)
{
    AJSVC_ServiceStatus serviceStatus = AJSVC_SERVICE_STATUS_HANDLED;

    if (*msgStatus == AJ_OK) {
        switch (msg->msgId) {

        case OBS_GET_PROP:
            *msgStatus = AJ_BusPropGet(msg, AJOBS_PropGetHandler, NULL);
            break;

        case OBS_SET_PROP:
            *msgStatus = AJ_BusPropSet(msg, AJOBS_PropSetHandler, NULL);
            break;

        case OBS_CONFIGURE_WIFI:
            *msgStatus = AJOBS_ConfigureWiFiHandler(msg);
            break;

        case OBS_CONNECT:
            *msgStatus = AJOBS_ConnectWiFiHandler(msg);
            break;

        case OBS_OFFBOARD:
            *msgStatus = AJOBS_OffboardWiFiHandler(msg);
            break;

        case OBS_GET_SCAN_INFO:
            *msgStatus = AJOBS_GetScanInfoHandler(msg);
            break;

        default:
            serviceStatus = AJSVC_SERVICE_STATUS_NOT_HANDLED;
            break;
        }
    } else {
        serviceStatus = AJSVC_SERVICE_STATUS_NOT_HANDLED;
    }

    return serviceStatus;
}

AJ_Status AJOBS_DisconnectHandler(AJ_BusAttachment* bus)
{
    AJ_Status status = AJ_OK;
    return status;
}
