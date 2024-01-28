//
// Created by Vincent.
//

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/sockets.h>
#include <string.h>
#include <stdio.h>
#include "../../logger/logger.h"
#include "../service_message_handler.h"
#include "../../access/access.h"
#include "tcp_server.h"


// Forward declarations for static functions/params
static SemaphoreHandle_t accept_semaphore;
static void tcp_server_connection_task(void *pvParameters);


void tcp_server_task(void *pvParameters) {
    // Create a semaphore to limit the number of connections
    accept_semaphore = xSemaphoreCreateCounting(MAX_CONNECTIONS, MAX_CONNECTIONS);

    int listen_sock, conn_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    int optval;

    // Create a listening socket
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TCP_TAG, "Error creating listening socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Set socket options
    optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        ESP_LOGE(TCP_TAG, "Error setting socket options: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Bind the socket to the port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT_NUMBER);
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TCP_TAG, "Error binding listening socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Listen for incoming connections
    if (listen(listen_sock, MAX_CONNECTIONS) < 0) {
        ESP_LOGE(TCP_TAG, "Error listening for incoming connections: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TCP_TAG, "TCP server listening on port %d", PORT_NUMBER);

    while (1) {
        // Wait for an incoming connection
        client_addr_len = sizeof(client_addr);
        conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn_sock < 0) {
            ESP_LOGE(TCP_TAG, "Error accepting connection: errno %d", errno);
            continue;
        }

        // Take the semaphore to limit the number of connections
        if (xSemaphoreTake(accept_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TCP_TAG, "Accepted connection #%d from %s:%d", uxSemaphoreGetCount(accept_semaphore), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Spawn a new task to handle the connection
            xTaskCreate(tcp_server_connection_task, "tcp_server_connection_task", 8*1024, (void *)&conn_sock, 3, NULL);
        } else {
            ESP_LOGE(TCP_TAG, "Semaphore timeout while waiting for accept_semaphore");
            close(conn_sock);
        }
    }
}

static void tcp_server_connection_task(void *pvParameters) {
    int conn_sock = *((int *)pvParameters);
    int recv_len;
    char recv_buf[MAX_DATA_LENGTH];
    connection_state CONN_STATE = USERNAME;
    int userID = -1;

    // Set timeout for connection
    struct timeval tv;
    tv.tv_sec = CONNECTION_TIMEOUT_MS / 1000;
    tv.tv_usec = (CONNECTION_TIMEOUT_MS % 1000) * 1000;
    if (setsockopt(conn_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        ESP_LOGE(TCP_TAG, "Error setting connection timeout: errno %d", errno);
        close_conn(conn_sock);
        return;
    }

    char *ret = "username:\n";
    if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
        ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        return;
    }

    while (1) {
        // Receive data from the client
        recv_len = recv(conn_sock, recv_buf, MAX_DATA_LENGTH-1, 0);
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout occurred
                ESP_LOGI(TCP_TAG, "Connection timed out after %d ms", CONNECTION_TIMEOUT_MS);
            } else {
                ESP_LOGW(TCP_TAG, "Error receiving data: errno %d", errno);
            }
            break;
        } else if (recv_len == 0) {
            ESP_LOGI(TCP_TAG, "Connection closed by client");
            break;
        }
        recv_buf[recv_len] = '\0';

        ESP_LOGI(TCP_TAG, "Received %d bytes of data. state: %d", recv_len, CONN_STATE);

        switch (CONN_STATE) {

            case USERNAME:
                username_handle(recv_buf, recv_len, conn_sock);
                CONN_STATE = PASSWORD;
                break;
            case PASSWORD:
                password_handle(recv_buf, recv_len, conn_sock);
                CONN_STATE = ID;
                break;
            case ID:
                userID = identification_handle(recv_buf, recv_len, conn_sock);
                CONN_STATE = OPENED;
                break;
            case OPENED:
                opened_handle(recv_buf, recv_len, conn_sock, userID);
                break;
            case CLOSED:
                break;
        }
    }

    close_conn(conn_sock);
}

void username_handle(char *username, int username_len, int conn_sock){
    // handle username message
    message_sanity_check(username, &username_len, conn_sock);

    if (username_len != strlen(USERNAMEID) || strncmp(username, USERNAMEID, username_len) != 0){
        // close connection in case of invalid password
        ESP_LOGE(TCP_TAG, "Error invalid username, conn_sock: %d", conn_sock);
        close_conn(conn_sock);
    }

    char *ret = "password:\n";
    if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
        ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        return;
    }
}

