//
// Created by Vincent.
//

/*
  -TX address will change depending on the slave 
    the masternode has to communicate with
  -RX pipe 0 address will correspond to TX address for ACK
  -RX pipe 1 address will be the MASTER_ADDR to receive
    messages from the slave nodes
*/

#ifndef NRF_H
#define NRF_H

#define NRF_TAG "NRF"

#define NRF_QUEUE_LEN 50
#define NRF_DATA_LEN 16     // amount of bytes of data
#define NRF_QUEUE_ITEM_LEN 17   // 1 byte for type, 16 bytes for data
#define CHANNEL 10
#define GPIO_INPUT_PIN_SEL (1ULL<<CONFIG_IRQ_GPIO)
#define ESP_INTR_FLAG_DEFAULT 0
#define MASTER_ADDR "AAAAA"

extern QueueHandle_t nrf_message_handler_queue;
extern QueueHandle_t nrf_queue;

typedef enum {
    RECEIVE_SUCCESS,
    SEND_SUCCESS,
    SEND_FAILED,
    ERROR 
} nrf_interrupt;

void nrf_task(void *pvParameters);

void init_nrf_queue();

void init_in_transit_queue();

void init_nrf_message_handler_queue();

void init_nrf();

void init_IRQ();

void enable_isr();

void disable_isr();

void enable_all_interrupts();

void destruct_nrf_task();

nrf_interrupt check_interrupt_type();

#endif //NRF_H