#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "config/config.h"
#include "uart/uart.h"
#include "wifi/wifi_manager.h"
#include "llm/llm.h"
#include "llm/openrouter.h"

/* UART task stack — needs headroom for TLS operations during LLM streaming */
#define UART_TASK_STACK_SIZE (20 * 1024)
#define UART_TASK_PRIORITY   5

void app_main(void)
{
    /* Silence all IDF log output before anything writes to UART0. */
    esp_log_level_set("*", ESP_LOG_WARN);

    /* Initialise NVS — erase and reinitialise if partition is bad. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    config_load();
    wifi_manager_init();
    llm_register(&openrouter_provider);

    /* All user interaction runs in the UART task. */
    xTaskCreate(uart_task, "uart_task", UART_TASK_STACK_SIZE, NULL,
                UART_TASK_PRIORITY, NULL);
}