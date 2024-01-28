//
// Created by Vincent.
//

/*
    message format:
    1 byte for type (0 for access, 1 for ping)
    2 bytes for device number
    13 bytes for data (dependent on type and device)
*/

#ifndef NRF_MESSAGE_HANDLER_H
#define NRF_MESSAGE_HANDLER_H

#include "esp_err.h"

#define NRF_MESSAGE_HANDLER_TAG "NRF_MESSAGE_HANDLER"

// enum for message data types
typedef enum{
    PING,
    TURNON,
    TURNOFF
} request_type;

typedef enum{
    ACCESS_TYPE,
    PING_TYPE
} nrf_message_type;

void nrf_message_handler_task(void *pvParameters);

esp_err_t send_nrf_message(int device, request_type type);

#endif //NRF_MESSAGE_HANDLER_H