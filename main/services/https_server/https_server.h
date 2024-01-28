//
// Created by Vincent.
//

#ifndef HTTPS_SERVER_H
#define HTTPS_SERVER_H

#define POST_BUF_SIZE 256
#define HTTPS_SERVER_TAG "https_server"

void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#endif //HTTPS_SERVER_H