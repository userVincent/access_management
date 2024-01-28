//
// Created by Vincent.
//

#ifndef SPIFFS_H
#define SPIFFS_H

#define SPIFFS_TAG "SPIFFS"

esp_err_t init_spiffs();

esp_err_t unregister_spiffs();

esp_err_t write_new_line(const char *filename, const char *text);

esp_err_t delete_first_line(const char *filename);

esp_err_t read_line_from_file(const char *filename, int line_number, char *buffer, size_t buffer_size);

esp_err_t delete_line_from_file(const char *filename, int line_number);

esp_err_t delete_lines_from_file(const char *filename, int start_line, int end_line);

esp_err_t overwrite_line_in_file(const char *filename, int line_number, const char *new_line);

int count_lines(const char *filename);

esp_err_t delete_file_content(const char *filename);

esp_err_t create_file_if_not_exists(const char *filename);

esp_err_t delete_file_if_exists(const char *filename);

esp_err_t clear_spiffs();

esp_err_t pretty_print_file_content(const char *filename);

#endif //SPIFFS_H