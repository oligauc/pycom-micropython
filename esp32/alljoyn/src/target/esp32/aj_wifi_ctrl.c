/**
 * @file Functions relating to wifi configuration and initialization
 */
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

#define AJ_MODULE WIFI


#include <stdio.h>

#include <aj_target.h>
#include <aj_util.h>
#include <aj_status.h>
#include <aj_wifi_ctrl.h>
#include <aj_debug.h>
#include <aj_target_rtos.h>

#include "py/obj.h"
#include "modwlan.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "serverstask.h"
#include "antenna.h"
#include "mpconfigport.h"

#include "freertos/event_groups.h"

extern wlan_obj_t wlan_obj;
extern EventGroupHandle_t wifi_event_group;

//#include <aj_wsl_target.h>
//#include <aj_wsl_wmi.h>

/**
 * Turn on per-module debug printing by setting this variable to non-zero value
 * (usually in debugger).
 */
#ifndef NDEBUG
uint8_t dbgWIFI = 0;
#endif

//extern AJ_WifiCallbackFunc AJ_WSL_WifiConnectCallback;

/*static char* AddrStr(uint32_t addr)
{
    static char txt[17];
    sprintf((char*)&txt, "%3u.%3u.%3u.%3u",
            (addr & 0xFF000000) >> 24,
            (addr & 0x00FF0000) >> 16,
            (addr & 0x0000FF00) >>  8,
            (addr & 0x000000FF)
            );

    return txt;
}*/

static AJ_Status AJ_Network_Up();

#define MAX_SSID_LENGTH                 32
#define DEFAULT_STA_CHANNEL             1


static const uint32_t startIP = 0xC0A80101;
static const uint32_t endIP   = 0xC0A80102;

#define IP_LEASE    (60 * 60 * 1000)

static const uint8_t IP6RoutePrefix[16] = { 0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

#define PREFIX_LEN          64
#define PREFIX_LIFETIME  12000

static const int CONNECTED_BIT = BIT0;
static uint8_t wifi_initialized = TRUE;
AJ_WiFiConnectState AJ_ESP32_connectState = AJ_WIFI_IDLE;
//static uint32_t athSecType = AJ_WIFI_SECURITY_NONE;

static bool AJ_Wifi_isConnected(){
    if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
        return true;
    }
    return false;
}
    
static int AJ_GetNumSTAs(void)
{
    wifi_sta_list_t sta_list = {.num = 0};
    
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK){
        //printf("esp_wifi_ap_get_sta_list: %d\n", sta_list.num);
        return sta_list.num;  
    }
    
    return -1;
}

AJ_WiFiConnectState AJ_GetWifiConnectState(void)
{
    wifi_mode_t mode;
    AJ_ESP32_connectState = AJ_WIFI_IDLE;
            
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        //printf("AJ_GetWifiConnectState mode: %u\n", mode);
        if ((mode == WIFI_MODE_STA) || (mode == WIFI_MODE_APSTA)){
            if (AJ_Wifi_isConnected()){
                AJ_ESP32_connectState = AJ_WIFI_CONNECT_OK;
                //printf("AJ_GetWifiConnectState AJ_WIFI_CONNECT_OK\n");
            } else {
                AJ_ESP32_connectState = AJ_WIFI_CONNECTING;
            }
        } else if (mode == WIFI_MODE_AP){
            if (AJ_GetNumSTAs() <= 0){
                AJ_ESP32_connectState = AJ_WIFI_SOFT_AP_UP;
            } else {
                AJ_ESP32_connectState = AJ_WIFI_STATION_OK;
                printf("AJ_ESP32_connectState : AJ_WIFI_STATION_OK\n");
            }
            //printf("AJ_GetWifiConnectState AJ_WIFI_SOFT_AP_UP\n");
        }
    }
           
    return AJ_ESP32_connectState;
}


#define RSNA_AUTH_FAILURE  10
#define RSNA_AUTH_SUCCESS  16

