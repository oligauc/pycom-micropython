/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include "alljoyn_interface.h"

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct PY_AJ_METHOD {
    char *method_name;
    mp_obj_t callback;
    struct PY_AJ_METHOD *next;
} PY_AJ_METHOD_t;

typedef struct PY_AJ_ITF {
	char *interface_name;
        PY_AJ_METHOD_t *methods;
	struct PY_AJ_ITF *next;
} PY_AJ_ITF_t;

typedef struct PY_AJ_OBJECT {
	char *service_path;
	PY_AJ_ITF_t *interface;
	struct PY_AJ_OBJECT *next;
} PY_AJ_OBJECT_t;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void freeLinkedLists(void);
static uint8_t getNumAjObjects(PY_AJ_OBJECT_t * root);
static uint8_t getNumInterfaces(PY_AJ_ITF_t *root);
static uint8_t getNumMethods(PY_AJ_METHOD_t *root);
bool getMethodParameters(PY_AJ_METHOD_t *node, uint8_t dept,char **method, mp_obj_t *callback);
static bool getInterfaceIndices(PY_AJ_ITF_t *node, const char *interface_name, const char *interface_methods, uint8_t *indices);
static void addMethod(PY_AJ_METHOD_t *node, char *new_method, mp_obj_t *new_callbacks);
static void addInterfaceName(PY_AJ_OBJECT_t *node, char* new_interface_name, char *new_interface_method, mp_obj_t *new_callbacks);
static void newInterface(PY_AJ_ITF_t **node, char *new_interface_name);
static void newServicePath(PY_AJ_OBJECT_t **node, char *new_service_path);
static void newObject(PY_AJ_OBJECT_t **node, char* new_service_path, char* new_interface_name, char *new_interface_method, mp_obj_t *new_callbacks);
static void newMethod(PY_AJ_METHOD_t **node_method, char *new_interface_method, mp_obj_t *new_callback);

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
 static PY_AJ_OBJECT_t *py_aj_obj_root = NULL;
 static AJ_Object* alljoyn_obj = NULL;

 /******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

bool hasAlljoynObjects(void) {
    if (py_aj_obj_root == NULL){
        return false;
    }

    return true;
}

void getMsgIdList(uint32_t *msgIds, bool isService){

    uint8_t msgIdx = 0;
    PY_AJ_OBJECT_t *current_service = py_aj_obj_root;

    uint8_t obj_idx = 0;
    while (current_service != NULL) {

        uint8_t itf_idx = 0;
	PY_AJ_ITF_t *current_interface = current_service->interface;
	while (current_interface != NULL) {

            uint8_t num_methods = getNumMethods(current_interface->methods);

            for (int m_idx = 0; m_idx < num_methods; m_idx++){
                if (isService){
                    msgIds[msgIdx++] = AJ_APP_MESSAGE_ID(obj_idx, itf_idx, m_idx);
                } else {
                    msgIds[msgIdx++] = AJ_PRX_MESSAGE_ID(obj_idx, itf_idx, m_idx);
                }
            }
            current_interface = current_interface->next;
            itf_idx++;
        }
        current_service = current_service->next;
        obj_idx++;
    }
}

uint8_t getTotalNumberMethods(void){

    uint8_t total = 0;
    PY_AJ_OBJECT_t *current_service = py_aj_obj_root;
    while (current_service != NULL) {
	PY_AJ_ITF_t *current_interface = current_service->interface;
	while (current_interface != NULL) {
            uint8_t num_methods = getNumMethods(current_interface->methods);
            total += num_methods;
            current_interface = current_interface->next;
        }
        current_service = current_service->next;
    }

    return total;
}

bool getMethodParameters(PY_AJ_METHOD_t *root, uint8_t depth,char **method, mp_obj_t *callback){

    uint8_t idx = 0;
    PY_AJ_METHOD_t *current_method = root;

    while (current_method != NULL){

        if (idx == depth){
            *method = current_method->method_name;
            *callback = current_method->callback;
            return true;
        }

        idx++;
        current_method = current_method->next;
    }

    return false;
}
bool getParametersFromMsgId(uint32_t msgId, char** method, mp_obj_t *callback, bool isService){

    PY_AJ_OBJECT_t *current_service = py_aj_obj_root;

    uint8_t obj_idx = 0;
    while (current_service != NULL) {

        uint8_t itf_idx = 0;
	PY_AJ_ITF_t *current_interface = current_service->interface;
	while (current_interface != NULL) {

            uint8_t num_methods = getNumMethods(current_interface->methods);

            for (int m_idx = 0; m_idx < num_methods; m_idx++){

                if (isService){
                    if (AJ_APP_MESSAGE_ID(obj_idx, itf_idx, m_idx) == msgId){
                        return getMethodParameters(current_interface->methods,m_idx, method, callback);
                    }
                } else {
                    uint32_t reply_id = AJ_REPLY_ID(AJ_PRX_MESSAGE_ID(obj_idx, itf_idx, m_idx));
                    if (reply_id == msgId){
                        return getMethodParameters(current_interface->methods,m_idx, method, callback);
                    }
                }

            }
            itf_idx++;
            current_interface = current_interface->next;
        }
        obj_idx++;
        current_service = current_service->next;
    }

    return false;
}

static void freeLinkedLists(void){

    PY_AJ_OBJECT_t *current_service = NULL;
    PY_AJ_OBJECT_t *service_head = py_aj_obj_root;

    while ((current_service = service_head) != NULL) {

        free(current_service->service_path);

        PY_AJ_ITF_t *current_interface = NULL;
	PY_AJ_ITF_t *interface_head = current_service->interface;
	while ((current_interface = interface_head) != NULL) {

            free(current_interface->interface_name);

            PY_AJ_METHOD_t *current_method = NULL;
            PY_AJ_METHOD_t *method_head = current_interface->methods;

            while ((current_method = method_head) != NULL){

                free(current_method->method_name);

                method_head = method_head->next;
                free(current_method);
            }

            interface_head = interface_head->next;
            free(current_interface);
        }
        service_head = service_head->next;
        free(current_service);
    }
}

void freeAjInterface(void)
{
    if (py_aj_obj_root == NULL){
        return;
    }

    uint8_t obj_idx = 0;
    while(alljoyn_obj[obj_idx].path != NULL){
        uint8_t itf_idx = 0;
        AJ_InterfaceDescription *interfaces = (AJ_InterfaceDescription *)alljoyn_obj[obj_idx].interfaces;
        while(interfaces[itf_idx] != NULL){
            free((char **)interfaces[itf_idx]);
            itf_idx++;
        }
        obj_idx++;
    }

    freeLinkedLists();
}

void addObject(char* new_service_path, char* new_interface_name, char *new_interface_method, mp_obj_t *new_callback) {

	if (py_aj_obj_root == NULL) {
		newObject(&py_aj_obj_root, new_service_path, new_interface_name, new_interface_method,new_callback);
	} else {
		PY_AJ_OBJECT_t *current = py_aj_obj_root;

		while (current != NULL) {

			if (!strcmp(current->service_path, new_service_path)) {
				// append to existing service path
				addInterfaceName(current, new_interface_name, new_interface_method, new_callback);
				return;
			}

			// service path not found
			if (current->next == NULL) {
				break;
			}

			current = current->next;
		}

		newObject(&(current->next), new_service_path, new_interface_name, new_interface_method, new_callback);
	}
}

bool getAJObjectIndex(const char *service_path, const char *inteface_name, const char *interface_method, uint8_t *indices){

    uint8_t aj_obj_idx = 0;
    PY_AJ_OBJECT_t *current = py_aj_obj_root;

    while (current != NULL) {

        if (!strcmp(current->service_path, service_path)){
            if (getInterfaceIndices(current->interface, inteface_name, interface_method, indices)) {
                indices[0] = aj_obj_idx;
                return true;
            }
        }

        aj_obj_idx++;
        current = current->next;
    }

    return false;
}

AJ_Object* getAlljoynObjects(void) {

	uint8_t num_AJ_Objects = getNumAjObjects(py_aj_obj_root);
	AJ_Object *AppObjects = (AJ_Object *)malloc((num_AJ_Objects + 1) * sizeof(AJ_Object));

	PY_AJ_OBJECT_t *current_service = py_aj_obj_root;

	uint8_t obj_idx = 0;
	while (current_service != NULL) {

		uint8_t num_iterfaces = getNumInterfaces(current_service->interface);
		AJ_InterfaceDescription *Interfaces = (AJ_InterfaceDescription *)malloc((num_iterfaces + 1) * sizeof(AJ_InterfaceDescription));

		uint8_t itf_idx = 0;
		PY_AJ_ITF_t *current_interface = current_service->interface;
		while (current_interface != NULL) {

                        uint8_t num_methods =  getNumMethods(current_interface->methods);
                        char **interface_methods = (char **)malloc((num_methods + 2) * sizeof(char *));

                        interface_methods[0] = current_interface->interface_name;
                        interface_methods[num_methods + 1] = NULL;

                        uint8_t mtd_idx = 1;
                        PY_AJ_METHOD_t *current_method = current_interface->methods;

                        while (current_method != NULL){

                            interface_methods[mtd_idx] = current_method->method_name;

                            mtd_idx++;
                            current_method = current_method->next;
                        }

			Interfaces[itf_idx] = (AJ_InterfaceDescription)interface_methods;
			current_interface = current_interface->next;

			itf_idx++;
		}

		Interfaces[itf_idx] = NULL; /* NULL terminated */

		AppObjects[obj_idx].path = current_service->service_path;
		AppObjects[obj_idx].interfaces = Interfaces;
                AppObjects[obj_idx].flags = 0;
                AppObjects[obj_idx].context = NULL;

		current_service = current_service->next;
		obj_idx++;
	}

	AppObjects[obj_idx].path = NULL;
        AppObjects[obj_idx].context = NULL;
        AppObjects[obj_idx].flags = 0;
        AppObjects[obj_idx].interfaces = NULL;

        alljoyn_obj = AppObjects;

	return AppObjects;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool getInterfaceIndices(PY_AJ_ITF_t *root, const char *interface_name, const char *method_name, uint8_t *indices){

    uint8_t itf_idx = 0;
    PY_AJ_ITF_t *current_interface = root;

    while (current_interface != NULL){

        if (!strcmp(current_interface->interface_name, interface_name)){

            uint8_t mtd_idx = 0;
            PY_AJ_METHOD_t *current_method = current_interface->methods;

             while (current_method != NULL){
                 if (!strcmp(current_method->method_name, method_name)){
                    indices[1] = itf_idx;
                    indices[2] = mtd_idx;
                    return true;
                }

                mtd_idx++;
                current_method = current_method->next;
             }
        }

        itf_idx++;
        current_interface = current_interface->next;
    }

    return false;
}

