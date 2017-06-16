/* 
 * File:   PWAService.h
 * Author: gauci
 *
 * Created on June 8, 2017, 1:04 PM
 */

#ifndef PWASERVICE_H
#define	PWASERVICE_H

#include <services/ServicesCommon.h>

AJ_Status AJAPP_ConnectedHandler(AJ_BusAttachment* bus);

AJSVC_ServiceStatus PWA_MessageProcessor(AJ_BusAttachment* busAttachment, AJ_Message* msg, AJ_Status* msgStatus);

#endif	/* PWASERVICE_H */