//static void WiFiCallback(int val)
//{
    /*AJ_InfoPrintf(("\nWiFiCallback %d\n", val));
    if (val == 0) {
        if (AJ_WSL_connectState == AJ_WIFI_DISCONNECTING || AJ_WSL_connectState == AJ_WIFI_CONNECT_OK) {
            AJ_WSL_connectState = AJ_WIFI_IDLE;
            AJ_InfoPrintf(("\nWiFi Disconnected\n"));
        } else if (AJ_WSL_connectState != AJ_WIFI_CONNECT_FAILED) {
            AJ_WSL_connectState = AJ_WIFI_CONNECT_FAILED;
            AJ_InfoPrintf(("\nWiFi Connect Failed\n"));
        }
    } else if (val == 1) {
        if ((athSecType == AJ_WIFI_SECURITY_NONE) || (athSecType == AJ_WIFI_SECURITY_WEP)) {
            AJ_WSL_connectState = AJ_WIFI_CONNECT_OK;
            AJ_InfoPrintf(("\nConnected to AP\n"));
        }
    } else if (val == RSNA_AUTH_SUCCESS) {
        AJ_WSL_connectState = AJ_WIFI_CONNECT_OK;
        AJ_InfoPrintf(("\nConnected to AP\n"));
    } else if (val == RSNA_AUTH_FAILURE) {
        AJ_WSL_connectState = AJ_WIFI_AUTH_FAILED;
        AJ_InfoPrintf(("\nWiFi Authentication Failed\n"));
    }

    if (AJ_WSL_connectState == AJ_WIFI_CONNECT_OK) {
        AJ_WSL_NET_ip6config_router_prefix(IP6RoutePrefix, PREFIX_LEN, PREFIX_LIFETIME, PREFIX_LIFETIME);
    }*/
//}

//static void SoftAPCallback(int val)
//{
    /*if (val == 0) {
        if (AJ_WSL_connectState == AJ_WIFI_DISCONNECTING || AJ_WSL_connectState == AJ_WIFI_SOFT_AP_UP) {
            AJ_WSL_connectState = AJ_WIFI_IDLE;
            AJ_InfoPrintf(("Soft AP Down\n"));
        } else if (AJ_WSL_connectState == AJ_WIFI_STATION_OK) {
            AJ_WSL_connectState = AJ_WIFI_SOFT_AP_UP;
            AJ_InfoPrintf(("Soft AP Station Disconnected\n"));
        } else {
            AJ_WSL_connectState = AJ_WIFI_CONNECT_FAILED;
            AJ_InfoPrintf(("Soft AP Connect Failed\n"));
        }
    } else if (val == 1) {
        if (AJ_WSL_connectState == AJ_WIFI_SOFT_AP_INIT) {
            AJ_InfoPrintf(("Soft AP Initialized\n"));
            AJ_WSL_connectState = AJ_WIFI_SOFT_AP_UP;
        } else {
            AJ_InfoPrintf(("Soft AP Station Connected\n"));
            AJ_WSL_connectState = AJ_WIFI_STATION_OK;
        }
    }*/
//}

AJ_Status AJ_Wifi_disconnect(void)
{
    AJ_Status status = AJ_OK;
    
    if ((wlan_obj.mode == WIFI_MODE_STA) && (!wlan_obj.disconnected)){
       
        if (esp_wifi_disconnect() == ESP_OK){
            wlan_obj.disconnected = true;
            printf("AJ_Wifi_disconnect disconnected: %d\n", wlan_obj.disconnected);
        } else {
            status = AJ_ERR_FAILURE;
        }
    }
    
    return status;
}

AJ_Status AJ_Wifi_DriverStop(void)
{
    AJ_ESP32_connectState = AJ_WIFI_IDLE;
    
    if (servers_are_enabled()){
       wlan_servers_stop();
    }

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    //esp_wifi_deinit();
           
    return AJ_OK;
}

AJ_Status AJ_Wifi_DriverStart(void)
{        
    wlan_setup (WIFI_MODE_STA, NULL, 0, 0, NULL, 0,
            0, ANTENNA_TYPE_INTERNAL, false);
    
    return AJ_OK;
}

