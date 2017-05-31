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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mpconfigport.h"
#include <assert.h>

/**
 * Per-module definition of the current module for debug logging.  Must be defined
 * prior to first inclusion of aj_debug.h.
 * The corresponding flag dbgAJSVCAPP is defined in the containing sample app.
 */
#define AJ_MODULE AJSVCAPP
#include <aj_debug.h>

#include <alljoyn.h>
#include <aj_creds.h>
#include <aj_config.h>
#include <aj_link_timeout.h>
#include <aj_security.h>
#include <aj_authorisation.h>
#include <aj_bus.h>
//#include <ConfigService.h>
#include <OnboardingService.h>
#include <OnboardingManager.h>
#include <PropertyStore.h>
#include <ServicesCommon.h>
#include <ServicesHandlers.h>
#include "PropertyStoreOEMProvisioning.h"

/*
 * Logger definition
 */

#ifndef NDEBUG
#ifndef ER_DEBUG_AJSVCAPP
#define ER_DEBUG_AJSVCAPP 0
#endif
AJ_EXPORT uint8_t dbgAJSVCAPP = ER_DEBUG_AJSVCAPP;
#endif

/*
 * Define timeout/pause values. Values are in milli seconds.
 * The following may be tuned according to platform requirements such as battery usage.
 */

#define AJAPP_BUS_LINK_TIMEOUT    60
#define AJAPP_CONNECT_TIMEOUT     (1000 * 60 * 10) // Override AJ_CONNECT_TIMEOUT to wait longer for a successful connection to a Routing Node
#define AJAPP_CONNECT_PAUSE       (1000 * 2)       // Override AJ_CONNECT_PAUSE to be more responsive
#define AJAPP_SLEEP_TIME          (1000 * 2)       // A little pause to let things settle
#define AJAPP_UNMARSHAL_TIMEOUT   (1000 * 1)       // Override AJ_UNMARSHAL_TIMEOUT to be more responsive

/**
 * Application wide globals
 */

#define ROUTING_NODE_NAME "org.alljoyn.BusNode"
static uint8_t isBusConnected = FALSE;
static AJ_BusAttachment busAttachment;
#define AJ_ABOUT_SERVICE_PORT 900
#define AJ_PERMISSION_MGMT_PORT 101

/**
 * Application handlers
 */

typedef enum {
    INIT_START = 0,
    INIT_SERVICES = INIT_START,
    INIT_SERVICES_PORT,
    INIT_ABOUT,
    INIT_CHECK_ANNOUNCE,
    INIT_FINISHED = INIT_CHECK_ANNOUNCE
} enum_init_state_t;

static enum_init_state_t currentServicesInitializationState = INIT_START;
static enum_init_state_t nextServicesInitializationState = INIT_START;

static AJ_Status AJApp_ConnectedHandler(AJ_BusAttachment* busAttachment)
{
    AJ_Status status = AJ_OK;
    if (AJ_GetUniqueName(busAttachment)) {
        if (currentServicesInitializationState == nextServicesInitializationState) {
            switch (currentServicesInitializationState) {
            case INIT_SERVICES:
                status = AJSVC_ConnectedHandler(busAttachment);
                if (status != AJ_OK) {
                    goto ErrorExit;
                }
                currentServicesInitializationState = nextServicesInitializationState = INIT_SERVICES_PORT;
                break;

            case INIT_SERVICES_PORT:
                status = AJ_BusBindSessionPort(busAttachment, AJ_ABOUT_SERVICE_PORT, NULL, 0);
                if (status != AJ_OK) {
                    goto ErrorExit;
                }
                nextServicesInitializationState = INIT_ABOUT;
                break;

            case INIT_ABOUT:
                status = AJ_AboutInit(busAttachment, AJ_ABOUT_SERVICE_PORT);
                if (status != AJ_OK) {
                    goto ErrorExit;
                }
                currentServicesInitializationState = nextServicesInitializationState = INIT_CHECK_ANNOUNCE;
                break;

            case INIT_CHECK_ANNOUNCE:
                status = AJ_AboutAnnounce(busAttachment);
                if (status != AJ_OK) {
                    goto ErrorExit;
                }
                break;

            default:
                break;
            }
        }
    }
    return status;

ErrorExit:

    AJ_ErrPrintf(("Application ConnectedHandler returned an error %s\n", (AJ_StatusText(status))));
    return status;
}

