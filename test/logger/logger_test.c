//
// Created by Vincent, Javad.
//

#include "unity.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include <time.h>
#include <sys/time.h>
#include "../main/logger/logger.h"

void test_log_item(void) {
    // Test implementation for log_item function
    int amount_logs_before = get_log_lines();
    esp_err_t result = log_item("TEST", "TEST ITEM");
    TEST_ASSERT_EQUAL(ESP_OK, result);

    TEST_ASSERT((get_log_lines() == amount_logs_before + 1) || (amount_logs_before+1 > MAX_LOG_LINES && get_log_lines() == MAX_LOG_LINES));
}

void stress_test_log_item(void) {
    // Stress test implementation for log_item function
    clear_logs();
    int amount_logs_before = get_log_lines();
    esp_err_t result;
    for (int count = 0; count < 50; count++) {
        char item[200];
        snprintf(item, 200, "TEST ITEM %d", count);
        result = log_item("TEST", item);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        if(result == ESP_FAIL){
            break;
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, result);

    TEST_ASSERT((get_log_lines() == amount_logs_before + 50) || (amount_logs_before+50 > MAX_LOG_LINES && get_log_lines() == MAX_LOG_LINES));
}

void test_delete_logs(void) {

    clear_logs();

    log_item("TEST1", "TEST_ITEM1");
    log_item("TEST2", "TEST_ITEM2");
    log_item("TEST3", "TEST_ITEM3");
    log_item("TEST4", "TEST_ITEM4");
    log_item("TEST5", "TEST_ITEM5");

    TEST_ASSERT_EQUAL(ESP_OK, delete_logs(2, 4));

    TEST_ASSERT_EQUAL(2, get_log_lines());
}