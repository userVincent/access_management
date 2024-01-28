//
// Created by Vincent.
//

#ifndef LOGGER_H
#define LOGGER_H

#include "esp_err.h"

#define LOGGER_TAG "LOGGER"

#define LOGGER_QUEUE_LEN 20
#define LOGGER_QUEUE_ITEM_LEN 200 // should be smaller then 255 for pretty_print_file_content() in spiffs.h to work correctly
#define LOGSFILENAME "/spiffs/logs.txt"
#define MAX_LOG_LINES 200

typedef struct log_t {
    char tag[20];
    char date_time[50];
    char info[220];
} log_t;

void logger_task(void *pvParameters);

esp_err_t init_logger_queue();

esp_err_t log_item(char *tag, char *info);

void midnight_task(void *pvParameters);

esp_err_t run_at_midnight();

void destruct_logger_task();

int get_log(int logline, char *buffer, size_t buffer_size);

esp_err_t parse_logs(int start_line, int end_line, log_t *logs, int max_logs);

int get_log_lines();

esp_err_t delete_logs(int start_log, int end_log);

esp_err_t clear_logs();

#endif //LOGGER_H