static AJSVC_ServiceStatus AJApp_MessageProcessor(AJ_BusAttachment* busAttachment, AJ_Message* msg, AJ_Status* status)
{
    AJSVC_ServiceStatus serviceStatus = AJSVC_SERVICE_STATUS_HANDLED;
    uint16_t port;
    char* joiner;
    uint32_t sessionId = 0;
    uint8_t session_accepted = FALSE;

    if (msg->msgId == AJ_METHOD_ACCEPT_SESSION) {    // Process all incoming request to join a session and pass request for acceptance by all services
        *status = AJ_UnmarshalArgs(msg, "qus", &port, &sessionId, &joiner);
        if (*status != AJ_OK) {
            return serviceStatus;
        }
        session_accepted |= (port == AJ_ABOUT_SERVICE_PORT);
        session_accepted |= (port == AJ_PERMISSION_MGMT_PORT);
        session_accepted |= AJSVC_CheckSessionAccepted(port, sessionId, joiner);
        *status = AJ_BusReplyAcceptSession(msg, session_accepted);
        AJ_AlwaysPrintf(("%s session session_id=%u joiner=%s for port %u\n", (session_accepted ? "Accepted" : "Rejected"), sessionId, joiner, port));
    } else {
        switch (currentServicesInitializationState) {
        case INIT_SERVICES_PORT:
            if (msg->msgId == AJ_REPLY_ID(AJ_METHOD_BIND_SESSION_PORT)) {
                currentServicesInitializationState = nextServicesInitializationState;
            }
            break;

        default:
            serviceStatus = AJSVC_MessageProcessorAndDispatcher(busAttachment, msg, status);
            break;
        }
    }

    return serviceStatus;
}

static AJ_Status AJApp_DisconnectHandler(AJ_BusAttachment* busAttachment, uint8_t restart)
{
    AJ_Status status = AJ_OK;

    if (restart) {
        AJ_BusUnbindSession(busAttachment, AJ_ABOUT_SERVICE_PORT);
    }

    AJ_AboutSetShouldAnnounce();
    currentServicesInitializationState = nextServicesInitializationState = INIT_START;

    status = AJSVC_DisconnectHandler(busAttachment);
    return status;
}

const char* deviceManufactureName = "PYCOM";
const char* deviceProductName = "WIPY";

/**
 * Config supported PropertyStore provisioning
 */
static const char DEFAULT_LANGUAGE[] = "en";
static const char* DEFAULT_LANGUAGES[] = { DEFAULT_LANGUAGE };
static const char* SUPPORTED_LANGUAGES[] = { DEFAULT_LANGUAGE, NULL };
const char* const* propertyStoreDefaultLanguages = SUPPORTED_LANGUAGES;

/**
 * property array of structure with defaults
 */
static const char* DEFAULT_DEVICE_NAMES[] = { "" }; // Leave empty to be generated at run-time
static const char* DEFAULT_APP_NAMES[] = { "Onboardee" };
static const char* DEFAULT_DESCRIPTIONS[] = { "My First IOE device" };
static const char* DEFAULT_MANUFACTURERS[] = { "Company A" };
static const char* DEFAULT_DEVICE_MODELS[] = { "0.0.1" };
static const char* DEFAULT_DATE_OF_MANUFACTURES[] = { "2014-05-01" };
static const char* DEFAULT_SOFTWARE_VERSIONS[] = { "0.0.1" };
static const char* DEFAULT_HARDWARE_VERSIONS[] = { "0.0.1" };
static const char* DEFAULT_SUPPORT_URLS[] = { "http://www.company_a.com" };

