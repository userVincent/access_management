//
// Created by Javad, Vincent.
//

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "../main/SQL_server/SQL_server.h"

void test_send_log_to_api()
{
    TEST_ASSERT_EQUAL(ESP_OK, send_log_to_api("Test", "2023-05-01T15:30:45", "Test message"));
}