AJ_Status AJ_PrintFWVersion()
{
    printf("AJ_PrintFWVersion\n");
            
    AJ_Status status = AJ_OK;
    //AJ_FW_Version version;
    AJ_Network_Up();

    /*extern AJ_FW_Version AJ_WSL_TargetFirmware;
    version = AJ_WSL_TargetFirmware;
    if (status == AJ_OK) {
        AJ_InfoPrintf(("Host version :  %ld.%ld.%ld.%ld.%ld\n",
                       (version.host_ver & 0xF0000000) >> 28,
                       (version.host_ver & 0x0F000000) >> 24,
                       (version.host_ver & 0x00FC0000) >> 18,
                       (version.host_ver & 0x0003FF00) >> 8,
                       (version.host_ver & 0x000000FF)));

        AJ_InfoPrintf(("Target version   :  0x%lx\n", version.target_ver));
        AJ_InfoPrintf(("Firmware version :  %ld.%ld.%ld.%ld.%ld\n",
                       (version.wlan_ver & 0xF0000000) >> 28,
                       (version.wlan_ver & 0x0F000000) >> 24,
                       (version.wlan_ver & 0x00FC0000) >> 18,
                       (version.wlan_ver & 0x0003FF00) >> 8,
                       (version.wlan_ver & 0x000000FF)));
        AJ_InfoPrintf(("Interface version:  %ld\n", version.abi_ver));
    }*/
    return status;
}

#define DHCP_TIMEOUT 5000

AJ_Status AJ_ConnectWiFiHelper(const char* ssid, AJ_WiFiSecurityType secType, const char* passphrase) {
    
    AJ_Status status = AJ_OK;
    
    wifi_config_t config;
    memset(&config, 0, sizeof(config));

    // first close any active connections
    esp_wifi_disconnect();

    wlan_obj.disconnected = false;

    size_t ssid_len = strlen(ssid);
    memcpy(config.sta.ssid, ssid, ssid_len);
    if (passphrase) {
        size_t pass_len = strlen(passphrase);
        memcpy(config.sta.password, passphrase, pass_len);
    }
    
    esp_wifi_set_config(WIFI_IF_STA, &config);
    esp_wifi_connect();
    
    AJ_Sleep(10000);
    
    if (AJ_Wifi_isConnected()){
        printf("+++ Wifi connected\n");
        //xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        AJ_ESP32_connectState = AJ_WIFI_CONNECT_OK;
        status = AJ_OK;
    } else {
        printf("+++ Wifi not connected\n");
        wlan_obj.disconnected = true;
        //xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        AJ_ESP32_connectState = AJ_WIFI_CONNECT_FAILED;
        status = AJ_ERR_SECURITY;
    }
   
    return status;
}

AJ_Status AJ_ConnectWiFi(const char* ssid, AJ_WiFiSecurityType secType, AJ_WiFiCipherType cipherType, const char* passphrase)
{
    printf("AJ_ConnectWiFi\n");
     
    AJ_Status status = AJ_OK;
    
    wlan_setup (WIFI_MODE_STA, NULL, 0, secType, NULL, 0,
                 DEFAULT_STA_CHANNEL, ANTENNA_TYPE_INTERNAL, false);
     
    status = AJ_ConnectWiFiHelper(ssid, secType,  passphrase);
    
    return status;
}

AJ_Status AJ_DisconnectWiFi(void)
{
    printf("AJ_DisconnectWiFi\n");
    
    AJ_Status status = AJ_OK;
    AJ_WiFiConnectState oldState = AJ_ESP32_connectState;

    if (oldState != AJ_WIFI_DISCONNECTING) {
        
        if (oldState != AJ_WIFI_IDLE) {
            AJ_ESP32_connectState = AJ_WIFI_DISCONNECTING;
        }
        AJ_Wifi_disconnect();
    }
    return status;
}