const char** propertyStoreDefaultValues[AJSVC_PROPERTY_STORE_NUMBER_OF_KEYS] =
{
// "Default Values per language",                    "Key Name"
    NULL,                                           /*DeviceId*/
    NULL,                                           /*AppId*/
    DEFAULT_DEVICE_NAMES,                           /*DeviceName*/
    DEFAULT_LANGUAGES,                              /*DefaultLanguage*/
    NULL,                                           /*Passcode*/
    NULL,                                           /*RealmName*/
// Add other runtime keys above this line
    DEFAULT_APP_NAMES,                              /*AppName*/
    DEFAULT_DESCRIPTIONS,                           /*Description*/
    DEFAULT_MANUFACTURERS,                          /*Manufacturer*/
    DEFAULT_DEVICE_MODELS,                          /*ModelNumber*/
    DEFAULT_DATE_OF_MANUFACTURES,                   /*DateOfManufacture*/
    DEFAULT_SOFTWARE_VERSIONS,                      /*SoftwareVersion*/
    NULL,                                           /*AJSoftwareVersion*/
 //   NULL,                                           /*MaxLength*/
// Add other mandatory about keys above this line
    //DEFAULT_HARDWARE_VERSIONS,                      /*HardwareVersion*/
    //DEFAULT_SUPPORT_URLS,                           /*SupportUrl*/
// Add other optional about keys above this line
};

/**
 * properties array of runtime values' buffers
 */
static char machineIdVar[MACHINE_ID_LENGTH + 1] = { 0 };
static char* machineIdVars[] = { machineIdVar };
static char deviceNameVar[DEVICE_NAME_VALUE_LENGTH + 1] = { 0 };
static char* deviceNameVars[] = { deviceNameVar };
static char defaultLanguageVar[LANG_VALUE_LENGTH + 1] = { 0 };
static char* defaultLanguageVars[] = { defaultLanguageVar };
static char passcodeVar[PASSWORD_VALUE_LENGTH + 1] = { 0 };
static char* passcodeVars[] = { passcodeVar };
static char realmNameVar[KEY_VALUE_LENGTH + 1] = { 0 };
static char* realmNameVars[] = { realmNameVar };

PropertyStoreConfigEntry propertyStoreRuntimeValues[AJSVC_PROPERTY_STORE_NUMBER_OF_RUNTIME_KEYS] =
{
//  {"Buffers for Values per language", "Buffer Size"},                  "Key Name"
    { machineIdVars,             MACHINE_ID_LENGTH + 1 },               /*DeviceId*/
    { machineIdVars,             MACHINE_ID_LENGTH + 1 },               /*AppId*/
    { deviceNameVars,            DEVICE_NAME_VALUE_LENGTH + 1 },        /*DeviceName*/
// Add other persisted keys above this line
    //{ defaultLanguageVars,       LANG_VALUE_LENGTH + 1 },               /*AppName*/
    //{ passcodeVars,              PASSWORD_VALUE_LENGTH + 1 },           /*Description*/
   // { realmNameVars,             KEY_VALUE_LENGTH + 1 },                /*Manufacturer*/
};

/**
 * Security
 */

// Copied from ajtcl/samples/secure/SecureServiceECDHE.c
static const char pem_prv[] = {
    "-----BEGIN EC PRIVATE KEY-----"
    "MDECAQEEICCRJMbxSiWUqj4Zs7jFQRXDJdBRPWX6fIVqE1BaXd08oAoGCCqGSM49"
    "AwEH"
    "-----END EC PRIVATE KEY-----"
};

static const char pem_x509[] = {
    "-----BEGIN CERTIFICATE-----"
    "MIIBuDCCAV2gAwIBAgIHMTAxMDEwMTAKBggqhkjOPQQDAjBCMRUwEwYDVQQLDAxv"
    "cmdhbml6YXRpb24xKTAnBgNVBAMMIDgxM2FkZDFmMWNiOTljZTk2ZmY5MTVmNTVk"
    "MzQ4MjA2MB4XDTE1MDcyMjIxMDYxNFoXDTE2MDcyMTIxMDYxNFowQjEVMBMGA1UE"
    "CwwMb3JnYW5pemF0aW9uMSkwJwYDVQQDDCAzOWIxZGNmMjBmZDJlNTNiZGYzMDU3"
    "NzMzMjBlY2RjMzBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABGJ/9F4xHn3Klw7z"
    "6LREmHJgzu8yJ4i09b4EWX6a5MgUpQoGKJcjWgYGWb86bzbciMCFpmKzfZ42Hg+k"
    "BJs2ZWajPjA8MAwGA1UdEwQFMAMBAf8wFQYDVR0lBA4wDAYKKwYBBAGC3nwBATAV"
    "BgNVHSMEDjAMoAoECELxjRK/fVhaMAoGCCqGSM49BAMCA0kAMEYCIQDixoulcO7S"
    "df6Iz6lvt2CDy0sjt/bfuYVW3GeMLNK1LAIhALNklms9SP8ZmTkhCKdpC+/fuwn0"
    "+7RX8CMop11eWCih"
    "-----END CERTIFICATE-----"
};