void password_handle(char *password, int password_len, int conn_sock){
    // handle password message
    message_sanity_check(password, &password_len, conn_sock);

    if (password_len != strlen(PASSWORDID) || strncmp(password, PASSWORDID, password_len) != 0){
        // close connection in case of invalid password
        ESP_LOGE(TCP_TAG, "Error invalid password, conn_sock: %d", conn_sock);
        close_conn(conn_sock);
    }

    char *ret = "user ID:\n";
    if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
        ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        return;
    }
}

int identification_handle(char *id, int id_len, int conn_sock){
    // handle identification of client
    message_sanity_check(id, &id_len, conn_sock);

    // Convert string to integer
    int int_id = atoi(id);
    
    // Check if the conversion was successful
    if (int_id == 0 && id[0] != '0') {
        ESP_LOGE(TCP_TAG, "Error invalid ID: %s , conn: %d", id, conn_sock);
        close_conn(conn_sock);
        return -1;
    }

    ESP_LOGI(TCP_TAG, "Succesfull login, conn: %d -> ID: %d", conn_sock, int_id);

    // log
    char info[LOGGER_QUEUE_ITEM_LEN];
    snprintf(info, LOGGER_QUEUE_ITEM_LEN, "Succesfull TCP login by ID: %d", int_id);
    log_item(TCP_TAG, (char *)info);

    send_help(conn_sock);

    return int_id;
}

