//
// Created by Vincent.
//

#ifndef WIFI_EVENTS_H
#define WIFI_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"


#define WIFI_EVENTS_TAG "WIFI_EVENTS"

void connect_wifi();

void wifi_events_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#endif //WIFI_EVENTS_H