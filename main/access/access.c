//
// Created by Vincent.
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/semphr.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_log.h"
#include "../spiffs/spiffs.h"
#include "access.h"


// Forward declarations for static functions/params
static int amount_keys;
static int amount_devices;
static key keys[MAX_KEYS];
static device devices[MAX_DEVICES];
static SemaphoreHandle_t access_mutex;
static esp_err_t parse_key_access(const char *input, char *key, int *access_level);
static esp_err_t parse_device_access(const char *input, int *device, int *access_level);
static esp_err_t is_valid_key(const char *key);
static esp_err_t is_valid_device(int device);
static esp_err_t is_valid_access_level(int access_level);
static esp_err_t add_key_to_file(const char *key, int access_level);
static esp_err_t change_key_access_level_in_file(const char *key, int new_access_level, int line);
static int key_exists(const char *key);
static esp_err_t add_device_to_file(int device, int access_level);
static esp_err_t change_device_access_level_in_file(int device, int new_access_level, int line);
static int device_exists(int device);


esp_err_t init_access(){
    // initialize mutex
    access_mutex = xSemaphoreCreateMutex();

    if(create_file_if_not_exists(KEYACCESSFILENAME) == -1 || create_file_if_not_exists(DEVICEACCESSFILENAME) == -1){
        return ESP_FAIL;
    };

    // initialize key access levels
    amount_keys = count_lines(KEYACCESSFILENAME);
    for (int key_number = 0; key_number < amount_keys; ++key_number){
        char line[KEY_LENGHT + 3 + ACCESS_LEVEL_LENGTH]; // +3 for the comma and nulloperator and possible endline

        if(read_line_from_file(KEYACCESSFILENAME, key_number+1, (char *)&line, KEY_LENGHT + 3 + ACCESS_LEVEL_LENGTH) == -1){
            ESP_LOGE(ACCESS_TAG, "failed initialize key access levels, failed to read line from file");
            return ESP_FAIL;
        }

        line[strcspn(line, "\n")] = '\0'; // Remove newline character

        if(parse_key_access((char *)&line, (char *)&(keys[key_number].key), &(keys[key_number].access_level)) != ESP_OK){
            ESP_LOGE(ACCESS_TAG, "failed initialize key access levels, failed to parse");
            return ESP_FAIL;
        }
    }

    // initialize device access levels
    amount_devices = count_lines(DEVICEACCESSFILENAME);
    for (int device_number = 0; device_number < amount_devices; ++device_number){
        char line[DEVICE_LENGHT + 3 + ACCESS_LEVEL_LENGTH]; // +3 for the comma and nulloperator and possible endline

        if(read_line_from_file(DEVICEACCESSFILENAME, device_number+1, (char *)&line, DEVICE_LENGHT + 3 + ACCESS_LEVEL_LENGTH) == -1){
            ESP_LOGE(ACCESS_TAG, "failed initialize device access levels, failed to read line from file");
            return ESP_FAIL;
        }

        line[strcspn(line, "\n")] = '\0'; // Remove newline character

        if(parse_device_access((char *)&line, &(devices[device_number].device), &(devices[device_number].access_level)) != ESP_OK){
            ESP_LOGE(ACCESS_TAG, "failed initialize device access levels, failed to parse");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t parse_key_access(const char *input, char *key, int *access_level) {
    size_t input_len = strlen(input);
    if (input_len != KEY_LENGHT + ACCESS_LEVEL_LENGTH + 1) { // +1 for the comma
        return ESP_FAIL;
    }

    if (input[KEY_LENGHT] != ',') {
        return ESP_FAIL;
    }

    strncpy(key, input, KEY_LENGHT);
    key[KEY_LENGHT] = '\0';

    for (int i = KEY_LENGHT+1; i < KEY_LENGHT+1+ACCESS_LEVEL_LENGTH; ++i) {
        if (input[i] < '0' || input[i] > '9') {
            *access_level = 0;
            return ESP_FAIL;
        }
        *access_level += (input[i] - '0') * pow(10, KEY_LENGHT+ACCESS_LEVEL_LENGTH-i);
    }

    return ESP_OK;
}

static esp_err_t parse_device_access(const char *input, int *device, int *access_level) {
    size_t input_len = strlen(input);
    if (input_len != DEVICE_LENGHT + ACCESS_LEVEL_LENGTH + 1) { // +1 for the comma
        return ESP_FAIL;
    }

    if (input[DEVICE_LENGHT] != ',') {
        return ESP_FAIL;
    }

    *device = 0;

    for (int i = 0; i < DEVICE_LENGHT; ++i) {
        if (input[i] < '0' || input[i] > '9') {
            *device = 0;
            return ESP_FAIL;
        }
        *device += (input[i] - '0') * pow(10, DEVICE_LENGHT-1-i);
    }

    for (int i = DEVICE_LENGHT+1; i < DEVICE_LENGHT+1+ACCESS_LEVEL_LENGTH; ++i) {
        if (input[i] < '0' || input[i] > '9') {
            *access_level = 0;
            return ESP_FAIL;
        }
        *access_level += (input[i] - '0') * pow(10, DEVICE_LENGHT+ACCESS_LEVEL_LENGTH-i);
    }

    return ESP_OK;
}

static esp_err_t is_valid_key(const char *key) {
    if (strlen(key) != KEY_LENGHT) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t is_valid_device(int device) {
    int max_device = 1;
    for (int i = 0; i < DEVICE_LENGHT; i++) {
        max_device *= 10;
    }
    max_device -= 1; // Max value for DEVICE_LENGHT digits

    if (device < 1 || device > max_device) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t is_valid_access_level(int access_level) {
    int max_access_level = 1;
    for (int i = 0; i < ACCESS_LEVEL_LENGTH; i++) {
        max_access_level *= 10;
    }
    max_access_level -= 1; // Max value for ACCESS_LEVEL_LENGTH digits

    if (access_level < 0 || access_level > max_access_level) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t add_key(char *key, int access_level) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to add key, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_key(key) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to add key, invalid key");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(is_valid_access_level(access_level) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to add key, invalid access level");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(amount_keys >= MAX_KEYS){
        ESP_LOGE(ACCESS_TAG, "failed to add key, max amount of keys");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if (key_exists(key) != -1) {
        ESP_LOGW(ACCESS_TAG, "failed to add key, key already exists");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if (keys[amount_keys].key[0] == '\0')
    { // Check if the key slot is empty
        // add key to file
        if (add_key_to_file(key, access_level) != ESP_OK)
        {
            ESP_LOGE(ACCESS_TAG, "failed to add key, error when adding to file");
            // unlock mutex
            xSemaphoreGive(access_mutex);
            return ESP_FAIL;
        }

        // add key to RAM
        strncpy(keys[amount_keys].key, key, KEY_LENGHT);
        keys[amount_keys].key[KEY_LENGHT] = '\0';
        keys[amount_keys].access_level = access_level;

        // update amount of keys
        ++amount_keys;

        ESP_LOGI(ACCESS_TAG, "succesfully added key");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_OK;
    }

    ESP_LOGE(ACCESS_TAG, "failed to add key");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_FAIL;
}

static esp_err_t add_key_to_file(const char *key, int access_level) {
    char text[KEY_LENGHT + ACCESS_LEVEL_LENGTH + 2];
    snprintf(text, KEY_LENGHT + ACCESS_LEVEL_LENGTH + 2, "%s,%0*d", key, ACCESS_LEVEL_LENGTH, access_level); // +2 for comma and nulterminator
    return write_new_line(KEYACCESSFILENAME, (char *)&text);
}

esp_err_t delete_key(const char *key) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to delete key, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_key(key) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to delete key, invalid key");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    int key_index = key_exists(key);
    if (key_index == -1) {
        ESP_LOGW(ACCESS_TAG, "failed to delete key, key not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // delete key from file
    if(delete_line_from_file(KEYACCESSFILENAME, key_index+1) == -1){
        ESP_LOGE(ACCESS_TAG, "failed to delete key from file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Shift the keys array, removing the empty slot
    for (int i = key_index; i < MAX_KEYS - 1; i++) {
        strcpy(keys[i].key, keys[i + 1].key);
        keys[i].access_level = keys[i + 1].access_level;
    }

    // Clear the last key slot
    keys[MAX_KEYS - 1].key[0] = '\0';
    keys[MAX_KEYS - 1].access_level = 0;

    // update amount of keys
    --amount_keys;

    ESP_LOGI(ACCESS_TAG, "successfully deleted key");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

esp_err_t change_key_access_level(const char *key, int new_access_level) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to change key access level, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_key(key) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to change key access level, invalid key");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(is_valid_access_level(new_access_level) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to change key access level, invalid access level");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    int key_index = key_exists(key);

    if (key_index == -1) {
        ESP_LOGE(ACCESS_TAG, "Failed to change access level, key not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Update the access level in RAM
    keys[key_index].access_level = new_access_level;

    // Update the access level in the file
    if (change_key_access_level_in_file(key, new_access_level, key_index+1) != ESP_OK) {
        ESP_LOGE(ACCESS_TAG, "Failed to update access level in the file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(ACCESS_TAG, "Successfully changed access level for key: %s", key);
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

static esp_err_t change_key_access_level_in_file(const char *key, int new_access_level, int line) {
    char text[KEY_LENGHT + ACCESS_LEVEL_LENGTH + 2];
    snprintf(text, KEY_LENGHT + ACCESS_LEVEL_LENGTH + 2, "%s,%0*d", key, ACCESS_LEVEL_LENGTH, new_access_level); // +2 for comma and nulterminator
    return overwrite_line_in_file(KEYACCESSFILENAME, line, (char *)&text);
}

static int key_exists(const char *key) {
    for (int i = 0; i < MAX_KEYS; i++) {
        if (strcmp(keys[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

esp_err_t add_device(int device, int access_level) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to add device, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_device(device) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to add device, invalid device");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(is_valid_access_level(access_level) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to add device, invalid access level");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if (amount_devices >= MAX_DEVICES) {
        ESP_LOGE(ACCESS_TAG, "failed to add device, max amount of devices");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if (device_exists(device) != -1) {
        ESP_LOGW(ACCESS_TAG, "failed to add device, device already exists");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if (devices[amount_devices].device == 0) { // Check if the device slot is empty
        // add device to file
        if (add_device_to_file(device, access_level) != ESP_OK) {
            ESP_LOGE(ACCESS_TAG, "failed to add device, error when adding to file");
            // unlock mutex
            xSemaphoreGive(access_mutex);
            return ESP_FAIL;
        }

        // add device to RAM
        devices[amount_devices].device = device;
        devices[amount_devices].access_level = access_level;

        // update amount of devices
        ++amount_devices;

        ESP_LOGI(ACCESS_TAG, "successfully added device");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_OK;
    }

    ESP_LOGE(ACCESS_TAG, "failed to add device");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_FAIL;
}

static esp_err_t add_device_to_file(int device, int access_level) {
    char text[DEVICE_LENGHT + ACCESS_LEVEL_LENGTH + 2];
    snprintf(text, DEVICE_LENGHT + ACCESS_LEVEL_LENGTH + 2, "%0*d,%0*d", DEVICE_LENGHT, device, ACCESS_LEVEL_LENGTH, access_level); // +2 for comma and null-terminator
    return write_new_line(DEVICEACCESSFILENAME, (char *)&text);
}

esp_err_t delete_device(int device) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to delete device, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_device(device) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to delete device, invalid device");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    int device_index = device_exists(device);
    if (device_index == -1) {
        ESP_LOGW(ACCESS_TAG, "failed to delete device, device not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // delete device from file
    if (delete_line_from_file(DEVICEACCESSFILENAME, device_index + 1) == -1) {
        ESP_LOGE(ACCESS_TAG, "failed to delete device from file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Shift the devices array to remove the empty slot
    for (int i = device_index; i < MAX_DEVICES - 1; i++) {
        devices[i] = devices[i + 1];
    }

    // Clear the last device
    devices[MAX_DEVICES - 1].device = 0;
    devices[MAX_DEVICES - 1].access_level = 0;

    // update amount of keys
    --amount_devices;

    ESP_LOGI(ACCESS_TAG, "successfully deleted device");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

esp_err_t change_device_access_level(int device, int new_access_level) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to change device access level, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_device(device) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to change device access level, invalid device");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(is_valid_access_level(new_access_level) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to change device access level, invalid access level");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    int device_index = device_exists(device);

    if (device_index == -1) {
        ESP_LOGE(ACCESS_TAG, "Failed to change access level, device not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Update the access level in RAM
    devices[device_index].access_level = new_access_level;

    // Update the access level in the file
    if (change_device_access_level_in_file(device, new_access_level, device_index+1) != ESP_OK) {
        ESP_LOGE(ACCESS_TAG, "Failed to update access level in the file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(ACCESS_TAG, "Successfully changed access level for device: %d", device);
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

static esp_err_t change_device_access_level_in_file(int device, int new_access_level, int line) {
    char text[DEVICE_LENGHT + ACCESS_LEVEL_LENGTH + 2];
    snprintf(text, DEVICE_LENGHT + ACCESS_LEVEL_LENGTH + 2, "%0*d,%0*d", DEVICE_LENGHT, device, ACCESS_LEVEL_LENGTH, new_access_level); // +2 for comma and null-terminator
    return overwrite_line_in_file(DEVICEACCESSFILENAME, line, (char *)&text);
}

static int device_exists(int device) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].device == device) {
            return i;
        }
    }
    return -1;
}

esp_err_t has_access(int device, const char* key) {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to check access, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(is_valid_device(device) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to check access, invalid device");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    if(is_valid_key(key) != ESP_OK){
        ESP_LOGE(ACCESS_TAG, "failed to check access, invalid key");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    int device_access_level = -1;
    int key_access_level = -1;

    // Find the access level for the given device
    int device_index = device_exists(device);
    if (device_index != -1) {
        device_access_level = devices[device_index].access_level;
    } else {
        ESP_LOGW(ACCESS_TAG, "device not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Find the access level for the given key
    int key_index = key_exists(key);
    if (key_index != -1) {
        key_access_level = keys[key_index].access_level;
    } else {
        ESP_LOGW(ACCESS_TAG, "key not found");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Check if the key's access level is equal or higher than the device's access level
    if (key_access_level >= device_access_level) {
        ESP_LOGI(ACCESS_TAG, "key: %s received access to device: %d", key, device);
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_OK;
    } else {
        ESP_LOGI(ACCESS_TAG, "key: %s didn't recieve access to device: %d", key, device);
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }
}

esp_err_t delete_all_keys() {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to delete all keys, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    // Clear the keys file
    if (delete_file_content(KEYACCESSFILENAME) == -1) {
        ESP_LOGE(ACCESS_TAG, "failed to clear keys file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Clear keys in RAM
    for (int i = 0; i < MAX_KEYS; i++) {
        keys[i].key[0] = '\0';
        keys[i].access_level = 0;
    }

    // Update the number of keys
    amount_keys = 0;

    ESP_LOGI(ACCESS_TAG, "successfully deleted all keys");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

esp_err_t delete_all_devices() {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to delete all devices, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    // Clear the devices file
    if (delete_file_content(DEVICEACCESSFILENAME) == -1) {
        ESP_LOGE(ACCESS_TAG, "failed to clear devices file");
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return ESP_FAIL;
    }

    // Clear devices in RAM
    for (int i = 0; i < MAX_DEVICES; i++) {
        devices[i].device = 0;
        devices[i].access_level = 0;
    }

    // Update the number of devices
    amount_devices = 0;

    ESP_LOGI(ACCESS_TAG, "successfully deleted all devices");
    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

int get_keys(char *keys_copy, int *access_levels_copy){
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to get keys, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(keys_copy == NULL || access_levels_copy == NULL){
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return amount_keys;
    }

    for(int i = 0; i < amount_keys; i++){
        strcpy(keys_copy + (i * (KEY_LENGHT + 1)), keys[i].key);
        access_levels_copy[i] = keys[i].access_level;
        //ESP_LOGI(ACCESS_TAG, "key: %s, access level: %d", keys_copy + (i * (KEY_LENGHT + 1)), access_levels_copy[i]);
    }

    // unlock mutex
    xSemaphoreGive(access_mutex);
    return amount_keys;
}

int get_devices(int *devices_copy, int *access_levels_copy){
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to get devices, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    if(devices_copy == NULL || access_levels_copy == NULL){
        // unlock mutex
        xSemaphoreGive(access_mutex);
        return amount_devices;
    }

    for(int i = 0; i < amount_devices; i++){
        devices_copy[i] = devices[i].device;
        access_levels_copy[i] = devices[i].access_level;
    }

    // unlock mutex
    xSemaphoreGive(access_mutex);
    return amount_devices;
}

esp_err_t print_all_data() {
    // lock mutex
    if(access_mutex == NULL){
        ESP_LOGE(ACCESS_TAG, "failed to print all access data, mutex not initialized");
        return ESP_FAIL;
    }
    xSemaphoreTake(access_mutex, portMAX_DELAY);

    ESP_LOGI(ACCESS_TAG, "========== Keys ==========");
    for (int i = 0; i < MAX_KEYS; i++) {
        if (keys[i].key[0] != '\0') {
            ESP_LOGI(ACCESS_TAG, "Key %d: %s, Access Level: %d", i, keys[i].key, keys[i].access_level);
        }
    }

    ESP_LOGI(ACCESS_TAG, "========== Devices ==========");
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].device != 0) {
            ESP_LOGI(ACCESS_TAG, "Device %d: %d, Access Level: %d", i, devices[i].device, devices[i].access_level);
        }
    }

    // unlock mutex
    xSemaphoreGive(access_mutex);
    return ESP_OK;
}

void test_access(){
    pretty_print_file_content(KEYACCESSFILENAME);
    pretty_print_file_content(DEVICEACCESSFILENAME);

    // test adding
    add_key("key00001", 3);
    add_device(45, 2);
    add_key("key00333", 9);
    add_device(33, 2);
    add_key("key00444", 9);
    add_device(99, 2);
    print_all_data();

    // test changing
    change_key_access_level("key00001", 1);
    change_device_access_level(45, 4);
    change_key_access_level("key00333", 8);
    change_device_access_level(33, 8);
    change_key_access_level("key00002", 5);
    change_device_access_level(42, 4);
    print_all_data();

    // test deleting
    delete_key("key00002");
    delete_device(42);
    delete_key("key00333");
    delete_device(33);
    print_all_data();

    // test adding after deleting
    add_key("key00333", 9);
    add_device(33, 2);
    print_all_data();

    // test having access
    has_access(45, "key00333");
    has_access(99, "key00001");

    delete_all_keys();
    delete_all_devices();

    pretty_print_file_content(KEYACCESSFILENAME);
    pretty_print_file_content(DEVICEACCESSFILENAME);
}

void test_access1(){
    pretty_print_file_content(KEYACCESSFILENAME);
    pretty_print_file_content(DEVICEACCESSFILENAME);

    print_all_data();

    delete_all_keys();
    delete_all_devices();

    // test adding
    add_key("key00001", 3);
    add_device(4, 2);
    add_key("key00333", 9);
    add_device(33, 0);
    add_key("key00444", 9);
    add_device(99, 2);
    print_all_data();

    pretty_print_file_content(KEYACCESSFILENAME);
    pretty_print_file_content(DEVICEACCESSFILENAME);
}
