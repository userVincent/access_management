//
// Created by Vincent.
//

#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "../nrf/nrf_message_handler.h"
#include "../access/access.h"
#include "../SQL_server/SQL_server.h"
#include "service_message_handler.h"


// Forward declarations for static functions
static esp_err_t handle_turnon_service(const turnon_turnoff_service_args_t *message);
static esp_err_t handle_turnoff_service(const turnon_turnoff_service_args_t *message);
static esp_err_t handle_sync_service(void);
static esp_err_t handle_accesslevel_service(accesslevel_service_args_t *message);
static esp_err_t handle_device_change_access_level(uint8_t device_id, uint8_t new_access_level);
static esp_err_t handle_device_add_access_level(uint8_t device_id, uint8_t new_access_level);
static esp_err_t handle_device_remove_access_level(uint8_t device_id, uint8_t new_access_level);
static esp_err_t handle_key_change_access_level(char *key_id, uint8_t new_access_level);
static esp_err_t handle_key_add_access_level(char *key_id, uint8_t new_access_level);
static esp_err_t handle_key_remove_access_level(char *key_id, uint8_t new_access_level);

esp_err_t handle_service_message(message_type_service type, void *message){
    esp_err_t ret = ESP_OK;

    switch (type) {
        case TURNON_SERVICE: {
            turnon_turnoff_service_args_t *args = (turnon_turnoff_service_args_t *)message;
            ret = handle_turnon_service(args);
            break;
        }

        case TURNOFF_SERVICE: {
            turnon_turnoff_service_args_t *args = (turnon_turnoff_service_args_t *)message;
            ret = handle_turnoff_service(args);
            break;
        }

        case SYNC_SERVICE: {
            ret = handle_sync_service();
            break;
        }

        case ACCESSLEVEL_SERVICE: {
            accesslevel_service_args_t *args = (accesslevel_service_args_t *)message;
            ret = handle_accesslevel_service(args);
            break;
        }

        default: {
            ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Invalid service message type");
            ret = ESP_FAIL;
            break;
        }
    }

    return ret;
}

static esp_err_t handle_turnon_service(const turnon_turnoff_service_args_t *message) {
    // Handle TURNOFF_SERVICE
    if(message->device_id == 0){
        int devices[MAX_DEVICES];
        int access_levels[MAX_DEVICES];
        int amount_devices = get_devices(devices, access_levels);
        if(amount_devices == -1){
            ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to get devices");
            return ESP_FAIL;
        }
        for(int i = 0; i < amount_devices; i++){
            if(send_nrf_message(devices[i], TURNON) != ESP_OK){
                ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to send message to device %d", devices[i]);
            }
        }
    }else{
        if(send_nrf_message(message->device_id, TURNON) != ESP_OK){
            ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to send message to device %d", message->device_id);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t handle_turnoff_service(const turnon_turnoff_service_args_t *message) {
    // Handle TURNOFF_SERVICE
    if(message->device_id == 0){
        int devices[MAX_DEVICES];
        int access_levels[MAX_DEVICES];
        int amount_devices = get_devices(devices, access_levels);
        if(amount_devices == -1){
            ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to get devices");
            return ESP_FAIL;
        }
        for(int i = 0; i < amount_devices; i++){
            if(send_nrf_message(devices[i], TURNOFF) != ESP_OK){
                ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to send message to device %d", devices[i]);
            }
        }
    }else{
        if(send_nrf_message(message->device_id, TURNOFF) != ESP_OK){
            ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to send message to device %d", message->device_id);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t handle_sync_service(void) {
    // Handle SYNC_SERVICE
    if(send_all_logs_to_api(true) != ESP_OK){
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to SYNC");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_accesslevel_service(accesslevel_service_args_t *message) {
    // Handle ACCESSLEVEL_SERVICE
    esp_err_t ret = ESP_OK;
    if (message->target_type == 0) {
        ESP_LOGI(SERVICE_MESSAGE_HANDLER_LOG_TAG, "ACCESSLEVEL: Change Type: %d, Target Type: %d, Device ID: %d, New Access Level: %d",
                 message->change_type, message->target_type, message->target.device_id, message->new_access_level);
        switch (message->change_type) {
            case 0:
                ret = handle_device_change_access_level(message->target.device_id, message->new_access_level);
                break;
            case 1:
                ret = handle_device_add_access_level(message->target.device_id, message->new_access_level);
                break;
            case 2:
                ret = handle_device_remove_access_level(message->target.device_id, message->new_access_level);
                break;
            default:
                ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Invalid change type");
                ret = ESP_FAIL;
                break;
        }
    } else if (message->target_type == 1) {
        ESP_LOGI(SERVICE_MESSAGE_HANDLER_LOG_TAG, "ACCESSLEVEL: Change Type: %d, Target Type: %d, Key ID: %s, New Access Level: %d",
                message->change_type, message->target_type, message->target.key_id, message->new_access_level);
        switch (message->change_type) {
            case 0:
                ret = handle_key_change_access_level(message->target.key_id, message->new_access_level);
                break;
            case 1:
                ret = handle_key_add_access_level(message->target.key_id, message->new_access_level);
                break;
            case 2:
                ret = handle_key_remove_access_level(message->target.key_id, message->new_access_level);
                break;
            default:
                ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Invalid change type");
                ret = ESP_FAIL;
                break;
            }
    } else {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Invalid target type");
        ret = ESP_FAIL;
    }
    return ret;
}

static esp_err_t handle_device_change_access_level(uint8_t device_id, uint8_t new_access_level) {
    if (change_device_access_level(device_id, new_access_level) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to change access level of device %d", device_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_device_add_access_level(uint8_t device_id, uint8_t new_access_level) {
    if (add_device(device_id, new_access_level) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to add access level of device %d", device_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_device_remove_access_level(uint8_t device_id, uint8_t new_access_level) {
    if (delete_device(device_id) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to remove access level of device %d", device_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_key_change_access_level(char *key_id, uint8_t new_access_level) {
    if (change_key_access_level(key_id, new_access_level) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to change access level of key %s", key_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_key_add_access_level(char *key_id, uint8_t new_access_level) {
    if (add_key(key_id, new_access_level) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to add access level of key %s", key_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t handle_key_remove_access_level(char *key_id, uint8_t new_access_level) {
    if (delete_key(key_id) != ESP_OK) {
        ESP_LOGE(SERVICE_MESSAGE_HANDLER_LOG_TAG, "Failed to remove access level of key %s", key_id);
        return ESP_FAIL;
    }
    return ESP_OK;
}