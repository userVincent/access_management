//
// Created by Vincent.
//

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "new_cert.h"
#include "cJSON.h"
#include "../logger/logger.h"
#include "SQL_server.h"

static SemaphoreHandle_t SQL_server_mutex;
static void url_encode(char *dst, size_t dst_size, const char *src);

esp_err_t init_SQL_server_mutex() {
    SQL_server_mutex = xSemaphoreCreateMutex();
    if (SQL_server_mutex == NULL) {
        ESP_LOGE(SQL_SERVER_TAG, "FAILED TO CREATE SQL_server_mutex");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t send_log_to_api(const char *TAG, const char *date_time, const char *info) {

    char encoded_date_time[50];
    char encoded_info[200];
    
    url_encode(encoded_date_time, sizeof(encoded_date_time), date_time);
    url_encode(encoded_info, sizeof(encoded_info), info);

    char url[350];  // Increased buffer size
    snprintf(url, sizeof(url), "%s?TAG=%s&date_time=%s&info=%s", REST_API_URL, TAG, encoded_date_time, encoded_info);

    esp_http_client_config_t config = {
        .url = url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)new_cert_pem,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(SQL_SERVER_TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(SQL_SERVER_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_fetch_headers(client);
    if (status_code < 0) {
        ESP_LOGE(SQL_SERVER_TAG, "Failed to fetch HTTP headers: %s", esp_err_to_name(status_code));
        esp_http_client_cleanup(client);
        return status_code;
    }
    char response_buffer[MAX_HTTP_OUTPUT_BUFFER];

    int response_length = esp_http_client_read(client, response_buffer, MAX_HTTP_OUTPUT_BUFFER - 1);
    if (response_length < 0) {
        ESP_LOGE(SQL_SERVER_TAG, "Failed to read HTTP response: %s", esp_err_to_name(response_length));
    } else if (response_length == 0) {
        ESP_LOGE(SQL_SERVER_TAG, "No data in HTTP response");
    } else {
        response_buffer[response_length] = '\0';
        ESP_LOGI(SQL_SERVER_TAG, "HTTP Response: %.*s", response_length, response_buffer);
    }

    //free(response_buffer);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return ESP_OK;
}

esp_err_t send_logs_to_api(const log_t *logs, size_t num_logs) {
    cJSON *logs_array = cJSON_CreateArray();
    for (size_t i = 0; i < num_logs; i++) {
        cJSON *log_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(log_obj, "TAG", logs[i].tag);
        cJSON_AddStringToObject(log_obj, "date_time", logs[i].date_time);
        cJSON_AddStringToObject(log_obj, "info", logs[i].info);
        cJSON_AddItemToArray(logs_array, log_obj);
    }
    char *json_payload = cJSON_PrintUnformatted(logs_array);

    esp_http_client_config_t config = {
        .url = REST_API_URL,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .cert_pem = (const char *)new_cert_pem,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(SQL_SERVER_TAG, "Failed to initialize HTTP client");
        cJSON_Delete(logs_array);
        free(json_payload);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(SQL_SERVER_TAG, "HTTP request failed: %s", esp_err_to_name(err));
        cJSON_Delete(logs_array);
        free(json_payload);
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);

    cJSON_Delete(logs_array);
    free(json_payload);
    esp_http_client_cleanup(client);

    if (status_code == 200) {
        ESP_LOGI(SQL_SERVER_TAG, "Log(s) sent successfully to REST API");
        return ESP_OK;
    } else {
        ESP_LOGE(SQL_SERVER_TAG, "Error sending log(s): HTTP status code %d", status_code);
        return ESP_FAIL;
    }
}

void url_encode(char *dst, size_t dst_size, const char *src) {
    size_t i = 0, j = 0;
    while (src[i] != '\0' && j + 4 < dst_size) {
        if (isalnum((unsigned char)src[i]) || src[i] == '-' || src[i] == '_' || src[i] == '.' || src[i] == '~') {
            dst[j++] = src[i++];
        } else {
            snprintf(dst + j, dst_size - j, "%%%02X", (unsigned char)src[i++]);
            j += 3;
        }
    }
    dst[j] = '\0';
}

esp_err_t send_all_logs_to_api(bool delete_on_success) {
    // lock mutex
    if(SQL_server_mutex == NULL){
        ESP_LOGE(LOGGER_TAG, "failed to send_all_logs, SQL_server_mutex not active");
        return ESP_FAIL;
    }
    xSemaphoreTake(SQL_server_mutex, portMAX_DELAY);

    int log_lines = get_log_lines(); // get the total number of log lines
    int start_line = 1;
    int total_batches = 0;
    int successful_batches = 0;

    while (start_line <= log_lines) {
        int free_heap_size = esp_get_free_heap_size();
        int max_logs_allocation_size = free_heap_size / 4;
        int max_logs_in_batch = max_logs_allocation_size / sizeof(log_t);

        int end_line = start_line + max_logs_in_batch - 1;
        if (end_line > log_lines) {
            end_line = log_lines;
        }

        log_t *logs = (log_t *)malloc(max_logs_in_batch * sizeof(log_t));
        ESP_LOGI(SQL_SERVER_TAG, "allocated batch: %d, size: %d", total_batches, end_line);
        if (logs == NULL) {
            ESP_LOGE(SQL_SERVER_TAG, "Failed to allocate memory for logs");
            return ESP_FAIL;
        }

        esp_err_t parse_result = parse_logs(start_line, end_line, logs, max_logs_in_batch);
        if (parse_result == ESP_OK) {
            int logs_to_send = end_line - start_line + 1;
            esp_err_t send_result = send_logs_to_api(logs, logs_to_send);
            if (send_result == ESP_OK) {
                successful_batches++;
                if (delete_on_success) {
                    delete_logs(start_line, end_line);
                    log_lines -= logs_to_send; // Update log_lines after deleting logs
                    end_line = start_line - 1; // Adjust end_line for next batch calculation
                }
            }
        }

        free(logs);
        total_batches++;
        start_line = end_line + 1;
    }

    ESP_LOGI(SQL_SERVER_TAG, "Total batches: %d, Successful batches: %d", total_batches, successful_batches);

    // unlock mutex
    xSemaphoreGive(SQL_server_mutex);

    if (successful_batches == total_batches) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}
