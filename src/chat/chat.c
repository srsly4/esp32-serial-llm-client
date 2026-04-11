#include "chat.h"
#include "uart/uart.h"
#include "config/config.h"
#include "llm/llm.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define CHAT_LINE_MAX  512
#define CHAT_RESP_MAX  8192

/* Context passed to the on_token callback */
typedef struct {
    char  *buf;
    size_t pos;
    size_t cap;
} resp_ctx_t;

static void on_token(const char *token, size_t len, void *ctx_ptr)
{
    /* Stream token to serial, transcoding UTF-8 → configured encoding. */
    uart_write_text(token, len);

    /* Also accumulate for history (truncate silently if full) */
    resp_ctx_t *c = (resp_ctx_t *)ctx_ptr;
    if (c->pos + len < c->cap - 1) {
        memcpy(c->buf + c->pos, token, len);
        c->pos += len;
    }
}

void chat_start(void)
{
    llm_provider_t *provider = llm_get_provider(config_get_provider());
    if (!provider) {
        uart_writeln("Error: unknown provider. Use /provider <name> [api_key]");
        return;
    }
    if (config_get_api_key()[0] == '\0') {
        uart_writeln("Error: no API key set. Use /provider <name> <api_key>");
        return;
    }

    if (provider->session_open) {
        uart_writeln("Connecting...");
        if (provider->session_open(config_get_api_key()) != ESP_OK) {
            uart_writeln("Error: failed to connect. Check WiFi and API key.");
            return;
        }
    }

    uart_writeln("Chat started. Type /end or Ctrl-D to exit.");

    cJSON *history = cJSON_CreateArray();
    if (!history) {
        uart_writeln("Error: out of memory.");
        if (provider->session_close) provider->session_close();
        return;
    }

    char *line     = malloc(CHAT_LINE_MAX);
    char *resp_buf = malloc(CHAT_RESP_MAX);
    if (!line || !resp_buf) {
        uart_writeln("Error: out of memory.");
        free(line);
        free(resp_buf);
        cJSON_Delete(history);
        if (provider->session_close) provider->session_close();
        return;
    }

    while (1) {
        uart_write("> ", 2);
        int len = uart_read_line(line, CHAT_LINE_MAX);

        if (len < 0) break;                      /* Ctrl-D */
        if (strcmp(line, "/end") == 0) break;
        if (len == 0) continue;

        /* Append user turn to history */
        cJSON *user_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", line);
        cJSON_AddItemToArray(history, user_msg);

        /* Stream the response token by token */
        resp_buf[0] = '\0';
        resp_ctx_t rctx = { .buf = resp_buf, .pos = 0, .cap = CHAT_RESP_MAX };

        uart_writeln("");   /* blank line before response */

        esp_err_t err = provider->stream_chat(
            config_get_api_key(),
            config_get_model(),
            history,
            on_token,
            &rctx
        );

        uart_writeln("");   /* blank line after response */

        if (err != ESP_OK) {
            uart_writeln("Error: request failed. Check WiFi connection and API key.");
            /* Drop the unanswered user turn from history */
            cJSON_DeleteItemFromArray(history, cJSON_GetArraySize(history) - 1);
            continue;
        }

        /* Append assistant turn to history */
        resp_buf[rctx.pos] = '\0';
        cJSON *asst_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(asst_msg, "role", "assistant");
        cJSON_AddStringToObject(asst_msg, "content", resp_buf);
        cJSON_AddItemToArray(history, asst_msg);
    }

    free(line);
    free(resp_buf);
    cJSON_Delete(history);
    if (provider->session_close) provider->session_close();
    uart_writeln("Chat ended.");
}
