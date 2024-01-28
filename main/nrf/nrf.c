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
#include "nrf_message_handler.h"
#include "nrf.h"

QueueHandle_t nrf_queue;
QueueHandle_t nrf_message_handler_queue;

// Forward declarations for static functions/params
static QueueHandle_t in_transit_queue;
static NRF24_t dev;


void nrf_task(void *pvParameters){
	// initialize
	init_nrf_queue();
	init_in_transit_queue();
	init_nrf_message_handler_queue();
	init_nrf();
	init_IRQ();

	//Print settings
	Nrf24_printDetails(&dev);
	ESP_LOGI(NRF_TAG, "nrf_task ready");

	// create nrf_message_handler task
	xTaskCreate(nrf_message_handler_task, "nrf_message_handler_task", 1024*8, NULL, 4, NULL);

	// receive messages on queue
	char queue_item[NRF_QUEUE_ITEM_LEN];
	while(1){
		//Nrf24_printDetails(&dev);
		if (xQueueReceive(nrf_queue, (void *)&queue_item, portMAX_DELAY) == pdTRUE) {
			// handle message from nrf_queue
			if(queue_item[0] == '0'){
				// set transmit address
				char address[5] = "00000";
				address[3] = queue_item[2];
				address[4] = queue_item[3];
				if(Nrf24_setTADDR(&dev, (uint8_t *)address) != ESP_OK){
					ESP_LOGW(NRF_TAG, "FAILED TO SET TRANSMIT ADDRESS");
				}

				// add message to in_transit_queue
				if(xQueueSend(in_transit_queue, &queue_item[1], 10) != pdTRUE){
					ESP_LOGW(NRF_TAG, "in_transit_queue is full, message not sent");
					continue;
				}

				// send message
				Nrf24_send(&dev, (uint8_t *)&queue_item[1]);

			}else if(queue_item[0] == '1'){
				// interrupt (can either be a MAX_RT or RX_DR or TX_DS interrupt)
				check_interrupt_type();

			}else{
				ESP_LOGW(NRF_TAG, "nrf_queue invalid message, type not defined");
			}
		}
	}
}

static void IRAM_ATTR gpio_isr_handler(void* arg){
	char queue_item[NRF_QUEUE_ITEM_LEN];
	queue_item[0] = '1';
	if(nrf_queue != NULL){
		xQueueSendFromISR(nrf_queue, &queue_item, NULL);
		disable_isr();
	}
	// if nrf_queue == NULL, nothing gets done to keep isr short
}

void init_nrf_queue(){
	// initialize queue
    nrf_queue = xQueueCreate(NRF_QUEUE_LEN, NRF_QUEUE_ITEM_LEN);

	if (nrf_queue == NULL) {
        ESP_LOGE(NRF_TAG, "FAILED TO CREATE nrf_queue");
        destruct_nrf_task();
    }
}

void init_in_transit_queue(){
	// initialize queue
	in_transit_queue = xQueueCreate(NRF_QUEUE_LEN, NRF_DATA_LEN);

	if (in_transit_queue == NULL) {
		ESP_LOGE(NRF_TAG, "FAILED TO CREATE in_transit_queue");
		destruct_nrf_task();
	}
}

void init_nrf_message_handler_queue(){
	// initialize queue
    nrf_message_handler_queue = xQueueCreate(NRF_QUEUE_LEN, NRF_DATA_LEN);

	if (nrf_message_handler_queue == NULL) {
        ESP_LOGE(NRF_MESSAGE_HANDLER_TAG, "FAILED TO CREATE nrf_message_handler_queue");
        destruct_nrf_task();
    }
}

void init_nrf(){
	Nrf24_init(&dev);
	uint8_t payload = NRF_DATA_LEN;
	Nrf24_config(&dev, CHANNEL, payload);
	//uint8_t new_config = 0x00;
	//Nrf24_writeRegister(&dev, EN_AA, &new_config, 1);
	Nrf24_SetOutputRF_PWR(&dev, RF24_PA_HIGH);
	Nrf24_setRetransmitDelay(&dev, 5); // (5+1)*250us = 1500us delay between retransmits
	enable_all_interrupts();
	uint8_t AA_config = 0x00;
	Nrf24_writeRegister(&dev, EN_AA, &AA_config, 1);

    //Set own address using 5 characters to pipe 1
	esp_err_t ret = Nrf24_setRADDR(&dev, (uint8_t *)MASTER_ADDR);
	if (ret != ESP_OK) {
		ESP_LOGE(NRF_TAG, "nrf24l01 not connected");
		destruct_nrf_task();
	}
}