static uint8_t getNumAjObjects(PY_AJ_OBJECT_t * root) {

	uint8_t num_objects = 0;
	PY_AJ_OBJECT_t *current = root;

	while (current != NULL) {
		num_objects++;

		current = current->next;
	}

	return num_objects;
}

static uint8_t getNumInterfaces(PY_AJ_ITF_t *node) {

	uint8_t num_interfaces = 0;
	PY_AJ_ITF_t *current = node;

	while (current != NULL) {
		num_interfaces++;

		current = current->next;
	}

	return num_interfaces;
}

static uint8_t getNumMethods(PY_AJ_METHOD_t *root){

    uint8_t num_methods = 0;
    PY_AJ_METHOD_t *current = root;

    while (current != NULL){
        num_methods++;
        current = current->next;
    }

    return num_methods;
}

static void addMethod(PY_AJ_METHOD_t *node, char *new_method, mp_obj_t *new_callback) {

    PY_AJ_METHOD_t *current = node;

    while (current != NULL){

        if (!strcmp(current->method_name,new_method)){
            return;
        }

        if (current->next == NULL){
            break;
        }

        current = current->next;
    }

    newMethod(&(current->next),new_method, new_callback);

}

static void addInterfaceName(PY_AJ_OBJECT_t *node, char* new_interface_name, char *new_interface_method, mp_obj_t *new_callback) {

	PY_AJ_ITF_t *current = node->interface;

	while (current != NULL) {
		if (!strcmp(current->interface_name, new_interface_name)) {
			// append method to existing interface
			addMethod(current->methods, new_interface_method, new_callback);
			return;
		}

		if (current->next == NULL) {
			break;
		}

		// interface name not found
		current = current->next;
	}

	newInterface(&(current->next), new_interface_name);
        newMethod(&(current->next->methods),new_interface_method, new_callback);
}

