/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef ALLJOYN_H_
#define ALLJOYN_H_

#include "py/obj.h"
#include "aj_bus.h"

#define MOD_ALLJOYN_SERVICENAME_MAX            100
#define MOD_ALLJOYN_MAX_PORT                   65535

enum aj_usage_mode {AJ_CLIENT = 0, AJ_SERVICE, AJ_NONE};
enum aj_data_types {AJ_INVALID= 0, AJ_ARRAY, AJ_STRING, AJ_BOOLEAN, AJ_DOUBLE, AJ_SIGNATURE, AJ_HANDLE,
        AJ_INT32, AJ_INT16, AJ_OBJ_PATH, AJ_UINT16, AJ_UINT64, AJ_UINT32, AJ_VARIANT, AJ_INT64, AJ_BYTE };

typedef struct {
  mp_obj_base_t     base;
  char              service_name[MOD_ALLJOYN_SERVICENAME_MAX];
  uint16_t 	    service_port;
 
  AJ_Arg            *aj_args;
  uint8_t           number_args;
  uint8_t           obj_indices[3];
  
  mp_obj_t          callbacks[2];  
  
  enum aj_usage_mode     mode;
 
} alljoyn_obj_t;

extern const mp_obj_type_t alljoyn_type;

#endif
