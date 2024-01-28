//
// Created by Vincent.
//

#include "nvs_flash.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/portmacro.h"
#include <esp_system.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <string.h>
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "esp_http_server.h"
#include "esp_wifi.h"

#include "services/service_message_handler.h"
#include "SQL_server/SQL_server.h"
#include "logger/sntp.h"
#include "mirf/mirf.h"
#include "nrf/nrf.h"
#include "spiffs/spiffs.h"
#include "logger/logger.h"
#include "access/access.h"
#include "nrf/nrf_message_handler.h"
#include "services/tcp_server/tcp_server.h"
#include "services/https_server/https_server.h"
#include "wifi_events/wifi_events.h"


#ifdef CONFIG_RUN_TESTS
#include "unity.h"
#include "../test/logger/logger_test.c"
#include "../test/access/test_access.c"
#include "../test/spiffs/spiffs_test.c"
#include "../test/SQL_server/SQL_server_test.c"
#endif

void app_main() {
    // Initialize NVS flash memory
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize networking interface
    ESP_ERROR_CHECK(esp_netif_init());

    // Create event loop with default settings
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // clear spiffs
    //ESP_ERROR_CHECK(clear_spiffs());

    /* Register event handlers to start server when Wi-Fi or Ethernet is connected,
     * and stop server when disconnection happens.
     */
    static httpd_handle_t server = NULL;
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    // Connect to Wi-Fi or Ethernet
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_events_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_events_handler, NULL));

    connect_wifi();

    // wait 10 seconds for wifi connection
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // initialize spiffs
    if(init_spiffs() == ESP_OK){
        // init access
        init_access();

        // init REST API connection
        init_SQL_server_mutex();

        // start logger
        xTaskCreate(logger_task, "logger_task", 1024*4, NULL, 2, NULL);

    }else{
        unregister_spiffs();
        ESP_LOGE("MAIN", "unable to start logger, unable to initialize access problem with spiffs");
    }

    // Create the nrf task
    vTaskDelay(5000 / portTICK_PERIOD_MS); // wait for proper initialization of spiffs, logger and access
    xTaskCreate(nrf_task, "nrf_task", 1024*4, NULL, 5, NULL);

    // Create the TCP server task
    vTaskDelay(1000 / portTICK_PERIOD_MS); // wait for proper initialization of nrf
    xTaskCreate(tcp_server_task, "tcp_server_task", 1024*8, NULL, 2, NULL);



#ifdef CONFIG_RUN_TESTS
    UNITY_BEGIN();

    // TEST CODE:
    vTaskDelay(30000 / portTICK_PERIOD_MS);
    ESP_LOGI("MAIN", "_________________RUNNING TEST CODE_________________\n");
    esp_log_level_set("*", ESP_LOG_NONE);

#ifdef CONFIG_TEST_LOGGER
    UNITY_BEGIN();
    // test logger
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("MAIN", "_________________RUNNING TEST LOGGER CODE_________________\n");
    esp_log_level_set("*", ESP_LOG_NONE);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    RUN_TEST(test_log_item);
    RUN_TEST(stress_test_log_item);
    RUN_TEST(test_delete_logs);

#endif

#ifdef CONFIG_TEST_ACCESS
    // test access
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("MAIN", "_________________RUNNING TEST ACCESS CODE_________________\n");
    esp_log_level_set("*", ESP_LOG_NONE);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    RUN_TEST(test_add_key);
    RUN_TEST(test_delete_key);
    RUN_TEST(test_change_key_access_level);
    RUN_TEST(test_change_device_access_level);
    RUN_TEST(test_has_access);
    RUN_TEST(test_delete_all_keys);
    RUN_TEST(test_delete_all_devices);

#endif

#ifdef CONFIG_TEST_SPIFFS
    // test nrf
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("MAIN", "_________________RUNNING TEST SPIFFS CODE_________________\n");
    esp_log_level_set("*", ESP_LOG_NONE);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    RUN_TEST(test_write_new_line);
    RUN_TEST(test_delete_first_line);
    RUN_TEST(test_read_line_from_file);
    RUN_TEST(test_delete_line_from_file);
    RUN_TEST(test_delete_lines_from_file);
    RUN_TEST(test_overwrite_line_in_file);
    RUN_TEST(test_delete_file_content);
    RUN_TEST(test_create_file_if_not_exists);
    RUN_TEST(test_delete_file_if_exists);

#endif

#ifdef CONFIG_TEST_SQL_SERVER
    // test nrf
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("MAIN", "_________________RUNNING TEST SQL SERVER CODE_________________\n");
    esp_log_level_set("*", ESP_LOG_NONE);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    RUN_TEST(test_send_log_to_api);

#endif


    esp_log_level_set("*", ESP_LOG_INFO);
    UNITY_END();
#endif

}	