void init_IRQ(){
	// Initialize gpio
	//zero-initialize the config structure.
	gpio_config_t io_conf = {};
	//interrupt of falling edge
	io_conf.intr_type = GPIO_INTR_NEGEDGE;
	//bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	//set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	//enable pull-up mode
	io_conf.pull_up_en = 1;

	esp_err_t err = gpio_config(&io_conf);
	if (err != ESP_OK) {
		ESP_LOGE(NRF_TAG, "Failed to configure GPIO: %s", esp_err_to_name(err));
		destruct_nrf_task();
	}

	//install gpio isr service
	err = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	if (err != ESP_OK) {
		ESP_LOGE(NRF_TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
		destruct_nrf_task();
	}

	//hook isr handler for specific gpio pin
	err = gpio_isr_handler_add(CONFIG_IRQ_GPIO, gpio_isr_handler, (void*) CONFIG_IRQ_GPIO);
	if (err != ESP_OK) {
		ESP_LOGE(NRF_TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
		destruct_nrf_task();
	}
}

void enable_isr() {
    esp_err_t err = gpio_isr_handler_add(CONFIG_IRQ_GPIO, gpio_isr_handler, (void*) CONFIG_IRQ_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(NRF_TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
    }
}

void disable_isr() {
	esp_err_t err = gpio_isr_handler_remove(CONFIG_IRQ_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(NRF_TAG, "Failed to remove ISR handler: %s", esp_err_to_name(err));
    }
}

void enable_all_interrupts() {
    uint8_t config_register;
    uint8_t new_config;

    // Read the current CONFIG register
    Nrf24_readRegister(&dev, CONFIG, &config_register, 1);

    // Clear the MASK_RX_DR, MASK_TX_DS, and MASK_MAX_RT bits to enable all interrupts
    new_config = config_register & ~(1 << MASK_RX_DR) & ~(1 << MASK_TX_DS) & ~(1 << MASK_MAX_RT);

    // Write the new CONFIG register value
    Nrf24_writeRegister(&dev, CONFIG, &new_config, 1);
}

void destruct_nrf_task(){
	ESP_LOGI(NRF_TAG, "destruct_nrf_task");
	if(nrf_queue != NULL){
		vQueueDelete(nrf_queue);
		nrf_queue = NULL;
	}
	if(nrf_message_handler_queue != NULL){
		vQueueDelete(nrf_message_handler_queue);
		nrf_message_handler_queue = NULL;
	}
	if(in_transit_queue != NULL){
		vQueueDelete(in_transit_queue);
		in_transit_queue = NULL;
	}
	vTaskDelete(NULL);
}

nrf_interrupt check_interrupt_type() {
    // Checking the interrupt type has to be handled quickly to avoid missing interrupts
    uint8_t status = Nrf24_getStatus(&dev);
	char mydata[NRF_DATA_LEN];

    if (status & (1 << RX_DR)) {
        // RX_DR (Data Ready) interrupt
		
		// get data from nrf
		Nrf24_getData(&dev, (uint8_t *)mydata);

		// put data in message queue
		if(xQueueSend(nrf_message_handler_queue, mydata, 10) == errQUEUE_FULL){
			ESP_LOGW(NRF_TAG, "nrf_message_handler_queue is full");
		}

		// clear RX_DR bit
		vTaskDelay(5);
		uint8_t clear_RX_DR = (1 << RX_DR);
		Nrf24_writeRegister(&dev, STATUS, &clear_RX_DR, 1);

		// enable ISR
		vTaskDelay(5);
		enable_isr();

		return RECEIVE_SUCCESS;
    }
    else if (status & (1 << TX_DS)) {
        // TX_DS (Data Sent) interrupt

		// get data from in_transit_queue
		if(xQueueReceive(in_transit_queue, mydata, 10) == pdTRUE){
			ESP_LOGI(NRF_TAG, "TX_DS interrupt data: %s", mydata);
		}else{
			ESP_LOGW(NRF_TAG, "in_transit_queue is empty when TX_DS interrupt is received");
		}

		// put data in message queue
		mydata[NRF_DATA_LEN - 1] = 'X'; // add X to indicate that this is an ACK
		if(xQueueSend(nrf_message_handler_queue, mydata, 10) == errQUEUE_FULL){
			ESP_LOGW(NRF_TAG, "nrf_message_handler_queue is full");
		}

		// clear TX_DS bit
		vTaskDelay(5);
		uint8_t clear_TX_DS = (1 << TX_DS);
		Nrf24_writeRegister(&dev, STATUS, &clear_TX_DS, 1);
		dev.PTX = 0;

		// power up RX
		vTaskDelay(5);
		//Nrf24_flushRx(&dev);
		Nrf24_powerUpRx(&dev);

		// enable ISR
		vTaskDelay(5);
		enable_isr();

		return SEND_SUCCESS;
    }
    else if (status & (1 << MAX_RT)) {
        // MAX_RT (Maximum Retries) interrupt

		// get data from in_transit_queue
		if(xQueueReceive(in_transit_queue, mydata, 10) == pdTRUE){
			ESP_LOGW(NRF_TAG, "MAX_RT interrupt data: %s", mydata);
		}else{
			ESP_LOGW(NRF_TAG, "in_transit_queue is empty when MAX_RT interrupt is received");
		}
		
		// clear MAX_RT bit
		vTaskDelay(5);
		uint8_t clear_MAX_RT = (1 << MAX_RT);
        Nrf24_writeRegister(&dev, STATUS, &clear_MAX_RT, 1);
		dev.PTX = 0;

		// power up RX
		vTaskDelay(5);
		//Nrf24_flushRx(&dev);
		Nrf24_powerUpRx(&dev);

		//enable ISR
		vTaskDelay(5);
		enable_isr();

		return SEND_FAILED;
    }

	// No interrupt
	ESP_LOGW(NRF_TAG, "No interrupt register set while IRQ triggered");

	// enable ISR
	enable_isr();

	return ERROR;
}
