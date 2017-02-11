/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODALLJOYN_H_
#define MODALLJOYN_H_

#include "py/obj.h"
#include "aj_bus.h"
#include "aj_debug.h"
#include "alljoyn.h"

/******************************************************************************
 DEFINE PUBLIC CONSTANTS
 ******************************************************************************/

#define MOD_ALLJOYN_SERVICENAME_MAX            100
#define MOD_ALLJOYN_MAX_PORT                   65535
#define MAX_NUMBER_ARGS                        10

/******************************************************************************
 DEFINE PUBLIC TYPES
 ******************************************************************************/

enum aj_usage_mode {AJ_CLIENT = 0, AJ_SERVICE, AJ_NONE};

typedef struct {
  mp_obj_base_t     base;
  char              service_name[MOD_ALLJOYN_SERVICENAME_MAX];
  uint16_t 	    service_port;
 
  uint32_t          *msgIds;
  uint8_t           number_msgIds;
  mp_obj_t          callback;
 
  AJ_Message        msg;
  mp_obj_t          *pyargs;
  mp_int_t          nparams; 
  
  char              replyArgs[MAX_NUMBER_ARGS ];
  
  uint8_t           obj_indices[3];
  
  AJ_Arg            *aj_args;
  uint8_t           number_args;
  
  enum aj_usage_mode     mode;
 
} alljoyn_obj_t;

extern const mp_obj_type_t alljoyn_type;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

void alljoyn_free_client(void);
void alljoyn_free_service(void);
void parse_method_arguments(const char *method, char *args_in, uint8_t *num_in, char *args_out, uint8_t *num_out);

#endif
