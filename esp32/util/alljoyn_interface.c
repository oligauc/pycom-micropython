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
typedef struct PY_AJ_ITF {
	char **interface_methods;
	uint8_t num_methods;
        mp_obj_t *callbacks;
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

static bool isUniqueMethod(PY_AJ_ITF_t *node, char *method);
static uint8_t getNumAjObjects(PY_AJ_OBJECT_t * root);
static uint8_t getNumInterfaces(PY_AJ_ITF_t *node);
static bool getInterfaceIndices(PY_AJ_ITF_t *node, const char *interface_name, const char *interface_methods, uint8_t *indices);
static uint8_t findUniqueMethods(PY_AJ_ITF_t *node, char **new_methods, uint8_t num_new, uint8_t *unique_index);
static void addMethods(PY_AJ_ITF_t *node, char **new_methods, uint8_t num_methods, mp_obj_t *new_callbacks);
static void addInterfaceName(PY_AJ_OBJECT_t *node, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks);
static void newInterface(PY_AJ_ITF_t **node, char *new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks);
static void newServicePath(PY_AJ_OBJECT_t **node, char *new_service_path);
static void newObject(PY_AJ_OBJECT_t **node, char* new_service_path, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks);

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
 static PY_AJ_OBJECT_t *py_aj_obj_root = NULL;
 
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
    PY_AJ_OBJECT_t *conductor = py_aj_obj_root;
    
    uint8_t obj_idx = 0;
    while (conductor != NULL) {
        
        uint8_t itf_idx = 0;
	PY_AJ_ITF_t *conductor_interface = conductor->interface;
	while (conductor_interface != NULL) {
            
            for (int m_idx = 1; m_idx <= conductor_interface->num_methods; m_idx++){
                if (isService){
                    msgIds[msgIdx++] = AJ_APP_MESSAGE_ID(obj_idx, itf_idx, m_idx - 1);
                } else {
                    msgIds[msgIdx++] = AJ_PRX_MESSAGE_ID(obj_idx, itf_idx, m_idx - 1);
                }
            }
            conductor_interface = conductor_interface->next;
            itf_idx++;
        }
        conductor = conductor->next;
        obj_idx++;
    }
}

uint8_t getTotalNumberMethods(void){
    
    uint8_t total = 0;
    PY_AJ_OBJECT_t *conductor = py_aj_obj_root;
    while (conductor != NULL) {
	PY_AJ_ITF_t *conductor_interface = conductor->interface;
	while (conductor_interface != NULL) {
            total += conductor_interface->num_methods;
            conductor_interface = conductor_interface->next;
        }
        conductor = conductor->next;
    }
    
    return total;
}

bool getParametersFromMsgId(uint32_t msgId, char** method, mp_obj_t *callback, bool isService){
    
    PY_AJ_OBJECT_t *conductor = py_aj_obj_root;
    
    uint8_t obj_idx = 0;
    while (conductor != NULL) {
        
        uint8_t itf_idx = 0;
	PY_AJ_ITF_t *conductor_interface = conductor->interface;
	while (conductor_interface != NULL) {
            
            for (int m_idx = 1; m_idx <= conductor_interface->num_methods; m_idx++){
             
                if (isService){
                    if (AJ_APP_MESSAGE_ID(obj_idx, itf_idx, m_idx - 1) == msgId){
                        *method = conductor_interface->interface_methods[m_idx];
                        *callback = conductor_interface->callbacks[m_idx];
                    }
                } else {
                    uint32_t reply_id = AJ_REPLY_ID(AJ_PRX_MESSAGE_ID(obj_idx, itf_idx, m_idx - 1));
                    if (reply_id == msgId){
                        *method = conductor_interface->interface_methods[m_idx];
                        *callback = conductor_interface->callbacks[m_idx];
                    }
                }
                    
            }
            itf_idx++;
            conductor_interface = conductor_interface->next;
        }
        obj_idx++;
        conductor = conductor->next;
    }
    
    return false;
}

void freeAjInterface(void)
{
    PY_AJ_OBJECT_t *current = NULL;
    PY_AJ_OBJECT_t *conductor = py_aj_obj_root;
    
    while ((current = conductor) != NULL) {
        
        free(current->service_path);
        
        PY_AJ_ITF_t *current_interface = NULL;
	PY_AJ_ITF_t *conductor_interface = conductor->interface;
	while ((current_interface = conductor_interface) != NULL) {
            
            for (int m_idx = 0; m_idx <= conductor_interface->num_methods + 1; m_idx++){
                free(conductor_interface->interface_methods[m_idx]);
            }
            
            free(conductor_interface->callbacks);
            free(conductor_interface->interface_methods);
            
            conductor_interface = conductor_interface->next;
            free(current_interface);
            
        }
        conductor = conductor->next;
        free(current);
    }
}

