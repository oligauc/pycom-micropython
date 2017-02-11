/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef ALLJOYN_MAIN_H
#define	ALLJOYN_MAIN_H

#include "modalljoyn.h"

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

void alljoyn_start_client(alljoyn_obj_t *alljoyn_obj);
void alljoyn_start_service(alljoyn_obj_t *alljoyn_obj);

#endif