#include "config.h"
#include "encoding/encoding.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *NVS_NS = "cfg";

static struct {
    char ssid    [CFG_SSID_MAX];
    char pass    [CFG_PASS_MAX];
    char provider[CFG_PROVIDER_MAX];
    char api_key [CFG_APIKEY_MAX];
    char model   [CFG_MODEL_MAX];
    char encoding[CFG_ENCODING_MAX];
} s_cfg = {
    .ssid     = "",
    .pass     = "",
    .provider = "openrouter",
    .api_key  = "",
    .model    = "openai/gpt-4o-mini",
    .encoding = "utf-8",
};

/* Read a string from NVS into dst; silently keep default on ESP_ERR_NVS_NOT_FOUND. */
static void nvs_read_str(nvs_handle_t h, const char *key, char *dst, size_t max_len)
{
    size_t len = max_len;
    esp_err_t err = nvs_get_str(h, key, dst, &len);
    (void)err; /* ESP_ERR_NVS_NOT_FOUND keeps the default value */
}

esp_err_t config_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; /* no saved config yet — use compiled-in defaults */
    }
    if (err != ESP_OK) return err;

    nvs_read_str(h, "ssid",     s_cfg.ssid,     sizeof(s_cfg.ssid));
    nvs_read_str(h, "pass",     s_cfg.pass,     sizeof(s_cfg.pass));
    nvs_read_str(h, "provider", s_cfg.provider, sizeof(s_cfg.provider));
    nvs_read_str(h, "api_key",  s_cfg.api_key,  sizeof(s_cfg.api_key));
    nvs_read_str(h, "model",    s_cfg.model,    sizeof(s_cfg.model));
    nvs_read_str(h, "encoding", s_cfg.encoding, sizeof(s_cfg.encoding));

    nvs_close(h);

    /* Apply the loaded encoding immediately. */
    encoding_set(encoding_from_str(s_cfg.encoding));
    return ESP_OK;
}

static esp_err_t nvs_write_str(const char *key, const char *value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

#define SAVE_FIELD(field, key) \
    static esp_err_t config_save_##field(const char *s) { \
        strncpy(s_cfg.field, s, sizeof(s_cfg.field) - 1); \
        s_cfg.field[sizeof(s_cfg.field) - 1] = '\0'; \
        return nvs_write_str(key, s_cfg.field); \
    }

esp_err_t config_save_ssid(const char *s) {
    strncpy(s_cfg.ssid, s, sizeof(s_cfg.ssid) - 1);
    s_cfg.ssid[sizeof(s_cfg.ssid) - 1] = '\0';
    return nvs_write_str("ssid", s_cfg.ssid);
}

esp_err_t config_save_pass(const char *s) {
    strncpy(s_cfg.pass, s, sizeof(s_cfg.pass) - 1);
    s_cfg.pass[sizeof(s_cfg.pass) - 1] = '\0';
    return nvs_write_str("pass", s_cfg.pass);
}

esp_err_t config_save_provider(const char *s) {
    strncpy(s_cfg.provider, s, sizeof(s_cfg.provider) - 1);
    s_cfg.provider[sizeof(s_cfg.provider) - 1] = '\0';
    return nvs_write_str("provider", s_cfg.provider);
}

esp_err_t config_save_api_key(const char *s) {
    strncpy(s_cfg.api_key, s, sizeof(s_cfg.api_key) - 1);
    s_cfg.api_key[sizeof(s_cfg.api_key) - 1] = '\0';
    return nvs_write_str("api_key", s_cfg.api_key);
}

esp_err_t config_save_model(const char *s) {
    strncpy(s_cfg.model, s, sizeof(s_cfg.model) - 1);
    s_cfg.model[sizeof(s_cfg.model) - 1] = '\0';
    return nvs_write_str("model", s_cfg.model);
}

esp_err_t config_save_encoding(const char *s) {
    strncpy(s_cfg.encoding, s, sizeof(s_cfg.encoding) - 1);
    s_cfg.encoding[sizeof(s_cfg.encoding) - 1] = '\0';
    return nvs_write_str("encoding", s_cfg.encoding);
}

const char *config_get_ssid(void)     { return s_cfg.ssid; }
const char *config_get_pass(void)     { return s_cfg.pass; }
const char *config_get_provider(void) { return s_cfg.provider; }
const char *config_get_api_key(void)  { return s_cfg.api_key; }
const char *config_get_model(void)    { return s_cfg.model; }
const char *config_get_encoding(void) { return s_cfg.encoding; }