static const uint32_t keyexpiration = 0xFFFFFFFF;
static const char ecspeke_password[] = "1234";
static X509CertificateChain* chain = NULL;

static AJ_Status AuthListenerCallback(uint32_t authmechanism, uint32_t command, AJ_Credential*cred)
{
    AJ_Status status = AJ_ERR_INVALID;
    X509CertificateChain* node;

    AJ_AlwaysPrintf(("AuthListenerCallback authmechanism %08X command %d\n", authmechanism, command));

    switch (authmechanism) {
        case AUTH_SUITE_ECDHE_NULL:
            cred->expiration = keyexpiration;
            status = AJ_OK;
            break;

        case AUTH_SUITE_ECDHE_SPEKE:
            switch (command) {
                case AJ_CRED_PASSWORD:
                    cred->data = (uint8_t*)ecspeke_password;
                    cred->len = strlen(ecspeke_password);
                    cred->expiration = keyexpiration;
                    status = AJ_OK;
                    break;
            }
            break;

        case AUTH_SUITE_ECDHE_ECDSA:
            switch (command) {
                case AJ_CRED_PRV_KEY:
                    AJ_ASSERT(sizeof (AJ_ECCPrivateKey) == cred->len);
                    status = AJ_DecodePrivateKeyPEM((AJ_ECCPrivateKey*) cred->data, pem_prv);
                    cred->expiration = keyexpiration;
                    break;

                case AJ_CRED_CERT_CHAIN:
                    switch (cred->direction) {
                        case AJ_CRED_REQUEST:
                            // Free previous certificate chain
                            AJ_X509FreeDecodedCertificateChain(chain);
                            chain = AJ_X509DecodeCertificateChainPEM(pem_x509);
                            if (NULL == chain) {
                                return AJ_ERR_INVALID;
                            }
                            cred->data = (uint8_t*) chain;
                            cred->expiration = keyexpiration;
                            status = AJ_OK;
                            break;

                        case AJ_CRED_RESPONSE:
                            node = (X509CertificateChain*) cred->data;
                            status = AJ_X509VerifyChain(node, NULL, AJ_CERTIFICATE_IDN_X509);
                            while (node) {
                                AJ_DumpBytes("CERTIFICATE", node->certificate.der.data, node->certificate.der.size);
                                node = node->next;
                            }
                            break;
                    }
                    break;
            }
            break;

        default:
            break;
    }
    return status;
}

static const uint32_t suites[3] = { AUTH_SUITE_ECDHE_NULL, AUTH_SUITE_ECDHE_SPEKE, AUTH_SUITE_ECDHE_ECDSA };
static const size_t numsuites = 3;

static const char InterfaceName[] = "org.alljoyn.Onboarding";
static const char ServicePath[] = "/Onboarding";

static AJ_PermissionMember members[] = { { "*", AJ_MEMBER_TYPE_ANY, AJ_ACTION_PROVIDE | AJ_ACTION_OBSERVE, NULL } };
static AJ_PermissionRule rules[] = { { ServicePath, InterfaceName, members, NULL } };

AJ_Status Security_Init(AJ_BusAttachment* bus) {
    assert(bus != NULL && "bus cannot be null");
    AJ_Status status = AJ_OK;

    uint16_t state;
    uint16_t capabilities;
    uint16_t info;

    status = AJ_BusEnableSecurity(bus, suites, numsuites);
    if (AJ_OK != status) {
        AJ_ErrPrintf(("AJ_BusEnableSecurity returned an error: %s\n", (AJ_StatusText(status))));
        return status;
    }
    AJ_BusSetAuthListenerCallback(bus, AuthListenerCallback);
    AJ_ManifestTemplateSet(rules);

    AJ_SecurityGetClaimConfig(&state, &capabilities, &info);
    /* Set app claimable if not already claimed */
    if (APP_STATE_CLAIMED != state) {
        AJ_SecuritySetClaimConfig(bus, APP_STATE_CLAIMABLE, CLAIM_CAPABILITY_ECDHE_SPEKE, 0);
    } else {
        AJ_WarnPrintf(("Already claimed\n"));
    }

    return status;
}

