/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "aj_helper.h"
#include "modalljoyn.h"
#include "alljoyn_interface.h"
#include "alljoyn_main.h"
#include "modwlan.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "mpexception.h"

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

static alljoyn_obj_t alljoyn_obj = {
        .service_name = {0},
        .service_port = -1,
        .mode         = AJ_NONE,
        .aj_args      = NULL,
        .number_args  = 0,
        .obj_indices  = {0},
        .msgIds       = NULL,
        .number_msgIds= 0,
        .callback     = NULL,
        .pyargs       = NULL,
        .nparams      = 0,
        .replyArgs    = {0},
        .obj_indices  = {0},
        .aj_args      = NULL,
        .number_args  = 0,
};

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool alljoyn_validate_port(uint32_t port);
static void alljoyn_validate_mode (uint mode);
static void alljoyn_marshal_request(const char* method_name, mp_obj_t *arguments, mp_uint_t number_arguments);    

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void parse_method_arguments(const char *method, char *args_in, uint8_t *num_in, char *args_out, uint8_t *num_out) {

	char * pch = NULL;
	
	pch = (char *)strchr(method, '<');
	while (pch != NULL) {
		args_in[*num_in] = *(pch + 1);
		pch = (char *)strchr(pch + 1, '<');

		(*num_in)++;
	}
        
	pch = (char *)strchr(method, '>');
	while (pch != NULL) {
		args_out[*num_out] = *(pch + 1);
		pch = (char *)strchr(pch + 1, '>');

		(*num_out)++;
	}
}

void alljoyn_free_service(void) {
    free(alljoyn_obj.msgIds);
}

void alljoyn_free_client(void) {
    free(alljoyn_obj.aj_args);
}
    
/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC const mp_arg_t alljoyn_init_args[] = {
    { MP_QSTR_mode,          MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = AJ_NONE} },
    { MP_QSTR_service_name,  MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ,  {.u_obj = MP_OBJ_NULL} },
    { MP_QSTR_service_port,  MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT,  {.u_int = -1} },
};

STATIC mp_obj_t alljoyn_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
   
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(alljoyn_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), alljoyn_init_args, args);
    
    // setup the object
    alljoyn_obj_t *self = &alljoyn_obj;
    self->base.type = (mp_obj_t)&alljoyn_type;
  
    self->mode = args[0].u_int;
    alljoyn_validate_mode (self->mode);
   
    mp_uint_t sn_len = 0;
    const char *service_name = mp_obj_str_get_data(args[1].u_obj, &sn_len);
    memset(self->service_name,0, MOD_ALLJOYN_SERVICENAME_MAX);
    memcpy(self->service_name,service_name, sn_len);
    
    self->service_port = (uint16_t)args[2].u_int;
    if (!alljoyn_validate_port(self->service_port)){
        mp_raise_ValueError("Service port must be between 0 and 65536\n");
    } 
    
    return (mp_obj_t)self;
}

STATIC mp_obj_t add_interface(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t add_interface_args[] = {
        { MP_QSTR_service_path,     MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_interface_name,   MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_interface_methods,MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_callbacks,        MP_ARG_KW_ONLY  | MP_ARG_OBJ,                      {.u_obj = MP_OBJ_NULL} },
    };
    
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(add_interface_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), add_interface_args, args);
    
    // service path
    mp_uint_t len = 0;
    const char *service_path = mp_obj_str_get_data(args[0].u_obj, &len);
    
    // interface name
    const char *interface_name = mp_obj_str_get_data(args[1].u_obj, &len);
    
    // interface methods
    mp_uint_t number_methods = 0;
    mp_obj_t *interface_methods;
    mp_obj_get_array(args[2].u_obj, &number_methods, &interface_methods);
    
    //callbacks
    mp_uint_t number_callbacks = 0;
    mp_obj_t *callbacks;
    if (args[3].u_obj != MP_OBJ_NULL){
        mp_obj_get_array(args[3].u_obj, &number_callbacks, &callbacks);
    }
        
    if (number_callbacks == 0){
        mp_raise_ValueError("Callbacks required for service mode\n");
    }
        
    if (number_callbacks != number_methods){
        mp_raise_ValueError("Missing callbacks\n");
    }
    
    for (uint8_t idx = 0; idx < number_methods; idx++){
        char *method_name = (char *)mp_obj_str_get_data(interface_methods[idx], &len);
        addObject((char *)service_path, (char *)interface_name, &method_name, 1, &callbacks[idx]);
    }
            
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(alljoyn_add_interface_obj, 1, add_interface);