static void newObject(PY_AJ_OBJECT_t **node, char* new_service_path, char* new_interface_name, char *new_interface_method, mp_obj_t *new_callback) {

	if (*node == NULL) {
		newServicePath(node, new_service_path);
		newInterface(&((*node)->interface), new_interface_name);
                newMethod(&((*node)->interface->methods), new_interface_method, new_callback);
        }
}

static void newServicePath(PY_AJ_OBJECT_t **node, char *new_service_path) {

	*node = (PY_AJ_OBJECT_t *)malloc(sizeof(PY_AJ_OBJECT_t));

	if (node != NULL) {
                (*node)->service_path = strdup(new_service_path);
		(*node)->next = NULL;
		(*node)->interface = NULL;
	}
}

static void newInterface(PY_AJ_ITF_t **node_itf, char *new_interface_name) {

	*node_itf = (PY_AJ_ITF_t *)malloc(sizeof(PY_AJ_ITF_t));

	if (*node_itf != NULL) {

            (*node_itf)->interface_name = strdup(new_interface_name);
            (*node_itf)->next = NULL;
            (*node_itf)->methods = NULL;
        }

}

static void newMethod(PY_AJ_METHOD_t **node_method, char *new_interface_method, mp_obj_t *new_callback) {

    *node_method = (PY_AJ_METHOD_t *)malloc(sizeof(PY_AJ_METHOD_t));

    if (*node_method != NULL){
        (*node_method)->method_name = strdup(new_interface_method);
        (*node_method)->callback = *new_callback;
        (*node_method)->next = NULL;
    }

}