/**
 * AboutIcon Provisioning
 */

/*#include <services/Common/AllJoynLogo.h>

const char* aboutIconMimetype = AJ_LogoMimeType;
const uint8_t* aboutIconContent = AJ_LogoData;
const size_t aboutIconContentSize = AJ_LogoSize;
const char* aboutIconUrl = AJ_LogoURL;*/

/**
 * Onboarding Provisioning
 */

static char* GenerateSoftAPSSID(char* obSoftAPssid)
{
    const char* deviceId;
    size_t deviceIdLen;
    char manufacture[AJOBS_DEVICE_MANUFACTURE_NAME_LEN + 1] = { 0 };
    size_t manufacureLen;
    char product[AJOBS_DEVICE_PRODUCT_NAME_LEN + 1] = { 0 };
    size_t productLen;
    char serialId[AJOBS_DEVICE_SERIAL_ID_LEN + 1] = { 0 };
    size_t serialIdLen;
    
    if (obSoftAPssid[0] == '\0') {
        deviceId = AJSVC_PropertyStore_GetValue(AJSVC_PROPERTY_STORE_DEVICE_ID);
        deviceIdLen = strlen(deviceId);
        manufacureLen = min(strlen(deviceManufactureName), AJOBS_DEVICE_MANUFACTURE_NAME_LEN);
        productLen = min(strlen(deviceProductName), AJOBS_DEVICE_PRODUCT_NAME_LEN);
        serialIdLen = min(deviceIdLen, AJOBS_DEVICE_SERIAL_ID_LEN);
        memcpy(manufacture, deviceManufactureName, manufacureLen);
        manufacture[manufacureLen] = '\0';
        memcpy(product, deviceProductName, productLen);
        product[productLen] = '\0';
        memcpy(serialId, deviceId + (deviceIdLen - serialIdLen), serialIdLen);
        serialId[serialIdLen] = '\0';
#ifdef _WIN32
        _snprintf(obSoftAPssid, AJOBS_SSID_MAX_LENGTH + 1, "AJ_%s_%s", manufacture, product);
#else
        snprintf(obSoftAPssid, AJOBS_SSID_MAX_LENGTH + 1, "AJ_%s_%s", manufacture, product);
#endif
    }

    return obSoftAPssid;
}

static AJOBS_Settings obSettings = AJOBS_DEFAULT_SETTINGS;

static AJ_Status Onboarding_Init()
{
    AJ_Status status = AJ_OK;

    GenerateSoftAPSSID(obSettings.AJOBS_SoftAPSSID);
    obSettings.AJOBS_SoftAPPassphrase = DEFAULT_AP_PASSWORD;
    
    printf("AJOBS_SoftAPSSID: %s\n", obSettings.AJOBS_SoftAPSSID);
    printf("AJOBS_SoftAPPassphrase: %s\n",obSettings.AJOBS_SoftAPPassphrase);
    
    status = AJOBS_Start(&obSettings);

    return status;
}

void Onboarding_DoWork(AJ_BusAttachment* bus)
{
}

AJ_Status Onboarding_Finish()
{
    return AJ_OK;
}

/*
 * Config Provisioning
 */

static AJ_Status FactoryReset()
{
    AJ_Status status = AJ_OK;

    AJ_InfoPrintf(("GOT FACTORY RESET\n"));
    status = AJSVC_PropertyStore_ResetAll();
    if (status != AJ_OK) {
        return status;
    }
    AJ_ClearCredentials(0);
    status = AJOBS_ClearInfo();
    if (status != AJ_OK) {
        return status;
    }
    return AJ_ERR_RESTART_APP;     // Force disconnect of AJ and services and reconnection of WiFi on restart of app
}

static AJ_Status Restart()
{
    AJ_InfoPrintf(("GOT RESTART REQUEST\n"));
    AJ_AboutSetShouldAnnounce(); // Set flag for sending an updated Announcement
    return AJ_ERR_RESTART_APP; // Force disconnect of AJ and services and reconnection of WiFi on restart of app
}

static uint8_t IsValueValid(const char* key, const char* value)
{
    return TRUE;
}

/*static AJ_Status Config_Init()
{
    AJ_Status status = AJCFG_Start(&FactoryReset, &Restart, NULL, &IsValueValid);
    return status;
}*/