static AJ_Status AJ_EnableSoftAPHelper(const char* ssid, uint8_t hidden, const char* passphrase)
{
    printf("AJ_EnableSoftAPHelper\n");
    
    AJ_Status status = AJ_OK;
      
    AJ_ESP32_connectState = AJ_WIFI_IDLE;
    if (strlen(ssid) > MAX_SSID_LENGTH) {
        AJ_ErrPrintf(("SSID length exceeds Maximum value\n"));
        return AJ_ERR_INVALID;
    }
    
    printf("AJ_EnableSoftAPHelper - ssid %s, passphrase %s\n", ssid, passphrase);
    
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(passphrase);
    
    printf("AJ_EnableSoftAPHelper - before wlansetup\n");
    
    wlan_setup(WIFI_MODE_AP, ssid, ssid_len, WIFI_AUTH_WPA2_PSK, passphrase, pass_len,
                        DEFAULT_AP_CHANNEL, ANTENNA_TYPE_INTERNAL, false);
                
    AJ_ESP32_connectState = AJ_WIFI_SOFT_AP_UP;

    return status;
}


#define SOFTAP_SLEEP_TIMEOUT 100
// block until somebody connects to us or the timeout expires
AJ_Status AJ_EnableSoftAP(const char* ssid, uint8_t hidden, const char* passphrase, const uint32_t timeout)
{
    printf("AJ_EnableSoftAP\n");
    
    AJ_Status status = AJ_OK;
    uint32_t time2 = 0;

    status = AJ_EnableSoftAPHelper(ssid, hidden, passphrase);
    if (status != AJ_OK) {
        AJ_ErrPrintf(("AJ_EnableSoftAP error\n"));
        return status;
    }

    printf("Waiting for remote station to connect\n");

    do {
        AJ_Sleep(SOFTAP_SLEEP_TIMEOUT);
        time2 += SOFTAP_SLEEP_TIMEOUT;
    } while (AJ_GetWifiConnectState() != AJ_WIFI_STATION_OK && (timeout == 0 || time2 < timeout));

    printf("Exiting AJ_EnableSoftAP\n");
    return (AJ_GetWifiConnectState() == AJ_WIFI_STATION_OK) ? AJ_OK : AJ_ERR_TIMEOUT;
}

AJ_Status AJ_WiFiScan(void* context, AJ_WiFiScanResult scanCallback, uint8_t maxAPs)
{
    printf("AJ_WiFiScan\n");
     
    AJ_Status status = AJ_OK;
    
    status = AJ_Network_Up();
    if (status != AJ_OK) {
        AJ_ErrPrintf(("AJ_WiFiScan(): AJ_Network_Up error"));
        return status;
    }
   
    esp_wifi_scan_start(NULL, true);

    uint16_t ap_num = 0;
    wifi_ap_record_t *ap_record_buffer = NULL;
    wifi_ap_record_t *ap_record = NULL;

    esp_wifi_scan_get_ap_num(&ap_num); // get the number of scanned APs
    printf("AJ_WiFiScan ap_num: %u\n",ap_num);
    
    ap_record_buffer = pvPortMalloc(ap_num * sizeof(wifi_ap_record_t));
    if (ap_record_buffer == NULL){
        status = AJ_ERR_RESOURCES;
    }
    
    if (ESP_OK == esp_wifi_scan_get_ap_records(&ap_num, (wifi_ap_record_t *)ap_record_buffer)) {
        for (int i = 0; i < ap_num; i++) {
            ap_record = &ap_record_buffer[i];
            printf("AJ_WiFiScan ssid: %s, bssid %s, rssi %d, auth mode %d\n", (const char *)ap_record->ssid, (const char *)ap_record->bssid, ap_record->rssi, (int)ap_record->authmode);
            scanCallback(context, (const char *)ap_record->ssid, (const byte *)ap_record->bssid, ap_record->rssi, ap_record->authmode, AJ_WIFI_CIPHER_NONE);
        }
    } else {
        status = AJ_ERR_FAILURE;
    }

    vPortFree(ap_record_buffer);
    
    return status;
}

/*static uint8_t get_tx_status()
{
    return 0;
}*/

AJ_Status AJ_SuspendWifi(uint32_t msec)
{
    printf("AJ_SuspendWifi\n");
    
    //static uint8_t suspendEnabled = FALSE;
    return AJ_OK;
}

static AJ_Status AJ_Network_Up()
{
    printf("AJ_Network_Up\n");
    
    AJ_Status status = AJ_OK;
    
    if (wifi_initialized == FALSE) {
        wifi_initialized = TRUE;
        
        status = AJ_Wifi_DriverStart();
        if (status != AJ_OK) {
            AJ_ErrPrintf(("AJ_Wifi_DriverStart failed %d\n", status));
            return AJ_ERR_DRIVER;
        }
    }

    return status;
}


