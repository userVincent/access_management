//
// Created by Vincent.
//

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sntp.c"
#include "../spiffs/spiffs.h"
#include "../SQL_server/SQL_server.h"
#include "logger.h"


// Forward declarations for static functions/params
static int LOG_LINES;
static SemaphoreHandle_t logger_mutex;
static QueueHandle_t logger_queue;


void logger_task(void *pvParameters){
    // init mutex
    logger_mutex = xSemaphoreCreateMutex();
    if(logger_mutex == NULL){
        ESP_LOGE(LOGGER_TAG, "FAILED TO CREATE logger_mutex");
        destruct_logger_task();
    }

    // init queue
    if(init_logger_queue() != ESP_OK){
        destruct_logger_task();
    }

    // init log file
    if(create_file_if_not_exists(LOGSFILENAME) == -1){
        destruct_logger_task();
    };
    LOG_LINES = count_lines(LOGSFILENAME);

    // init sntp
    set_time_using_sntp();

    // create midnight task
    xTaskCreate(midnight_task, "midnight_task", 1024*4, NULL, 1, NULL);

    // receive messages on queue
	const char queue_item[LOGGER_QUEUE_ITEM_LEN];
	while(1){
		if (xQueueReceive(logger_queue, (void *)&queue_item, portMAX_DELAY) == pdTRUE) {
            // lock mutex
            xSemaphoreTake(logger_mutex, portMAX_DELAY);
            
			// handle message from logger_queue
            while(MAX_LOG_LINES <= LOG_LINES){
                delete_first_line(LOGSFILENAME);
                --LOG_LINES;
                ESP_LOGW(LOGGER_TAG, "logs file is full, oldest log deleted to make space");
            }
            if(write_new_line(LOGSFILENAME, (const char *)&queue_item) == 0){
                ++LOG_LINES;
                ESP_LOGI(LOGGER_TAG, "succesfully logged '%s' to logs file", queue_item);
            }else{
                ESP_LOGE(LOGGER_TAG, "failed to log '%s' to logs file", queue_item);
            }

            // unlock mutex
            xSemaphoreGive(logger_mutex);
        }
	}
    destruct_logger_task();
}

