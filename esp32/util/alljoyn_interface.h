/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
 

#ifndef ALLJOYN_INTERFACE_H
#define	ALLJOYN_INTERFACE_H

#include "aj_debug.h"
#include <alljoyn.h>
#include "py/runtime.h"
#include "py/obj.h"

bool hasAlljoynObjects(void);
AJ_Object* getAlljoynObjects(void);
bool getAJObjectIndex(const char *service_path, const char *inteface_name, const char *interface_method, uint8_t *indices);
void addObject(char* new_service_path, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks);

#endif	/* ALLJOYN_INTERFACE_H */

