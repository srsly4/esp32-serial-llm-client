#include "uart.h"
#include "cli/cli.h"
#include "encoding/encoding.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#define UART_PORT       UART_PORT_NUM
#define UART_BAUD_RATE  9600
#define UART_RX_BUF    1024

/* Printed once after init */
#define BANNER \
    "\r\n" \
    "====================================\r\n" \
    "  Vintage Serial LLM  (ESP32)\r\n" \
    "  Type /help for commands\r\n" \
    "====================================\r\n"

void uart_init(void)
{
    /* Suppress all IDF log output so it doesn't pollute the serial line. */
    esp_log_level_set("*", ESP_LOG_NONE);

    uart_config_t cfg = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, UART_PIN_TX, UART_PIN_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT, UART_RX_BUF, 0, 0, NULL, 0);
}

void uart_write(const char *buf, size_t len)
{
    if (buf && len > 0) {
        uart_write_bytes(UART_PORT, buf, len);
    }
}

void uart_writeln(const char *msg)
{
    if (msg && msg[0]) {
        uart_write_bytes(UART_PORT, msg, strlen(msg));
    }
    uart_write_bytes(UART_PORT, "\r\n", 2);
}

/* Write buf, replacing bare \n with \r\n for Windows 9x terminals. */
static void write_crlf(const char *buf, size_t len)
{
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r')) {
            if (i > start) {
                uart_write_bytes(UART_PORT, buf + start, i - start);
            }
            uart_write_bytes(UART_PORT, "\r\n", 2);
            start = i + 1;
        }
    }
    if (start < len) {
        uart_write_bytes(UART_PORT, buf + start, len - start);
    }
}

void uart_write_text(const char *utf8, size_t len)
{
    if (!utf8 || len == 0) return;

    if (encoding_get() == ENCODING_UTF8) {
        write_crlf(utf8, len);
        return;
    }

    /* Output is always <= input length, so a same-sized heap buffer suffices. */
    char *buf = malloc(len);
    if (!buf) {
        write_crlf(utf8, len); /* fallback — send raw but still fix newlines */
        return;
    }
    size_t out_len = encoding_utf8_to_target(utf8, len, buf, len);
    write_crlf(buf, out_len);
    free(buf);
}

int uart_read_line(char *buf, size_t max_len)
{
    /*
     * Accumulate raw bytes from the terminal into a separate buffer.
     * In single-byte encodings (CP1250 / ISO-8859-2) every character is
     * exactly one byte, so backspace handling stays simple.
     * After the full line is received, transcode to UTF-8 for the caller.
     */
    uint8_t raw[UART_LINE_MAX];
    size_t  raw_pos = 0;
    uint8_t c;

    while (1) {
        if (uart_read_bytes(UART_PORT, &c, 1, portMAX_DELAY) <= 0) {
            continue;
        }

        if (c == 0x04) {                        /* Ctrl-D — end of input */
            uart_write_bytes(UART_PORT, "\r\n", 2);
            return -1;
        }

        if (c == '\r' || c == '\n') {           /* Enter — end of line */
            uart_write_bytes(UART_PORT, "\r\n", 2);
            break;
        }

        if ((c == 0x08 || c == 0x7F) && raw_pos > 0) {   /* Backspace / DEL */
            raw_pos--;
            uart_write_bytes(UART_PORT, "\x08 \x08", 3);
            continue;
        }

        if (c < 0x20) continue;                 /* Ignore other control chars */

        if (raw_pos < UART_LINE_MAX - 1) {
            raw[raw_pos++] = c;
            uart_write_bytes(UART_PORT, &c, 1); /* Local echo (raw byte) */
        }
    }

    /* Transcode the raw line to UTF-8;  NUL-terminate the result. */
    size_t out = encoding_raw_to_utf8(raw, raw_pos, buf, max_len);
    buf[out] = '\0';
    return (int)out;
}

void uart_task(void *arg)
{
    uart_init();
    uart_write(BANNER, strlen(BANNER));
    cli_init();

    char line[UART_LINE_MAX];
    while (1) {
        uart_write("> ", 2);
        int len = uart_read_line(line, sizeof(line));
        if (len < 0) {
            uart_writeln("(Ctrl-D outside of chat has no effect. Use /chat to start a session.)");
            continue;
        }
        if (len == 0) continue;
        cli_dispatch(line);
    }
}
