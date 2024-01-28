//
// Created by Vincent.
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
#include "spiffs.h"
#include "esp_vfs.h"
#include <dirent.h>

static esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

esp_err_t init_spiffs(){
    ESP_LOGI(SPIFFS_TAG, "Initializing SPIFFS");

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SPIFFS_TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(SPIFFS_TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(SPIFFS_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

#ifdef CONFIG_EXAMPLE_SPIFFS_CHECK_ON_START
    ESP_LOGI(SPIFFS_TAG, "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(SPIFFS_TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return ret;
    } else {
        ESP_LOGI(SPIFFS_TAG, "SPIFFS_check() successful");
    }
#endif

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(SPIFFS_TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return ret;
    } else {
        ESP_LOGI(SPIFFS_TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partiton size info.
    if (used > total) {
        ESP_LOGW(SPIFFS_TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(SPIFFS_TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return ret;
        } else {
            ESP_LOGI(SPIFFS_TAG, "SPIFFS_check() successful");
        }
    }

    return ESP_OK;
}

esp_err_t unregister_spiffs(){
    // unmount partition and disable SPIFFS
    esp_err_t ret = esp_vfs_spiffs_unregister(conf.partition_label);
    if(ret != ESP_OK){
        ESP_LOGE(SPIFFS_TAG, "Failed to unregister SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(SPIFFS_TAG, "SPIFFS unmounted");
    return ESP_OK;
}

esp_err_t write_new_line(const char *filename, const char *text) {
    FILE *file = fopen(filename, "a");

    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    fprintf(file, "%s\n", text);

    fclose(file);

    ESP_LOGI(SPIFFS_TAG, "New line written to %s", filename);

    return ESP_OK;
}

esp_err_t delete_first_line(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    // Generate a random temporary file name
    srand((unsigned) time(NULL));
    int random_number = rand();
    char temp_filename[42];
    snprintf(temp_filename, sizeof(temp_filename), "/spiffs/temp_%d.txt", random_number);

    // Create a temporary file to store the content without the first line
    FILE *temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open temp file for writing");
        fclose(file);
        return ESP_FAIL;
    }

    bool skip_first_line = true;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file)) {
        if (skip_first_line) {
            skip_first_line = false;
            continue;
        }
        fputs(buffer, temp_file);
    }

    fclose(file);
    fclose(temp_file);

    // Remove the original file
    if (unlink(filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to remove the original file");
        unlink(temp_filename);
        return ESP_FAIL;
    }

    // Rename the temporary file to the original file name
    if (rename(temp_filename, filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to rename the temp file");
        return ESP_FAIL;
    } else {
        ESP_LOGI(SPIFFS_TAG, "First line deleted from %s", filename);
        return ESP_OK;
    }
    
}

esp_err_t read_line_from_file(const char *filename, int line_number, char *buffer, size_t buffer_size) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    int current_line = 0;
    esp_err_t found = ESP_FAIL;

    while (fgets(buffer, buffer_size, file) != NULL) {
        current_line++;
        if (current_line == line_number) {
            found = ESP_OK;
            break;
        }
    }

    fclose(file);
    return found;
}

esp_err_t delete_line_from_file(const char *filename, int line_number) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    // Generate a random temporary file name
    srand((unsigned) time(NULL));
    int random_number = rand();
    char temp_filename[42];
    snprintf(temp_filename, sizeof(temp_filename), "/spiffs/temp_%d.txt", random_number);

    FILE *temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to create temporary file");
        fclose(file);
        return ESP_FAIL;
    }

    char buffer[256];
    int current_line = 0;
    esp_err_t deleted = ESP_FAIL;

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        current_line++;
        if (current_line != line_number) {
            fputs(buffer, temp_file);
        } else {
            deleted = ESP_OK;
        }
    }

    fclose(file);
    fclose(temp_file);

    if (deleted == ESP_OK) {
        if (remove(filename) != 0) {
            ESP_LOGE(SPIFFS_TAG, "Failed to remove the original file");
            remove(temp_filename); // Clean up the temporary file
            return ESP_FAIL;
        }

        if (rename(temp_filename, filename) != 0) {
            ESP_LOGE(SPIFFS_TAG, "Failed to rename the temporary file");
            return ESP_FAIL;
        }
    } else {
        remove(temp_filename); // Clean up the temporary file if the line was not found
    }

    return deleted;
}

esp_err_t delete_lines_from_file(const char *filename, int start_line, int end_line) {
    if (start_line > end_line) {
        ESP_LOGE(SPIFFS_TAG, "Invalid line range: start line should be less than or equal to end line");
        return ESP_FAIL;
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    // Generate a random temporary file name
    srand((unsigned) time(NULL));
    int random_number = rand();
    char temp_filename[42];
    snprintf(temp_filename, sizeof(temp_filename), "/spiffs/temp_%d.txt", random_number);

    FILE *temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open temporary file");
        fclose(file);
        return ESP_FAIL;
    }

    int current_line = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        current_line++;

        if (current_line < start_line || current_line > end_line) {
            fputs(buffer, temp_file);
        }
    }

    fclose(file);
    fclose(temp_file);

    if (remove(filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to remove the original file: %s", filename);
        remove(temp_filename);
        return ESP_FAIL;
    }

    if (rename(temp_filename, filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to rename the temporary file to the original file");
        return ESP_FAIL;
    }

    ESP_LOGI(SPIFFS_TAG, "Successfully deleted lines %d to %d in file: %s", start_line, end_line, filename);
    return ESP_OK;
}

esp_err_t overwrite_line_in_file(const char *filename, int line_number, const char *new_line) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    // Generate a random temporary file name
    srand((unsigned) time(NULL));
    int random_number = rand();
    char temp_filename[42];
    snprintf(temp_filename, sizeof(temp_filename), "/spiffs/temp_%d.txt", random_number);


    FILE *temp_file = fopen(temp_filename, "w");
    if (temp_file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open temporary file");
        fclose(file);
        return ESP_FAIL;
    }

    int current_line = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        current_line++;

        if (current_line == line_number) {
            fprintf(temp_file, "%s\n", new_line);
        } else {
            fprintf(temp_file, "%s", buffer);
        }
    }

    fclose(file);
    fclose(temp_file);

    if (remove(filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to remove the original file: %s", filename);
        remove(temp_filename);
        return ESP_FAIL;
    }

    if (rename(temp_filename, filename) != 0) {
        ESP_LOGE(SPIFFS_TAG, "Failed to rename the temporary file to the original file");
        return ESP_FAIL;
    }

    ESP_LOGI(SPIFFS_TAG, "Successfully overwritten line %d in file: %s", line_number, filename);
    return ESP_OK;
}

int count_lines(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for reading");
        return -1;
    }

    int line_count = 0;
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), file)) {
        line_count++;
    }

    fclose(file);

    return line_count;
}

