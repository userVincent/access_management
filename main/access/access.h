//
// Created by Vincent.
//

#ifndef ACCESS_H
#define ACCESS_H

#include "esp_err.h"

#define ACCESS_TAG "ACCESS"

#define KEYACCESSFILENAME "/spiffs/key_access.txt"
#define DEVICEACCESSFILENAME "/spiffs/device_access.txt"
#define MAX_KEYS 40
#define MAX_DEVICES 40
#define KEY_LENGHT 16
#define ACCESS_LEVEL_LENGTH 1
#define DEVICE_LENGHT 2

typedef struct {
    char key[KEY_LENGHT+1];
    int access_level;
} key;

typedef struct {
    int device;
    int access_level;
} device;

esp_err_t init_access();

esp_err_t add_key(char *key, int access_level);

esp_err_t delete_key(const char *key);

esp_err_t change_key_access_level(const char *key, int new_access_level);

esp_err_t add_device(int device, int access_level);

esp_err_t delete_device(int device);

esp_err_t change_device_access_level(int device, int new_access_level);

esp_err_t has_access(int device, const char *key);

esp_err_t delete_all_keys();

esp_err_t delete_all_devices();

int get_keys(char *keys_copy, int *access_levels_copy);

int get_devices(int *devices_copy, int *access_levels_copy);

esp_err_t print_all_data();

#endif //ACCESS_H