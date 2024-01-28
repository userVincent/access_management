//
// Created by Vincent.
//

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "cJSON.h"
#include "../service_message_handler.h"
#include "../../logger/logger.h"
#include "../../access/access.h"
#include <esp_https_server.h>
#include "esp_tls.h"
#include "https_server.h"


// Forward declarations for static functions/params
static esp_err_t get_data_handler(httpd_req_t *req);
static esp_err_t post_data_handler(httpd_req_t *req);
static const httpd_uri_t get_data = {
    .uri = "/get-data",
    .method = HTTP_GET,
    .handler = get_data_handler,
};
static const httpd_uri_t post_data = {
    .uri = "/post-data",
    .method = HTTP_POST,
    .handler = post_data_handler,
};
static httpd_handle_t start_webserver(void);
static esp_err_t stop_webserver(httpd_handle_t server);


// An HTTP GET handler 
static esp_err_t get_data_handler(httpd_req_t *req) {
    char query[100];
    char id[20];

    memset(query, 0, sizeof(query));
    memset(id, 0, sizeof(id));

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "id", id, sizeof(id)) == ESP_OK) {
            ESP_LOGI(HTTPS_SERVER_TAG, "Client ID: %s", id);
        } else {
            ESP_LOGI(HTTPS_SERVER_TAG, "Client ID not found");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Client ID not found");
        }
    } else {
        ESP_LOGI(HTTPS_SERVER_TAG, "Query string not found");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Query string not found");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"data\": \"Hello from ESP32!\"}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// An HTTP POST handler 
static esp_err_t post_data_handler(httpd_req_t *req) {
    char buf[POST_BUF_SIZE];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    // Null-terminate the received buffer
    buf[ret] = '\0';

    // Parse the JSON object
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        ESP_LOGE(HTTPS_SERVER_TAG, "Error parsing JSON");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    cJSON *id_json = cJSON_GetObjectItem(root, "ID");
    cJSON *message_type_json = cJSON_GetObjectItem(root, "message_type");

    if (id_json == NULL || message_type_json == NULL) {
        ESP_LOGE(HTTPS_SERVER_TAG, "Error: ID or message_type not found in JSON");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ID or message_type not found");
    }

    int userID = id_json->valueint;
    message_type_service message_type = message_type_json->valueint;

    // Validate userID
    if (!cJSON_IsNumber(id_json) || id_json->valueint <= 0) {
        ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid user ID");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid user ID");
    }

     // Validate message_type
    if (!cJSON_IsNumber(message_type_json) || message_type_json->valueint < 0) {
        ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid message type");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid message type");
    }

    switch (message_type)
    {
    case TURNON_SERVICE:
    case TURNOFF_SERVICE:{
            char log_message[LOGGER_QUEUE_ITEM_LEN];
            if(message_type == TURNON_SERVICE)
                snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "TURNON message received from ID: %d", userID);
            else
                snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "TURNOFF message received from ID: %d", userID);
            log_item(HTTPS_SERVER_TAG, log_message);
            cJSON *device_id_json = cJSON_GetObjectItem(root, "device_id");
            if (device_id_json == NULL) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: device_id not found in JSON");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "device_id not found");
            }
            uint8_t device_id = device_id_json->valueint;

            // Validate device_id
            if (!cJSON_IsNumber(device_id_json) || device_id_json->valueint < 0) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid device id");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device id");
            }

            turnon_turnoff_service_args_t message = {
                .device_id = device_id,
            };
            handle_service_message(message_type, &message);
        }
        break;
    
    case SYNC_SERVICE:{
            char log_message[LOGGER_QUEUE_ITEM_LEN];
            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "SYNC message received from ID: %d", userID);
            log_item(HTTPS_SERVER_TAG, log_message);
            handle_service_message(message_type, NULL);
        }
        break;

    case ACCESSLEVEL_SERVICE:{
            char log_message[LOGGER_QUEUE_ITEM_LEN];
            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "ACCESSLEVEL message received from ID: %d", userID);
            log_item(HTTPS_SERVER_TAG, log_message);
            cJSON *change_type_json = cJSON_GetObjectItem(root, "change_type");
            cJSON *target_type_json = cJSON_GetObjectItem(root, "target_type");
            cJSON *target_id_json = cJSON_GetObjectItem(root, target_type_json->valueint == 0 ? "device_id" : "key_id");
            cJSON *access_level_json = cJSON_GetObjectItem(root, "access_level");

            if (!change_type_json || !target_type_json || !target_id_json || !access_level_json) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: Missing fields in JSON");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing fields in JSON");
            }

            accesslevel_service_args_t args = {
                .change_type = change_type_json->valueint,
                .target_type = target_type_json->valueint,
                .new_access_level = access_level_json->valueint,
            };

            // Validate change_type
            if (!cJSON_IsNumber(change_type_json) || change_type_json->valueint < 0) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid change type");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid change type");
            }

            // Validate target_type
            if (!cJSON_IsNumber(target_type_json) || target_type_json->valueint < 0) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid target type");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid target type");
            }

            // Validate new_access_level
            if (!cJSON_IsNumber(access_level_json) || access_level_json->valueint < 1 || access_level_json->valueint >= 9) {
                ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid new access level");
                cJSON_Delete(root);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid new access level");
            }

            // Validate target_id depending on target_type
            if (args.target_type == 0) {
                if (!cJSON_IsNumber(target_id_json) || target_id_json->valueint < 0) {
                    ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid device_id");
                    cJSON_Delete(root);
                    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device_id");
                }
                args.target.device_id = target_id_json->valueint;
            } else {
                if (!cJSON_IsString(target_id_json) || strlen(target_id_json->valuestring) > KEY_LENGHT) {
                    ESP_LOGE(HTTPS_SERVER_TAG, "Error: Invalid key_id");
                    cJSON_Delete(root);
                    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid key_id");
                }
                strncpy(args.target.key_id, target_id_json->valuestring, 8);
                args.target.key_id[8] = '\0';
            }

            handle_service_message(message_type, &args);
        }
        break;

    default:
        ESP_LOGE(HTTPS_SERVER_TAG, "Error: message_type not found in JSON");
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "message_type not found");
        break;
    }

    cJSON_Delete(root);
    httpd_resp_sendstr(req, "Success");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(HTTPS_SERVER_TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(HTTPS_SERVER_TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(HTTPS_SERVER_TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &get_data);
    httpd_register_uri_handler(server, &post_data);
    return server;
}

static esp_err_t stop_webserver(httpd_handle_t server) {
    // Stop the httpd server
    return httpd_ssl_stop(server);
}

void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(HTTPS_SERVER_TAG, "Failed to stop https server");
        }
    }
}

void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        *server = start_webserver();
    }
}
