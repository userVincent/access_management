//
// Created by Vincent.
//

#ifndef SQL_SERVER_H
#define SQL_SERVER_H

#include "esp_err.h"
#include <stdbool.h>
#include "../logger/logger.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define SQL_SERVER_TAG "SQL_SERVER"
#define REST_API_URL "https://a22-access3.studev.groept.be/api.php"
#define MAX_HTTP_OUTPUT_BUFFER 50
#define BATCH_SIZE 50

esp_err_t init_SQL_server_mutex();

esp_err_t send_log_to_api(const char *TAG, const char *date_time, const char *info);

esp_err_t send_logs_to_api(const log_t *logs, size_t num_logs);

esp_err_t send_all_logs_to_api(bool delete_on_success);

#endif //SQL_SERVER_H