esp_err_t init_logger_queue(){
	// initialize queue
    logger_queue = xQueueCreate(LOGGER_QUEUE_LEN, LOGGER_QUEUE_ITEM_LEN);

	if (logger_queue == NULL) {
        ESP_LOGE(LOGGER_TAG, "FAILED TO CREATE logger_queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t log_item(char *tag, char *info){
    if(logger_queue == NULL){
        ESP_LOGE(LOGGER_TAG, "logging not possible, logger_queue not active");
        return ESP_FAIL;
    }

    char log_message[LOGGER_QUEUE_ITEM_LEN];

    time_t now;
    struct tm timeinfo;
    char time_buffer[32];

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    if(timeinfo.tm_mon == 3 && timeinfo.tm_mday == 1){
        tag = "APRILFOOLS";
        info = "OH NO SEEMS LIKE ALIENS STOLE OUR LOGS";
    }

    // Calculate the length of the resulting string
    size_t result_len = strlen(tag) + 1 + strlen(time_buffer) + 1 + strlen(info) + 1;

    if (result_len > LOGGER_QUEUE_ITEM_LEN) {
        ESP_LOGE(LOGGER_TAG, "log_item message is to long (%d > %d)", result_len, LOGGER_QUEUE_ITEM_LEN);
        return ESP_FAIL;
    }

    // Concatenate the strings
    snprintf((char *)&log_message, result_len, "%s,%s,%s", tag, time_buffer, info);

    // Fill the remaining space with null characters
    memset(&log_message + result_len - 1, '\0', LOGGER_QUEUE_ITEM_LEN - result_len + 1);

    // put log_message on logger_queue
    if (xQueueSend(logger_queue, (void *)(&log_message), 0) != pdTRUE) {
        ESP_LOGE(LOGGER_TAG, "logger_queue full");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void midnight_task(void *pvParameters) {
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // Calculate time until midnight
        int seconds_until_midnight = (23 - timeinfo.tm_hour) * 3600 + (59 - timeinfo.tm_min) * 60 + (60 - timeinfo.tm_sec);
        ESP_LOGI(LOGGER_TAG, "midnight_task set, %d seconds until midnight", seconds_until_midnight);

        // Wait until midnight
        vTaskDelay(seconds_until_midnight * 1000 / portTICK_PERIOD_MS);

        // Run the function
        run_at_midnight();

        // Sleep for 2 minutes to avoid running the task again if there are slight time drifts
        vTaskDelay(120 * 1000 / portTICK_PERIOD_MS);

        // Reset time to reduce drift
        set_time_using_sntp();
    }
}

esp_err_t run_at_midnight() {
    ESP_LOGI(LOGGER_TAG, "MIDNIGHT TIMER GOING OFF");
    // push daily logs to mysql server and delete logs that where succesfully pushed
    if(send_all_logs_to_api(true) == ESP_OK){
        ESP_LOGI(LOGGER_TAG, "succesfully pushed all logs to api");
        return ESP_OK;
    }else{
        ESP_LOGE(LOGGER_TAG, "failed to push all logs to api");
        return ESP_FAIL;
    }
}

void destruct_logger_task(){
    if(logger_mutex != NULL){
        vSemaphoreDelete(logger_mutex);
        logger_mutex = NULL;
    }
    if(logger_queue != NULL){
        vQueueDelete(logger_queue);
        logger_queue = NULL;
    }
	ESP_LOGE(LOGGER_TAG, "destoying logger task");
	vTaskDelete(NULL);
}

int get_log(int logline, char *buffer, size_t buffer_size){
    // lock mutex
    if(logger_mutex == NULL){
        ESP_LOGE(LOGGER_TAG, "failed to get log, logger_mutex not active");
        return ESP_FAIL;
    }
    xSemaphoreTake(logger_mutex, portMAX_DELAY);

    if(logline <= LOG_LINES){
        read_line_from_file(LOGSFILENAME, logline, buffer, buffer_size);

        int count = 0;
        while (buffer[count] != '\0' && buffer[count] != '\n') {
            count++;
        }
        // unlock mutex
        xSemaphoreGive(logger_mutex);
        
        return count;
    }
    ESP_LOGE(LOGGER_TAG, "failed to get log");

    // unlock mutex
    xSemaphoreGive(logger_mutex);
    return ESP_FAIL;
}

esp_err_t parse_logs(int start_line, int end_line, log_t *logs, int max_logs) {

    int logs_stored = 0;

    for (int i = start_line; i <= end_line && logs_stored < max_logs; i++) {
        char buffer[300];

        int read_count = get_log(i, buffer, sizeof(buffer));
        if (read_count == -1) {
            return ESP_FAIL;
        }

        // Parse the line as CSV
        char *token = strtok(buffer, ",");
        if (token == NULL) {
            continue;
        }
        strncpy(logs[logs_stored].tag, token, sizeof(logs[logs_stored].tag) - 1);
        logs[logs_stored].tag[sizeof(logs[logs_stored].tag) - 1] = '\0';

        token = strtok(NULL, ",");
        if (token == NULL) {
            continue;
        }
        strncpy(logs[logs_stored].date_time, token, sizeof(logs[logs_stored].date_time) - 1);
        logs[logs_stored].date_time[sizeof(logs[logs_stored].date_time) - 1] = '\0';

        token = strtok(NULL, ",");
        if (token == NULL) {
            continue;
        }
        strncpy(logs[logs_stored].info, token, sizeof(logs[logs_stored].info) - 1);
        logs[logs_stored].info[sizeof(logs[logs_stored].info) - 1] = '\0';

        logs_stored++;
    }

    return ESP_OK;
}

int get_log_lines(){
    // lock mutex
    xSemaphoreTake(logger_mutex, portMAX_DELAY);

    int ret = LOG_LINES;

    // unlock mutex
    xSemaphoreGive(logger_mutex);
    return ret;
}

esp_err_t delete_logs(int start_log, int end_log) {
    if (start_log > end_log) {
        ESP_LOGE(LOGGER_TAG, "Invalid log range: start_log should be less than or equal to end_log");
        return ESP_FAIL;
    }

    // lock mutex
    if (logger_mutex == NULL) {
        ESP_LOGE(LOGGER_TAG, "failed to delete logs, logger_mutex not active");
        return ESP_FAIL;
    }
    xSemaphoreTake(logger_mutex, portMAX_DELAY);

    // Check if end_log is larger than LOG_LINES
    if (end_log > LOG_LINES) {
        ESP_LOGE(LOGGER_TAG, "Invalid log range: end_log cannot be larger than LOG_LINES");
        xSemaphoreGive(logger_mutex);
        return ESP_FAIL;
    }

    esp_err_t ret = delete_lines_from_file(LOGSFILENAME, start_log, end_log);
    if (ret == ESP_OK) {
        LOG_LINES -= (end_log - start_log + 1);
        ESP_LOGI(LOGGER_TAG, "Successfully deleted logs %d to %d", start_log, end_log);
    } else {
        ESP_LOGE(LOGGER_TAG, "Failed to delete logs %d to %d", start_log, end_log);
    }

    // unlock mutex
    xSemaphoreGive(logger_mutex);
    return ret;
}


esp_err_t clear_logs(){
    // lock mutex
    if(logger_mutex == NULL){
        ESP_LOGE(LOGGER_TAG, "failed to clear logs, logger_mutex not active");
        return ESP_FAIL;
    }
    xSemaphoreTake(logger_mutex, portMAX_DELAY);

    esp_err_t ret = delete_file_content(LOGSFILENAME);
    if(ret == ESP_OK){
        LOG_LINES = 0;
        ESP_LOGI(LOGGER_TAG, "succesfully cleared all logs");
    }else{
        ESP_LOGW(LOGGER_TAG, "failed to clear all logs");
    }

    // unlock mutex
    xSemaphoreGive(logger_mutex);
    return ret;
}

void test_logger(){
    delete_file_content(LOGSFILENAME);
    LOG_LINES = 0;

    for (int log = 1; log < 203; ++log){
        char info[50];
        // Concatenate the strings
        snprintf((char *)&info, 50, "%s%d", "TESTING LOGGER ", log);

        log_item("TEST_LOGGER", (char *) &info);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    pretty_print_file_content(LOGSFILENAME);
}