void addObject(char* new_service_path, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks) {

	if (py_aj_obj_root == NULL) {
		newObject(&py_aj_obj_root, new_service_path, new_interface_name, new_interface_methods, num_methods, new_callbacks);
	} else {
		PY_AJ_OBJECT_t *conductor = py_aj_obj_root;

		while (conductor != NULL) {

			if (!strcmp(conductor->service_path, new_service_path)) {
				// append to existing service path
				addInterfaceName(conductor, new_interface_name, new_interface_methods, num_methods, new_callbacks);
				return;
			}
			
			// service path not found
			if (conductor->next == NULL) {
				break;
			}
			
			conductor = conductor->next;
		}

		newObject(&(conductor->next), new_service_path, new_interface_name, new_interface_methods, num_methods, new_callbacks);
	}
}

bool getAJObjectIndex(const char *service_path, const char *inteface_name, const char *interface_method, uint8_t *indices){
    
    uint8_t aj_obj_idx = 0;
    PY_AJ_OBJECT_t *conductor = py_aj_obj_root;
    
    while (conductor != NULL) {
        
        if (!strcmp(conductor->service_path, service_path)){
            if (getInterfaceIndices(conductor->interface, inteface_name, interface_method, indices)) {
                indices[0] = aj_obj_idx;
                return true;
            }
        }
        
        aj_obj_idx++;
        conductor = conductor->next;
    }
 
    return false;
}

