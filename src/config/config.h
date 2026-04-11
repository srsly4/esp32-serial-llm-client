#pragma once

#include "esp_err.h"

#define CFG_SSID_MAX      64
#define CFG_PASS_MAX      64
#define CFG_PROVIDER_MAX  32
#define CFG_APIKEY_MAX    256
#define CFG_MODEL_MAX     128
#define CFG_ENCODING_MAX  16

esp_err_t   config_load(void);

esp_err_t   config_save_ssid(const char *s);
esp_err_t   config_save_pass(const char *s);
esp_err_t   config_save_provider(const char *s);
esp_err_t   config_save_api_key(const char *s);
esp_err_t   config_save_model(const char *s);
esp_err_t   config_save_encoding(const char *s);

const char *config_get_ssid(void);
const char *config_get_pass(void);
const char *config_get_provider(void);
const char *config_get_api_key(void);
const char *config_get_model(void);
const char *config_get_encoding(void);
