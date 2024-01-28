//
// Created by Vincent.
//

#ifndef SERVICE_MESSAGE_HANDLER_H
#define SERVICE_MESSAGE_HANDLER_H

#include "esp_err.h"

#define SERVICE_MESSAGE_HANDLER_LOG_TAG "service_message_handler"

/*
* TURNON: needs one extra parameter, the device id (0 incase of all devices)(1-99)
* TURNOFF: needs one extra parameter, the device id (0 incase of all devices)(1-99)
* SYNC: needs no extra parameters (will push local logs to server)
* ACCESSLEVEL: needs four extra parameters:
*        1. wether you want to change(0) add(1) or delete(2) a device/key 
*        2. wether you want to adjust a device(0) or a key(1)
*        3. the device(1-99) id or the key id(8byte hex)
*        4. the new access level(0-9)
*/

typedef enum{TURNON_SERVICE, TURNOFF_SERVICE, SYNC_SERVICE, ACCESSLEVEL_SERVICE} message_type_service;

typedef struct {
    uint8_t device_id;
} turnon_turnoff_service_args_t;

typedef struct {
    uint8_t change_type;
    uint8_t target_type;
    union {
        uint8_t device_id;
        char key_id[17];
    } target;
    uint8_t new_access_level;
} accesslevel_service_args_t;


esp_err_t handle_service_message(message_type_service type, void *message);

#endif //SERVICE_MESSAGE_HANDLER_H