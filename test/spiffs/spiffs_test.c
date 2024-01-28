//
// Created by Javad, Vincent.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "../main/spiffs/spiffs.h"

void test_write_new_line(void)
{
    // Create a temporary file path for testing
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);
    delete_file_content(filepath);

    TEST_ASSERT_EQUAL(ESP_OK, write_new_line(filepath, "Hello, world!"));

    TEST_ASSERT_EQUAL(1, count_lines(filepath));
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_delete_first_line(void)
{
    // Create a temporary file path for testing
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "First line");
    write_new_line(filepath, "Second line");
    write_new_line(filepath, "Third line");

    TEST_ASSERT_EQUAL(ESP_OK, delete_first_line(filepath));

    TEST_ASSERT_EQUAL(2, count_lines(filepath));
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_read_line_from_file()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "First line");
    write_new_line(filepath, "Second line");

    char buffer[256];

    read_line_from_file(filepath, 1, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("First line\n", buffer);
    read_line_from_file(filepath, 2, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("Second line\n", buffer);
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_delete_line_from_file()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "First line");
    write_new_line(filepath, "Second line"); 

    TEST_ASSERT_EQUAL(2, count_lines(filepath));

    TEST_ASSERT_EQUAL(ESP_OK, delete_line_from_file(filepath, 1));

    TEST_ASSERT_EQUAL(1, count_lines(filepath));    
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_delete_lines_from_file()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "line0");
    write_new_line(filepath, "line1");
    write_new_line(filepath, "line2");
    write_new_line(filepath, "line3");
    write_new_line(filepath, "line4");
    write_new_line(filepath, "line5");

    TEST_ASSERT_EQUAL(6, count_lines(filepath));

    TEST_ASSERT_EQUAL(ESP_OK, delete_lines_from_file(filepath, 2, 4));

    TEST_ASSERT_EQUAL(3, count_lines(filepath));
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_overwrite_line_in_file()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "line0");
    write_new_line(filepath, "line1");
    write_new_line(filepath, "line2");

    TEST_ASSERT_EQUAL(ESP_OK, overwrite_line_in_file(filepath, 1, "new line1"));

    char buffer[256];

    read_line_from_file(filepath, 1, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("new line1\n", buffer); 
    delete_file_content(filepath);
    delete_file_if_exists(filepath);   
}

void test_delete_file_content()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    write_new_line(filepath, "line0");
    write_new_line(filepath, "line1");
    write_new_line(filepath, "line2");

    TEST_ASSERT_EQUAL(ESP_OK, delete_file_content(filepath));

    TEST_ASSERT_EQUAL(0, count_lines(filepath));
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_create_file_if_not_exists()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    TEST_ASSERT_EQUAL(ESP_OK, create_file_if_not_exists(filepath));
    delete_file_content(filepath);
    delete_file_if_exists(filepath);
}

void test_delete_file_if_exists()
{
    const char *filepath = "/spiffs/test_file.txt";
    create_file_if_not_exists(filepath);

    TEST_ASSERT_EQUAL(ESP_OK, delete_file_if_exists(filepath));
   
}