STATIC mp_obj_t call_method(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
        STATIC const mp_arg_t call_method_args[] = {
        { MP_QSTR_name,         MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ,     {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_arguments,    MP_ARG_OBJ,                                        {.u_obj = MP_OBJ_NULL} },
    };
    
    if (alljoyn_obj.mode == AJ_NONE){
        mp_raise_ValueError("Operating mode not set\n");
    }
    
    if (!hasAlljoynObjects()){
        mp_raise_ValueError("No interfaces found\n");
    }
           
    mp_arg_val_t args[MP_ARRAY_SIZE(call_method_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), call_method_args, args);
  
    // calling method
    mp_uint_t len = 0;
    mp_obj_t *calling_method;
    mp_obj_get_array_fixed_n(args[0].u_obj, 3, &calling_method);    
    
    const char* service_path = mp_obj_str_get_data(calling_method[0], &len);
    const char* interface_name = mp_obj_str_get_data(calling_method[1], &len);
    const char* interface_method = mp_obj_str_get_data(calling_method[2], &len);
    
    if (!getAJObjectIndex(service_path, interface_name, interface_method, alljoyn_obj.obj_indices)){
        mp_raise_ValueError("calling method not found\n");
    }
   
    mp_uint_t number_arguments = 0;
    mp_obj_t *arguments;
    mp_obj_get_array(args[1].u_obj, &number_arguments , &arguments); 
    
    alljoyn_marshal_request(interface_method, arguments, number_arguments);
    
    alljoyn_start_client(&alljoyn_obj);
       
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(alljoyn_call_method_obj, 1, call_method);

STATIC mp_obj_t start_service(mp_obj_t self_in) {
    
    if (alljoyn_obj.mode == AJ_NONE){
        mp_raise_ValueError("Operating mode not set\n");
    }
    
    if (!hasAlljoynObjects()){
        mp_raise_ValueError("No interfaces found\n");
    }
   
    alljoyn_obj.number_msgIds = getTotalNumberMethods();
    alljoyn_obj.msgIds = (uint32_t *)malloc(alljoyn_obj.number_msgIds * sizeof(uint32_t));
    getMsgIdList(alljoyn_obj.msgIds, true);
    
    alljoyn_start_service(&alljoyn_obj);
   
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(alljoyn_start_service_obj, start_service);

STATIC mp_obj_t service_reply(size_t n_args, const mp_obj_t *args) {
    
    AJ_Message reply;
    mp_uint_t len = 0;
    size_t oparams = strlen(alljoyn_obj.replyArgs);
    
    if ((n_args - 1) != oparams){
        mp_raise_ValueError("Wrong number of parameters\n");
    }
    
    AJ_MarshalReplyMsg(&alljoyn_obj.msg, &reply);
    for (uint8_t idx = 0; idx < n_args - 1; idx++){
        AJ_Arg replyArg;
        
        if (alljoyn_obj.replyArgs[idx] == AJ_ARG_STRING){
            char *reply_str = (char *)mp_obj_str_get_data(args[idx + 1], &len);
            AJ_InitArg(&replyArg, AJ_ARG_STRING, 0, reply_str, 0);
        } else if (alljoyn_obj.replyArgs[idx] == AJ_ARG_INT32) {
            mp_int_t value = mp_obj_get_int(args[idx + 1]);
            AJ_InitArg(&replyArg, AJ_ARG_INT32, 0, (void *)value, 0);
        } else if (alljoyn_obj.replyArgs[idx] == AJ_ARG_INT16) {
            mp_int_t value = mp_obj_get_int(args[idx + 1]);
            AJ_InitArg(&replyArg, AJ_ARG_INT16, 0, (void *)value, 0);
        } else if (alljoyn_obj.replyArgs[idx] == AJ_ARG_UINT32) {
            mp_int_t value = mp_obj_get_int(args[idx + 1]);
            AJ_InitArg(&replyArg, AJ_ARG_UINT32, 0, (void *)value, 0);
        } else if (alljoyn_obj.replyArgs[idx] == AJ_ARG_UINT16) {
            mp_int_t value = mp_obj_get_int(args[idx + 1]);
            AJ_InitArg(&replyArg, AJ_ARG_UINT16, 0, (void *)value, 0);
        } else if (alljoyn_obj.replyArgs[idx] == AJ_ARG_BOOLEAN) {
            mp_int_t value = mp_obj_get_int(args[idx + 1]);
            AJ_InitArg(&replyArg, AJ_ARG_BOOLEAN, 0, (void *)value, 0);
        } else {
            mp_raise_ValueError("Unsupported type\n");
        }
        
        AJ_MarshalArg(&reply, &replyArg);
    }
    
    AJ_DeliverMsg(&reply);
     
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(alljoyn_service_reply_obj, 0, 20, service_reply);

STATIC const mp_map_elem_t alljoyn_locals_dict_table[] = {
   { MP_OBJ_NEW_QSTR(MP_QSTR_add_interface),                (mp_obj_t)&alljoyn_add_interface_obj},
   { MP_OBJ_NEW_QSTR(MP_QSTR_call_method),                  (mp_obj_t)&alljoyn_call_method_obj}, 
   { MP_OBJ_NEW_QSTR(MP_QSTR_start_service),                (mp_obj_t)&alljoyn_start_service_obj},
   { MP_OBJ_NEW_QSTR(MP_QSTR_service_reply),                (mp_obj_t)&alljoyn_service_reply_obj},
    
   { MP_OBJ_NEW_QSTR(MP_QSTR_CLIENT),                 MP_OBJ_NEW_SMALL_INT(AJ_CLIENT) },
   { MP_OBJ_NEW_QSTR(MP_QSTR_SERVICE),                MP_OBJ_NEW_SMALL_INT(AJ_SERVICE)},
};
STATIC MP_DEFINE_CONST_DICT(alljoyn_locals_dict, alljoyn_locals_dict_table);

const mp_obj_type_t alljoyn_type = {
    { &mp_type_type },
    .name = MP_QSTR_ALLJOYN,
    .make_new = alljoyn_make_new,
    .locals_dict = (mp_obj_t)&alljoyn_locals_dict,
};

static void alljoyn_validate_mode (uint mode) {
    if (mode > AJ_SERVICE) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, mpexception_value_invalid_arguments));
    }
}

static bool alljoyn_validate_port(uint32_t port) {
    
    if ((port < 0) || (port > MOD_ALLJOYN_MAX_PORT)){
        return false;
    }
    
    return true;
}

static void alljoyn_marshal_request(const char* method_name, mp_obj_t *arguments, mp_uint_t number_arguments){
    
    mp_uint_t len = 0;
    uint8_t num_args_in = 0;
    uint8_t num_args_out = 0;
    char args_in[MAX_NUMBER_ARGS] = { 0 };
    char args_out[MAX_NUMBER_ARGS] = { 0 };
    
    alljoyn_obj.number_args = number_arguments;
    parse_method_arguments(method_name, args_in, &num_args_in, args_out, &num_args_out);
    
    if (number_arguments != num_args_in){
        mp_raise_ValueError("Wrong number of parameters\n");
    }
    
    alljoyn_obj.aj_args = (AJ_Arg *)malloc(sizeof(AJ_Arg) * number_arguments);
    for(int idx = 0; idx < number_arguments; idx++){
        if (args_in[idx] == AJ_ARG_STRING){
            char *request_str = (char *)mp_obj_str_get_data(arguments[idx], &len);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_STRING, 0, (void *)request_str, 0);  
        } else if (args_in[idx] == AJ_ARG_INT32){
            mp_int_t value = mp_obj_get_int(arguments[idx]);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_INT32, 0, (void *)value, 0);
        } else if (args_in[idx] == AJ_ARG_INT16) {
            mp_int_t value = mp_obj_get_int(arguments[idx]);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_INT16, 0, (void *)value, 0);
        } else if (args_in[idx] == AJ_ARG_UINT32) {
            mp_int_t value = mp_obj_get_int(arguments[idx]);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_UINT32, 0, (void *)value, 0);
        } else if (args_in[idx] == AJ_ARG_UINT16) {
            mp_int_t value = mp_obj_get_int(arguments[idx]);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_UINT16, 0, (void *)value, 0);
        } else if (args_in[idx] == AJ_ARG_BOOLEAN) {
            mp_int_t value = mp_obj_get_int(arguments[idx]);
            AJ_InitArg(&alljoyn_obj.aj_args[idx], AJ_ARG_BOOLEAN, 0, (void *)value, 0);
        } else {
            mp_raise_ValueError("Unsupported type\n");
        }
    }
}