static AJ_Status AJ_Network_Down()
{
    AJ_Status err;
    AJ_ESP32_connectState = AJ_WIFI_IDLE;

    if (wifi_initialized == TRUE) {
        wifi_initialized = FALSE;
        
        err = AJ_Wifi_DriverStop();
        if (err != AJ_OK) {
            AJ_ErrPrintf(("AJ_WSL_DriverStop failed %d\n", err));
            return AJ_ERR_DRIVER;
        }
    }

    return AJ_OK;
}

AJ_Status AJ_ResetWiFi(void)
{
    printf("AJ_ResetWiFi\n");
    
    AJ_Status status = AJ_OK;
    AJ_InfoPrintf(("Reset WiFi driver\n"));
    AJ_ESP32_connectState = AJ_WIFI_IDLE;
    status = AJ_Network_Down();
    if (status != AJ_OK) {
        AJ_ErrPrintf(("AJ_ResetWiFi(): AJ_Network_Down failed %s\n", AJ_StatusText(status)));
        return status;
    }
    wifi_initialized = FALSE; // restarting _everything_
    //status = AJ_Network_Up();
    
    return status;
}

AJ_Status AJ_GetIPAddress(uint32_t* ip, uint32_t* mask, uint32_t* gateway)
{
    printf("AJ_GetIPAddress\n");
    
    // set to zero first
    /**ip = *mask = *gateway = 0;

    return AJ_WSL_ipconfig(IPCONFIG_QUERY, ip, mask, gateway);*/
    return AJ_OK;
}

#define DHCP_WAIT       100

AJ_Status AJ_AcquireIPAddress(uint32_t* ip, uint32_t* mask, uint32_t* gateway, int32_t timeout)
{
    printf("AJ_AcquireIPAddress\n");
    
    AJ_Status status = AJ_OK;
    /*AJ_WiFiConnectState current_wifi_state = AJ_GetWifiConnectState();

    switch (current_wifi_state) {
    case AJ_WIFI_CONNECT_OK:
        break;

    // no need to do anything in Soft-AP mode
    case AJ_WIFI_SOFT_AP_INIT:
    case AJ_WIFI_SOFT_AP_UP:
    case AJ_WIFI_STATION_OK:
        return AJ_OK;

    // shouldn't call this function unless already connected!
    case AJ_WIFI_IDLE:
    case AJ_WIFI_CONNECTING:
    case AJ_WIFI_CONNECT_FAILED:
    case AJ_WIFI_AUTH_FAILED:
    case AJ_WIFI_DISCONNECTING:
        return AJ_ERR_DHCP;
    }

    status = AJ_GetIPAddress(ip, mask, gateway);
    if (status != AJ_OK) {
        return status;
    }

    while (0 == *ip) {
        if (timeout < 0) {
            AJ_ErrPrintf(("AJ_AcquireIPAddress(): DHCP Timeout\n"));
            return AJ_ERR_TIMEOUT;
        }

        AJ_InfoPrintf(("Sending DHCP request\n"));
        
        status = AJ_WSL_ipconfig(IPCONFIG_DHCP, ip, mask, gateway);
        if (status != AJ_OK) {
            return AJ_ERR_DHCP;
        }

        AJ_Sleep(DHCP_WAIT);
        status = AJ_GetIPAddress(ip, mask, gateway);
        if (status != AJ_OK) {
            return status;
        }
        timeout -= DHCP_WAIT;
    }

    if (status == AJ_OK) {
        AJ_InfoPrintf(("*********** DHCP succeeded %s\n", AddrStr(*ip)));
    }*/

    return status;
}

AJ_Status AJ_SetIPAddress(uint32_t ip, uint32_t mask, uint32_t gateway)
{
    printf("AJ_SetIPAddress\n");
    
    //return AJ_WSL_ipconfig(IPCONFIG_STATIC, &ip, &mask, &gateway);
    return AJ_OK;
}
