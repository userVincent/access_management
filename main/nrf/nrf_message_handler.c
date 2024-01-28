//
// Created by Vincent.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "../mirf/mirf.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nrf.h"
#include "../logger/logger.h"
#include "../access/access.h"
#include "nrf_message_handler.h"

void nrf_message_handler_task(void *pvParameters){
    ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "nrf_message_handler_task ready");
    // wait for item on queue
    char queue_item[NRF_DATA_LEN];
	while(1){
		if (xQueueReceive(nrf_message_handler_queue, (void *)&queue_item, portMAX_DELAY) == pdTRUE) {
			// handle message from nrf_message_handler_queue
			nrf_message_type type = queue_item[0] - '0';
            int device = (queue_item[1] - '0') * 10 + (queue_item[2] - '0');
            char log_message[LOGGER_QUEUE_ITEM_LEN];
            switch (type){
                case ACCESS_TYPE:
                
                    char ibutton[17];
                    const char* hex_chars = "0123456789ABCDEF";
                    for (int i = 0; i < 8; ++i) {
                        ibutton[i * 2] = hex_chars[queue_item[i+3] >> 4];
                        ibutton[i * 2 + 1] = hex_chars[queue_item[i+3] & 0x0F];
                    }
                    ibutton[16] = 0;

                    if(queue_item[NRF_DATA_LEN - 1] == 'X'){
                        // handling of ACK message (for logging purposes)
                        if(queue_item[3] == '1'){
                            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "sending TURNON to device: %d", device);
                            log_item(NRF_MESSAGE_HANDLER_TAG, (char *)log_message);
                        }else if(queue_item[3] == '0'){
                            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "sending TURNOFF to device: %d", device);
                            log_item(NRF_MESSAGE_HANDLER_TAG, (char *)log_message);
                        }
                    }else{
                        // handling of actual access message
                        if (has_access(device, ibutton) == 0){
                            ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "received ACCESS from device: %d, ibutton: %s, access granted", device, ibutton);

                            // send turn on message
                            send_nrf_message(device, TURNON);
                        } else {
                            ESP_LOGW(NRF_MESSAGE_HANDLER_TAG, "received ACCESS from device: %d, ibutton: %s, access denied", device, ibutton);
                    
                            // send turn off message
                            send_nrf_message(device, TURNOFF);
                        }
                    }
                    break;
                case PING_TYPE:
                    int error_code = queue_item[3] - '0';
                    if (error_code == 1){
                        ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "received PING from device: %d, successful", device);
                        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "received PING from device: %d, successful", device);
                        log_item(NRF_MESSAGE_HANDLER_TAG, (char *)log_message);
                    } else {
                        ESP_LOGW(NRF_MESSAGE_HANDLER_TAG, "received PING from device: %d, error code: %d", device, error_code);
                        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "received PING from device: %d, error code: %d", device, error_code);
                        log_item(NRF_MESSAGE_HANDLER_TAG, (char *)log_message);
                    }
                    break;
                default:
                    //ESP_LOGW(NRF_MESSAGE_HANDLER_TAG, "nrf_message_handler_queue invalid message, type not defined");
                    ESP_LOGW(NRF_MESSAGE_HANDLER_TAG, "nrf_message_handler_queue invalid message type, message: %s", queue_item);
                    break;
            }
		}
	}
}

esp_err_t send_nrf_message(int device, request_type type){
    char data[NRF_QUEUE_ITEM_LEN];
    // first byte indicates sending data over nrf
    data[0] = '0';
    // third and fourth byte are device number
    data[2] = '0' + device / 10;
    data[3] = '0' + device % 10;
    data[5] = '\0';
    data[NRF_QUEUE_ITEM_LEN - 1] = '\0';
    switch (type){
        case PING:
            // second byte is data type
            data[1] = '1';
            // fifth byte is 1 for simple ping
            data[4] = '1';
            break;
        case TURNON:
            // second byte is data type
            data[1] = '0';
            // fifth byte is 1 for turn on
            data[4] = '1';
            break;
        case TURNOFF:
            // second byte is data type
            data[1] = '0';
            // fifth byte is 0 for turn off
            data[4] = '0';
            break;
        default:
            ESP_LOGW(NRF_MESSAGE_HANDLER_TAG, "send_nrf_message invalid message, type not defined");
            return ESP_FAIL;
    }

    // transfer data to nrf_task
    if(nrf_queue == NULL){
        ESP_LOGE(NRF_MESSAGE_HANDLER_TAG, "nrf_queue is NULL");
        return ESP_FAIL;
    }

    if (xQueueSend(nrf_queue, (void *)&data, 0) != pdTRUE) {
        ESP_LOGE(NRF_MESSAGE_HANDLER_TAG, "FAILED TO SEND TO nrf_queue");
        return ESP_FAIL;
    }else{
        if(type == PING){
            ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "sending PING to device: %d", device);
        }
        else if(type == TURNON){
            ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "sending TURNON to device: %d", device);
        }
        else if(type == TURNOFF){
            ESP_LOGI(NRF_MESSAGE_HANDLER_TAG, "sending TURNOFF to device: %d", device);
        }
        return ESP_OK;
    }

}
