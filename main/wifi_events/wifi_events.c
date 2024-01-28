//
// Created by Vincent.
//


#include "wifi_events.h" 
#include "esp_log.h"  
#include "esp_wifi.h"  
#include "esp_event.h"  
#include "freertos/FreeRTOS.h"  
#include "freertos/timers.h"
#include "esp_wifi.h"  
#include "protocol_examples_common.h"
#include "../logger/sntp.h"
#include "wifi_events.h"


static TimerHandle_t wifi_reconnect_timer = NULL;

static void wifi_reconnect_timer_cb(TimerHandle_t xTimer);

void connect_wifi() {
    // Only create the timer if it hasn't been created yet
    if (wifi_reconnect_timer == NULL) {
        wifi_reconnect_timer = xTimerCreate("wifiReconnectTimer", pdMS_TO_TICKS(3600000), 
                                            pdFALSE, (void *) 0, wifi_reconnect_timer_cb);
        if (wifi_reconnect_timer == NULL) {
            ESP_LOGE(WIFI_EVENTS_TAG, "Failed to create wifi reconnect timer");
            return;
        }
    }

    if(example_connect() != ESP_OK){
        ESP_LOGE(WIFI_EVENTS_TAG, "Failed to reconnect to WiFi, starting timer to retry in 1 hour");
        // Check if timer is already running, if so stop it
        if (xTimerIsTimerActive(wifi_reconnect_timer) != pdFALSE) {
            xTimerStop(wifi_reconnect_timer, 0);
        }
        // Start the timer
        if (xTimerStart(wifi_reconnect_timer, 0) != pdPASS) {
            ESP_LOGE(WIFI_EVENTS_TAG, "Failed to start wifi reconnect timer");
        }
    } else {
        // If connection is successful and timer is running, stop it
        if (xTimerIsTimerActive(wifi_reconnect_timer) != pdFALSE) {
            xTimerStop(wifi_reconnect_timer, 0);
        }
    }
}

static void wifi_reconnect_timer_cb(TimerHandle_t xTimer) {
    if(example_connect() != ESP_OK){
        ESP_LOGE(WIFI_EVENTS_TAG, "Failed to reconnect to WiFi, starting timer to retry in 1 hour");
        if (xTimerStart(wifi_reconnect_timer, 0) != pdPASS) {
            ESP_LOGE(WIFI_EVENTS_TAG, "Failed to start wifi reconnect timer");
        }
    }
}

// Event handler function for wifi connects/disconnects
void wifi_events_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // A WiFi connection has been established, so we try to set the time using SNTP
        //set_time_using_sntp();
    } else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // A WiFi connection has been lost, so we try to reconnect
        connect_wifi();
    }
}