void opened_handle(char *message, int message_len, int conn_sock, int userID){
    // handle message
    message_sanity_check(message, &message_len, conn_sock);

    if (message == NULL || message_len <= 0) {
        char *ret = "invalid data: failed\n";
        if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            return;
        }
        return;
    }

    // Tokenize the message
    char *command = strtok(message, " ");
    if (command == NULL) {
        char *ret = "failed to tokenize: failed\n";
        if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        }
        return;
    }

    if (strcmp(command, "HELP") == 0) {
        send_help(conn_sock);
        return;
    }

    message_type_service message_type = -1;
    void *args = NULL;
    char log_message[LOGGER_QUEUE_ITEM_LEN];

    if (strcmp(command, "TURNON") == 0) {
        // log
        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "TURNON message received from ID: %d", userID);

        message_type = TURNON_SERVICE;
        char *next_token = strtok(NULL, " ");

        if (next_token != NULL) {
            char *endptr;
            unsigned long device_id_temp = strtoul(next_token, &endptr, 10);

            if ((*endptr == '\0' || *endptr == ' ') && device_id_temp <= 99) {
                uint8_t device_id = (uint8_t)device_id_temp;
                turnon_turnoff_service_args_t message_args;
                message_args.device_id = device_id;
                args = &message_args;
            } else {
                ESP_LOGW(TCP_TAG, "Invalid TURNON parameter: %s", next_token);
                // Handle the invalid parameter, e.g., send an error message to the user
                const char *error_message = "Invalid TURNON parameter. Expected value from 0 to 99.\n";
                if (send(conn_sock, error_message, strlen(error_message), 0) < 0) {
                    ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
                }
                return;
            }
        } else {
            ESP_LOGW(TCP_TAG, "Missing TURNON parameter");
            // Handle the missing parameter, e.g., send an error message to the user
            const char *error_message = "Missing TURNON parameter. Expected value between 0 and 99.\n";
            if (send(conn_sock, error_message, strlen(error_message), 0) < 0) {
                ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            }
            return;
        }
    } else if (strcmp(command, "TURNOFF") == 0) {
        // log
        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "TURNOFF message received from ID: %d", userID);

        message_type = TURNOFF_SERVICE;
        char *next_token = strtok(NULL, " ");

        if (next_token != NULL) {
            char *endptr;
            unsigned long device_id_temp = strtoul(next_token, &endptr, 10);

            if ((*endptr == '\0' || *endptr == ' ') && device_id_temp <= 99) {
                uint8_t device_id = (uint8_t)device_id_temp;
                turnon_turnoff_service_args_t message_args;
                message_args.device_id = device_id;
                args = &message_args;
            } else {
                ESP_LOGW(TCP_TAG, "Invalid TURNOFF parameter: %s", next_token);
                const char *error_message = "Invalid TURNOFF parameter. Expected value between 1 and 99.\n";
                if (send(conn_sock, error_message, strlen(error_message), 0) < 0) {
                    ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
                }
                return;
            }
        } else {
            ESP_LOGW(TCP_TAG, "Missing TURNOFF parameter");
            const char *error_message = "Missing TURNOFF parameter. Expected value between 1 and 99.\n";
            if (send(conn_sock, error_message, strlen(error_message), 0) < 0) {
                ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            }
            return;
        }
    } else if (strcmp(command, "SYNC") == 0) {
        // log
        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "SYNC message received from ID: %d", userID);
        
        message_type = SYNC_SERVICE;
    } else if (strcmp(command, "ACCESSLEVEL") == 0) {
        // log
        snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "ACCESSLEVEL message received from ID: %d", userID);

        message_type = ACCESSLEVEL_SERVICE;

        char *change_type_str = strtok(NULL, " ");
        char *target_type_str = strtok(NULL, " ");
        char *new_access_level_str = strtok(NULL, " ");
        char *target_id_str = strtok(NULL, " ");

        if (change_type_str && target_type_str && new_access_level_str && target_id_str) {
            int change_type = atoi(change_type_str);
            int target_type = atoi(target_type_str);
            int new_access_level = atoi(new_access_level_str);
            bool valid_target_id = false;

            if (target_type == 0) {
                int target_id_int = atoi(target_id_str);
                valid_target_id = target_id_int >= 1 && target_id_int <= 99;
            } else if (target_type == 1) {
                valid_target_id = strlen(target_id_str) == 16;
            }

            if (change_type >= 0 && change_type <= 2 &&
                target_type >= 0 && target_type <= 1 &&
                new_access_level >= 0 && new_access_level <= 9 &&
                valid_target_id) {

                accesslevel_service_args_t message_args;
                message_args.change_type = change_type;
                message_args.target_type = target_type;
                message_args.new_access_level = new_access_level;

                if (message_args.target_type == 0) {
                    message_args.target.device_id = atoi(target_id_str);
                } else {
                    strncpy(message_args.target.key_id, target_id_str, 16);
                    message_args.target.key_id[16] = '\0';
                }

                args = &message_args;
            } else {
                const char *error_message = "ACCESSLEVEL: Invalid parameters\n";
                send(conn_sock, error_message, strlen(error_message), 0);
                return;
            }
        } else {
            const char *error_message = "ACCESSLEVEL: Missing parameters\n";
            send(conn_sock, error_message, strlen(error_message), 0);
            return;
        }
    } else if(strcmp(command, "SHOW") == 0){
        char *resource = strtok(NULL, " ");
        if (resource == NULL) {
            if (send(conn_sock, "Error: Missing resource type. Use SHOW KEYS, SHOW DEVICES, or SHOW LOGS.\n", strlen("Error: Missing resource type. Use SHOW KEYS, SHOW DEVICES, or SHOW LOGS.\n"),0) < 0) {
                ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            }
            return;
        }

        if (strcmp(resource, "KEYS") == 0) {
            // log
            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "SHOW KEYS message received from ID: %d", userID);
            // Display the list of keys
            show_keys(conn_sock);
        } else if (strcmp(resource, "DEVICES") == 0) {
            // log
            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "SHOW DEVICES message received from ID: %d", userID);
            // Display the list of devices
            show_devices(conn_sock);
        } else if (strcmp(resource, "LOGS") == 0) {
            // log
            snprintf(log_message, LOGGER_QUEUE_ITEM_LEN, "SHOW LOGS message received from ID: %d", userID);
            // Display the logs
            show_logs(conn_sock);
        } else {
            send(conn_sock, "Error: Invalid resource type. Use SHOW KEYS, SHOW DEVICES, or SHOW LOGS.\n", strlen("Error: Invalid resource type. Use SHOW KEYS, SHOW DEVICES, or SHOW LOGS.\n"),0);
            return;
        }
        // log
        log_item(TCP_TAG, log_message);
        return;
    } else if(strcmp(command, "CLOSE") == 0){
        close_conn(conn_sock);
    }else {
        char *ret = "invalid service request: failed\n";
        if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        }
        return;
    }

    // log
    log_item(TCP_TAG, log_message);

    if(handle_service_message(message_type, args) != ESP_OK){
        // return failed message
        char *ret = "service request: failed\n";
        if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            return;
        }
    }else{
        // return success message
        char *ret = "service request: success\n";
        if (send(conn_sock, (void *)ret, strlen(ret), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            return;
        }
    }
}

