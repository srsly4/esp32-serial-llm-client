#pragma once

#include "esp_err.h"
#include <stddef.h>

void      wifi_manager_init(void);
esp_err_t wifi_manager_connect(void);       /* Blocks up to ~10 s */
void      wifi_manager_disconnect(void);
void      wifi_manager_status(char *buf, size_t len);
int       wifi_manager_is_connected(void);
