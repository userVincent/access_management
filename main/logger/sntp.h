//
// Created by Vincent.
//

#ifndef SNTP_H
#define SNTP_H

#include "esp_err.h"

#define SNTP_TAG "SNTP"

esp_err_t set_time_using_sntp(void);

#endif //SNTP_H