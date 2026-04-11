#include "wifi_manager.h"
#include "config/config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdio.h>

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRIES    3
#define WIFI_TIMEOUT_MS     10000

static EventGroupHandle_t s_wifi_events       = NULL;
static esp_netif_t       *s_sta_netif          = NULL;
static int                s_retry_count        = 0;
static bool               s_connected          = false;
static bool               s_started            = false;
static bool               s_handlers_registered = false;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (!s_wifi_events) return;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_count < WIFI_MAX_RETRIES) {
            esp_wifi_connect();
            s_retry_count++;
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAIL_BIT);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        s_connected   = true;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    s_wifi_events = xEventGroupCreate();
}

esp_err_t wifi_manager_connect(void)
{
    const char *ssid = config_get_ssid();
    if (ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    /* Stop any previous session */
    if (s_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_started   = false;
        s_connected = false;
    }

    /* Register event handlers once */
    if (!s_handlers_registered) {
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                   wifi_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                   wifi_event_handler, NULL);
        s_handlers_registered = true;
    }

    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_count = 0;

    wifi_config_t wifi_cfg = {};
    strncpy((char *)wifi_cfg.sta.ssid,     config_get_ssid(),
            sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, config_get_pass(),
            sizeof(wifi_cfg.sta.password) - 1);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    s_started = true;

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) return ESP_OK;
    return ESP_FAIL;
}

void wifi_manager_disconnect(void)
{
    if (s_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_started   = false;
        s_connected = false;
    }
}

void wifi_manager_status(char *buf, size_t len)
{
    if (s_connected) {
        esp_netif_ip_info_t ip_info = {};
        esp_netif_get_ip_info(s_sta_netif, &ip_info);
        snprintf(buf, len, "WiFi: connected to \"%s\", IP: " IPSTR,
                 config_get_ssid(), IP2STR(&ip_info.ip));
    } else {
        snprintf(buf, len, "WiFi: disconnected");
    }
}

int wifi_manager_is_connected(void)
{
    return s_connected ? 1 : 0;
}
