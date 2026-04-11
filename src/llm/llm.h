#pragma once

#include "esp_err.h"
#include "cJSON.h"
#include <stddef.h>

/*
 * Callback fired for each streaming token received from the provider.
 *   token  — NUL-terminated UTF-8 fragment (may be empty string)
 *   len    — byte length of token (excluding NUL)
 *   ctx    — caller-supplied context pointer
 */
typedef void (*llm_token_cb_t)(const char *token, size_t len, void *ctx);

/*
 * Abstract LLM provider.
 * session_open / session_close are optional (may be NULL).
 *   session_open  — called once when a chat session starts; may establish a
 *                   persistent TLS connection to avoid per-request handshakes.
 *   session_close — called when the chat session ends; tears down the connection.
 * stream_chat sends the messages array and calls on_token for every
 * response fragment until the stream ends.
 */
typedef struct {
    const char *name;
    esp_err_t (*session_open)(const char *api_key);    /* optional */
    void      (*session_close)(void);                   /* optional */
    esp_err_t (*stream_chat)(const char     *api_key,
                              const char     *model,
                              const cJSON    *messages,   /* JSON array */
                              llm_token_cb_t  on_token,
                              void           *ctx);
} llm_provider_t;

void             llm_register(const llm_provider_t *provider);
llm_provider_t  *llm_get_provider(const char *name);