/**
 * The AllJoyn Message Loop
 */

void onboarding_task(void *args)
{
    AJ_Status status = AJ_OK;
    uint8_t isUnmarshalingSuccessful = FALSE;
    AJSVC_ServiceStatus serviceStatus;
    AJ_Message msg;
    uint8_t forcedDisconnnect = FALSE;
    uint8_t rebootRequired = FALSE;

    AJ_Initialize();

    //AJ_AboutSetIcon(aboutIconContent, aboutIconContentSize, aboutIconMimetype, aboutIconUrl);

    status = PropertyStore_Init();
    if (status != AJ_OK) {
        printf("PropertyStore_Init returned an error: %s\n", AJ_StatusText(status));
        goto Exit;
    }

    /*status = Config_Init();
    if (status != AJ_OK) {
        printf("Config_Init returned an error: %s\n", AJ_StatusText(status));
        goto Exit;
    }*/

    status = Onboarding_Init();
    if (status != AJ_OK) {
        printf("Onboarding_Init returned an error: %s\n", AJ_StatusText(status));
        goto Exit;
    }

    while (TRUE) {
        status = AJ_OK;
        serviceStatus = AJSVC_SERVICE_STATUS_NOT_HANDLED;

        printf("onboarding_task: %u\n",isBusConnected);
        if (!isBusConnected) {
            status = AJSVC_RoutingNodeConnect(&busAttachment, ROUTING_NODE_NAME, AJAPP_CONNECT_TIMEOUT, AJAPP_CONNECT_PAUSE, AJAPP_BUS_LINK_TIMEOUT, &isBusConnected);
            if (!isBusConnected) { // Failed to connect to Routing Node?
                continue; // Retry establishing connection to Routing Node.
            }

            status = Security_Init(&busAttachment);
            if (status != AJ_OK) {
                AJ_ErrPrintf(("Security_Init returned an error: %s\n", (AJ_StatusText(status))));
                goto Exit;
            }
        }

        status = AJApp_ConnectedHandler(&busAttachment);

        if (status == AJ_OK) {
            status = AJ_UnmarshalMsg(&busAttachment, &msg, AJAPP_UNMARSHAL_TIMEOUT);
            isUnmarshalingSuccessful = (status == AJ_OK);

            if (status == AJ_ERR_TIMEOUT) {
                if (AJ_ERR_LINK_TIMEOUT == AJ_BusLinkStateProc(&busAttachment)) {
                    status = AJ_ERR_READ;             // something's not right. force disconnect
                }
            }

            if (isUnmarshalingSuccessful) {
                if (serviceStatus == AJSVC_SERVICE_STATUS_NOT_HANDLED) {
                    serviceStatus = AJApp_MessageProcessor(&busAttachment, &msg, &status);
                }
                if (serviceStatus == AJSVC_SERVICE_STATUS_NOT_HANDLED) {
                    //Pass to the built-in bus message handlers
                    status = AJ_BusHandleBusMessage(&msg);
                }
                AJ_NotifyLinkActive();
            }

            //Unmarshaled messages must be closed to free resources
            AJ_CloseMsg(&msg);
        }

        if (status == AJ_ERR_READ || status == AJ_ERR_WRITE || status == AJ_ERR_RESTART || status == AJ_ERR_RESTART_APP) {
            if (isBusConnected) {
                forcedDisconnnect = (status != AJ_ERR_READ);
                rebootRequired = (status == AJ_ERR_RESTART_APP);
                AJApp_DisconnectHandler(&busAttachment, forcedDisconnnect);
                AJSVC_RoutingNodeDisconnect(&busAttachment, forcedDisconnnect, AJAPP_SLEEP_TIME, AJAPP_SLEEP_TIME, &isBusConnected);
                if (rebootRequired) {
                    AJ_Reboot();
                }
            }
        }
    }     // while (TRUE)
    
Exit:
    AJ_X509FreeDecodedCertificateChain(chain);
    AJ_InfoPrintf(("Exit status: %s\n", (AJ_StatusText(status))));
}

void startOnboardingService(void){
    xTaskCreate(&onboarding_task, "onboarding_task", 2048, NULL, 5, NULL);
}


#ifdef AJ_MAIN
int main()
{
    return AJ_Main();
}
#endif
