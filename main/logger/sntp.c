//
// Created by Vincent.
//

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/apps/sntp.h"
#include "sntp.h"

static bool sntp_initialized = false;

static void initialize_sntp(void) {
    ESP_LOGI(SNTP_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    sntp_initialized = true;
}

esp_err_t set_time_using_sntp(void) {
    if (!sntp_initialized) {
        initialize_sntp();
    }

    // Wait for the system time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(SNTP_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry < retry_count) {
        // Set the timezone information for Belgium
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // indicating the transition to summer time on the last Sunday of March (M3.5.0) and the transition back to winter time on the last Sunday of October (M10.5.0/3)
        tzset();

        // Update the timeinfo structure with the new time
        time(&now);
        localtime_r(&now, &timeinfo);

        ESP_LOGI(SNTP_TAG, "System time set using SNTP");
        return ESP_OK;
    } else {
        ESP_LOGE(SNTP_TAG, "Failed to set system time using SNTP");
        return ESP_FAIL;
    }
}