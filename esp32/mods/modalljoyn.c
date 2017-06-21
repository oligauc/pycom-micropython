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

#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "modalljoyn.h"
#include "ServicesMain.h"

typedef struct {
    mp_obj_base_t base;
} alljoyn_obj_t;

STATIC alljoyn_obj_t alljoyn_obj;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t alljoyn_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
   
    alljoyn_obj_t *self = &alljoyn_obj;
    self->base.type = &alljoyn_type;
    
    return (mp_obj_t)self;
}

STATIC mp_obj_t start_service(mp_obj_t self_in) {
    
    startAlljoynServices();
    
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(alljoyn_start_service_obj, start_service);

STATIC const mp_map_elem_t alljoyn_locals_dict_table[] = {
   { MP_OBJ_NEW_QSTR(MP_QSTR_start_service),                (mp_obj_t)&alljoyn_start_service_obj},
};
STATIC MP_DEFINE_CONST_DICT(alljoyn_locals_dict, alljoyn_locals_dict_table);

const mp_obj_type_t alljoyn_type = {
    { &mp_type_type },
    .name = MP_QSTR_ALLJOYN,
    .make_new = alljoyn_make_new,
    .locals_dict = (mp_obj_t)&alljoyn_locals_dict,
};