esp_err_t device_type_check(char *message, int message_len, int userID){
    // device number decoding
    if(message_len < 2){
        ESP_LOGW(TCP_TAG, "ID: %d   MESSAGE: INVALID DEVICE", userID);
        return ESP_FAIL;
    }

    int device = atoi(message + 1);

    if (device == 0) {
        ESP_LOGW(TCP_TAG, "ID: %d   MESSAGE: INVALID DEVICE", userID);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void message_sanity_check(char *message, int *message_len, int conn_sock){
    // sanity check on any incoming message as a buffer overflow protection
    if (*message_len < 0) {
        ESP_LOGE(TCP_TAG, "Error receiving message: errno %d", errno);
        close_conn(conn_sock);
        return;
    }
    // Remove any trailing spaces or newlines or carriage return
    while (*message_len > 0 && (message[(*message_len) - 1] == '\n' || message[(*message_len) - 1] == ' ' || message[(*message_len) - 1] == '\r')) {
        (*message_len)--;
        message[*message_len] = '\0';
    }
    message[*message_len] = '\0';
}

esp_err_t send_help(int conn_sock){
    const char *help_message =
        "Available commands:\n"
        "\n"
        "  TURNON <device_id>\n"
        "    Turns on the specified device.\r"
        "    <device_id> : The ID of the device to control (0: ALL, 1-99).\n"
        "\n"
        "  TURNOFF <device_id>\n"
        "    Turns off the specified device.\n"
        "    <device_id> : The ID of the device to control (0: ALL, 1-99).\n"
        "\n"
        "  SYNC\n"
        "    Synchronizes the device list.\n"
        "\n"
        "  ACCESSLEVEL <change_type> <target_type> <new_access_level> <target_id>\n"
        "    Changes the access level of the specified device or key.\n"
        "    <change_type>       : The type of change (0: MODIFY, 1: ADD, 2: DELETE).\n"
        "    <target_type>       : The type of target (0: DEVICE, 1: KEY).\n"
        "    <new_access_level>  : The new access level (0: NO ACCESS, ... , 9: FULL ACCESS).\n"
        "    <target_id>         : The ID of the device (1-99) or key (16byte) to change the access level.\n"
        "\n"
        "  SHOW <resource>\n"
        "    Shows the list of the specified resource (KEYS, DEVICES, LOGS).\n"
        "    <resource> : The type of resource to display (KEYS, DEVICES, LOGS).\n"
        "\n"
        "  CLOSE\n"
        "    Closes the connection.\n";

    if (send(conn_sock, help_message, strlen(help_message), 0) < 0) {
        ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t show_keys(int conn_sock){
    int ret = ESP_OK;

    char keys_copy[MAX_KEYS][KEY_LENGHT + 1];
    int access_levels_copy[MAX_KEYS];

    int amount_keys = get_keys((char *)keys_copy, access_levels_copy);
    if (amount_keys == ESP_FAIL) {
        ESP_LOGE(TCP_TAG, "Error getting keys");
        const char *error_message = "show keys: failed\n";
        send(conn_sock, error_message, strlen(error_message), 0);
        return ESP_FAIL;
    }

    for (int i = 0; i < amount_keys; ++i) {
        char key_info[50]; // Key length, space, access level (1 digit), newline, and null terminator
        snprintf(key_info, sizeof(key_info), "key: %s\taccess level: %d\n", keys_copy[i], access_levels_copy[i]);
        if (send(conn_sock, key_info, strlen(key_info), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t show_devices(int conn_sock){
    int ret = ESP_OK;

    int devices_copy[MAX_DEVICES];
    int access_levels_copy[MAX_DEVICES];

    int amount_devices = get_devices(devices_copy, access_levels_copy);
    if (amount_devices == ESP_FAIL) {
        ESP_LOGE(TCP_TAG, "Error getting devices");
        const char *error_message = "show devices: failed\n";
        send(conn_sock, error_message, strlen(error_message), 0);
        return ESP_FAIL;
    }

    for (int i = 0; i < amount_devices; ++i) {
        char device_info[50]; // Device number (up to 5 digits), space, access level (1 digit), newline, and null terminator
        snprintf(device_info, sizeof(device_info), "device: %d\taccess level: %d\n", devices_copy[i], access_levels_copy[i]);
        if (send(conn_sock, device_info, strlen(device_info), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            ret = ESP_FAIL;
        }
    }

    return ret;
}

esp_err_t show_logs(int conn_sock) {
    int ret = ESP_OK;
    int logs_count = get_log_lines();
    for (int i = 1; i <= logs_count; ++i) {
        char log[LOGGER_QUEUE_ITEM_LEN+2];
        get_log(i, log, LOGGER_QUEUE_ITEM_LEN);
        log[LOGGER_QUEUE_ITEM_LEN] = '\n';
        log[LOGGER_QUEUE_ITEM_LEN+1] = '\0';
        if (send(conn_sock, log, strlen(log), 0) < 0) {
            ESP_LOGW(TCP_TAG, "Error sending data: errno %d", errno);
            ret = ESP_FAIL;
        }
    }
    return ret;
}

void close_conn(int conn_sock){
    ESP_LOGE(TCP_TAG, "closing connection: conn_sock %d", conn_sock);

    // Close the connection
    close(conn_sock);

    // Release the semaphore to allow another connection to be accepted
    xSemaphoreGive(accept_semaphore);

    // Delete this task
    vTaskDelete(NULL);
}