esp_err_t delete_file_content(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    fclose(file);

    ESP_LOGI(SPIFFS_TAG, "Content of %s deleted", filename);
    return ESP_OK;
}

esp_err_t create_file_if_not_exists(const char *filename) {
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open or create file");
        return ESP_FAIL;
    }

    fclose(file);

    ESP_LOGI(SPIFFS_TAG, "File %s created if it didn't exist", filename);

    return ESP_OK;
}

esp_err_t delete_file_if_exists(const char *filename) {
    // Check if the file exists
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        // File exists, close and remove it
        fclose(file);
        if (unlink(filename) == 0) {
            ESP_LOGI(SPIFFS_TAG, "File %s deleted", filename);
            return ESP_OK;
        } else {
            ESP_LOGE(SPIFFS_TAG, "Failed to delete file %s", filename);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGI(SPIFFS_TAG, "File %s does not exist", filename);
        return ESP_OK;
    }
}

esp_err_t pretty_print_file_content(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    char buffer[255];
    size_t line_number = 1;

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character
        ESP_LOGI(SPIFFS_TAG, "Line %zu: %s", line_number, buffer);
        line_number++;
    }

    fclose(file);
    return ESP_OK;
}

esp_err_t clear_spiffs() {
    ESP_LOGI(SPIFFS_TAG, "Clearing SPIFFS...");

    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t spiffs_config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_config);

    if (ret != ESP_OK) {
        ESP_LOGE(SPIFFS_TAG, "Failed to initialize SPIFFS, error: %s", esp_err_to_name(ret));
        return ret;
    }

    // Open directory
    DIR *dir;
    struct dirent *entry;
    dir = opendir("/spiffs");

    if (!dir) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open directory");
        esp_vfs_spiffs_unregister(NULL);
        return ESP_FAIL;
    }

    // Iterate through files and remove them
    while ((entry = readdir(dir)) != NULL) {
        char file_path[128];
        snprintf(file_path, sizeof(file_path), "/spiffs/%.*s", (int)(sizeof(file_path) - 9), entry->d_name);
        ESP_LOGI(SPIFFS_TAG, "Removing file: %s", file_path);
        if (unlink(file_path) != 0) {
            ESP_LOGE(SPIFFS_TAG, "Failed to remove file: %s", file_path);
        }
    }

    // Close directory
    closedir(dir);

    // Deinitialize SPIFFS
    esp_vfs_spiffs_unregister(NULL);

    ESP_LOGI(SPIFFS_TAG, "SPIFFS cleared successfully");
    return ESP_OK;
}

void spiffs_test(){
    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(SPIFFS_TAG, "Opening file");
    FILE* f = fopen("/spiffs/hello.txt", "w");
    if (f == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello World!\n");
    fclose(f);
    ESP_LOGI(SPIFFS_TAG, "File written");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/spiffs/foo.txt", &st) == 0) {
        // Delete it if it exists
        unlink("/spiffs/foo.txt");
    }

    // Rename original file
    ESP_LOGI(SPIFFS_TAG, "Renaming file");
    if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
        ESP_LOGE(SPIFFS_TAG, "Rename failed");
        return;
    }

    // Open renamed file for reading
    ESP_LOGI(SPIFFS_TAG, "Reading file");
    f = fopen("/spiffs/foo.txt", "r");
    if (f == NULL) {
        ESP_LOGE(SPIFFS_TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char* pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(SPIFFS_TAG, "Read from file: '%s'", line);

    // All done, unmount partition and disable SPIFFS
    unregister_spiffs();
}