AJ_Object* getAlljoynObjects(void) {
	
	uint8_t num_AJ_Objects = getNumAjObjects(py_aj_obj_root);
	AJ_Object *AppObjects = (AJ_Object *)malloc((num_AJ_Objects + 1) * sizeof(AJ_Object));
	
	PY_AJ_OBJECT_t *conductor = py_aj_obj_root;

	uint8_t obj_idx = 0;
	while (conductor != NULL) {

		uint8_t num_iterfaces = getNumInterfaces(conductor->interface);
		AJ_InterfaceDescription *Interfaces = (AJ_InterfaceDescription *)malloc((num_iterfaces + 1) * sizeof(AJ_InterfaceDescription));
		
		uint8_t itf_idx = 0;
		PY_AJ_ITF_t *conductor_interface = conductor->interface;
		while (conductor_interface != NULL) {

			Interfaces[itf_idx] = (AJ_InterfaceDescription)conductor_interface->interface_methods;
			conductor_interface = conductor_interface->next;

			itf_idx++;
		}

		Interfaces[itf_idx] = NULL; /* NULL terminated */

		AppObjects[obj_idx].path = conductor->service_path;
		AppObjects[obj_idx].interfaces = Interfaces;
                AppObjects[obj_idx].flags = 0;
                AppObjects[obj_idx].context = NULL;

		conductor = conductor->next;
		obj_idx++;
	}

	AppObjects[obj_idx].path = NULL;
        AppObjects[obj_idx].context = NULL;
        AppObjects[obj_idx].flags = 0;
        AppObjects[obj_idx].interfaces = NULL;

	return AppObjects;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool getInterfaceIndices(PY_AJ_ITF_t *node, const char *interface_name, const char *interface_method, uint8_t *indices){
    
    uint8_t aj_itf_idx = 0;
    PY_AJ_ITF_t *conductor = node;
    
    while (conductor != NULL){
        
        if (!strcmp(conductor->interface_methods[0], interface_name)){
            for (uint8_t m_idx = 1; m_idx < conductor->num_methods + 1; m_idx++){
                if (!strcmp(conductor->interface_methods[m_idx], interface_method)){
                    indices[1] = aj_itf_idx;
                    indices[2] = m_idx - 1;
                    return true;
                }
            }
        }
        
        aj_itf_idx++;
        conductor = conductor->next;
    }
    
    return false;
}

static uint8_t getNumAjObjects(PY_AJ_OBJECT_t * root) {

	uint8_t num_objects = 0;
	PY_AJ_OBJECT_t *conductor = root;

	while (conductor != NULL) {
		num_objects++;
                
		conductor = conductor->next;
	}

	return num_objects;
}

static uint8_t getNumInterfaces(PY_AJ_ITF_t *node) {

	uint8_t num_interfaces = 0;
	PY_AJ_ITF_t *conductor = node;

	while (conductor != NULL) {
		num_interfaces++;

		conductor = conductor->next;
	}

	return num_interfaces;
}

static bool isUniqueMethod(PY_AJ_ITF_t *node, char *method) {

	bool unique = true;

	for (uint8_t idx = 1; idx < node->num_methods + 1; idx++) {
		if (!strcmp(node->interface_methods[idx], method)) {
			unique = false;
			break;
		}
	}

	return unique;
}

static uint8_t findUniqueMethods(PY_AJ_ITF_t *node, char **new_methods, uint8_t num_new, uint8_t *unique_index) {

	uint8_t num_unique = 0;

	for (uint8_t idx = 0; idx < num_new; idx++) {
		if (isUniqueMethod(node, new_methods[idx])) {
			unique_index[num_unique] = idx;
			num_unique++;
		}
	}
	
	return num_unique;
}

static void addMethods(PY_AJ_ITF_t *node, char **new_methods, uint8_t num_methods, mp_obj_t *new_callbacks) {

	uint8_t *unique_index = (uint8_t *)malloc(num_methods);
	uint8_t num_unique = findUniqueMethods(node, new_methods, num_methods, unique_index);

	uint8_t new_len = node->num_methods + num_unique;

	char **all_methods = (char **)malloc((new_len + 2) * sizeof(char *));
	memcpy(&(all_methods[0]), &(node->interface_methods[0]), (node->num_methods + 1) * sizeof(char *));
	free(node->interface_methods);
        
        mp_obj_t *all_callbacks = (mp_obj_t *)malloc((new_len + 2) * sizeof(mp_obj_t));
	memcpy(&(all_callbacks[0]), &(node->callbacks[0]), (node->num_methods + 1) * sizeof(mp_obj_t));
	free(node->callbacks);

	for (uint8_t idx = node->num_methods + 1, uidx = 0; idx < new_len + 1; idx++) {
		char *unique_method = new_methods[unique_index[uidx]];
                mp_obj_t *unique_callback = new_callbacks[unique_index[uidx]];
		size_t mLen = strlen(unique_method);

		all_methods[idx] = (char *)malloc(mLen + 1);
		memcpy(all_methods[idx], unique_method, mLen);
		all_methods[idx][mLen] = '\0';
                
                all_callbacks[idx] = unique_callback;
                
		uidx++;
	}

	node->interface_methods = all_methods;
        node->callbacks = all_callbacks;
	node->num_methods = new_len;

	all_methods[new_len + 1] = NULL;
        all_callbacks[new_len + 1] = MP_OBJ_NULL;

	free(unique_index);
}

static void addInterfaceName(PY_AJ_OBJECT_t *node, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks) {
	
	PY_AJ_ITF_t *conductor = node->interface;

	while (conductor != NULL) {
		if (!strcmp(conductor->interface_methods[0], new_interface_name)) {
			// append methods to existing interface name
			addMethods(conductor, new_interface_methods, num_methods, new_callbacks);
			return;
		}
		
		if (conductor->next == NULL) {
			break;
		}
			
		// interface name not found
		conductor = conductor->next;	
	}

	newInterface(&(conductor->next), new_interface_name, new_interface_methods, num_methods, new_callbacks);
}

static void newObject(PY_AJ_OBJECT_t **node, char* new_service_path, char* new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks) {
	
	if (*node == NULL) {
		newServicePath(node, new_service_path);
		newInterface(&((*node)->interface), new_interface_name, new_interface_methods, num_methods, new_callbacks);
	}
}

static void newServicePath(PY_AJ_OBJECT_t **node, char *new_service_path) {

	*node = (PY_AJ_OBJECT_t *)malloc(sizeof(PY_AJ_OBJECT_t));

	size_t len = strlen(new_service_path);

	if (node != NULL) {
		(*node)->service_path = (char *)malloc(len + 1);

		memcpy((*node)->service_path, new_service_path, len);
		(*node)->service_path[len] = '\0';

		(*node)->next = NULL;
		(*node)->interface = NULL;
	}
}

static void newInterface(PY_AJ_ITF_t **node_itf, char *new_interface_name, char **new_interface_methods, uint8_t num_methods, mp_obj_t *new_callbacks) {

	*node_itf = (PY_AJ_ITF_t *)malloc(sizeof(PY_AJ_ITF_t));
	(*node_itf)->interface_methods = (char **)malloc((num_methods + 2) * sizeof(char *)); /* Iterface at postion 0, methods, NULL termainated */
	(*node_itf)->callbacks = (mp_obj_t *)malloc((num_methods + 2) * sizeof(mp_obj_t));
        
	if ((*node_itf != NULL) && ((*node_itf)->interface_methods != NULL)) {

		/* new iterface name at position 0 */
		size_t mLen = strlen(new_interface_name);
		(*node_itf)->interface_methods[0] = (char *)malloc(mLen + 1);
		memcpy((*node_itf)->interface_methods[0], new_interface_name, mLen);
		(*node_itf)->interface_methods[0][mLen] = '\0';
                (*node_itf)->callbacks[0] = MP_OBJ_NULL;

		/* new methods */
		for (uint8_t i = 1; i < num_methods + 1; i++) {
			mLen = strlen(new_interface_methods[i-1]);
			(*node_itf)->interface_methods[i] = (char *)malloc(mLen + 1);
			
			memcpy((*node_itf)->interface_methods[i], new_interface_methods[i-1], mLen);
			(*node_itf)->interface_methods[i][mLen] = '\0';
                        
                        (*node_itf)->callbacks[i] = new_callbacks[i - 1];
		}

		(*node_itf)->num_methods = num_methods;
	}

	(*node_itf)->next = NULL;
	(*node_itf)->interface_methods[num_methods + 1] = NULL;
        (*node_itf)->callbacks[num_methods + 1] = MP_OBJ_NULL;
}