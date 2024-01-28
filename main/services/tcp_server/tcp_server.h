//
// Created by Vincent.
//

#ifndef MAIN_TCP_SERVER_H
#define MAIN_TCP_SERVER_H

#define TCP_TAG "TCP"

#define PORT_NUMBER 3333
#define MAX_CONNECTIONS 5
#define CONNECTION_TIMEOUT_MS 300000 // 5 minutes
#define MAX_DATA_LENGTH 250
#define USERNAMEID "root"
#define PASSWORDID "password"

typedef enum{USERNAME, PASSWORD, ID, OPENED, CLOSED} connection_state;
//typedef enum{TURNALLON, TURNALLOFF, TURNONDEVICE, TURNOFFDEVICE, GETLOGS, ACCESSLEVEL} message_type;
//typedef enum{DRAWER1, DRAWER2, SAW} device_type; // this is just an example as the true amount of devices is unknown at the moment, maybe this should just be an int in order to be more flexible when adding devices when in use


void tcp_server_task(void *pvParameters);

void username_handle(char *username, int username_len, int conn_sock);

void password_handle(char *password, int password_len, int conn_sock);

int identification_handle(char *id, int id_len, int conn_sock);

void opened_handle(char *message, int message_len, int conn_sock, int userID);

esp_err_t device_type_check(char *message, int message_len, int userID);

void message_sanity_check(char *username, int *username_len, int conn_sock);

esp_err_t send_help(int conn_sock);

esp_err_t show_keys(int conn_sock);

esp_err_t show_devices(int conn_sock);

esp_err_t show_logs(int conn_sock);

void close_conn(int conn_sock);

#endif //MAIN_TCP_SERVER_H