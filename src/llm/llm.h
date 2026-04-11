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
 * Per-request timing breakdown filled in by the provider.
 * All values are in milliseconds; 0 means the stage was not reached.
 *   connect_ms — TCP connect + TLS handshake (esp_http_client_open)
 *   server_ms  — request sent → first streaming token received
 *   stream_ms  — first token → last token (streaming body)
 */
typedef struct {
    int32_t connect_ms;
    int32_t server_ms;
    int32_t stream_ms;
} llm_timing_t;

/*
 * Abstract LLM provider.
 * stream_chat sends the messages array and calls on_token for every
 * response fragment until the stream ends.
 */
typedef struct {
    const char *name;
    esp_err_t (*stream_chat)(const char     *api_key,
                              const char     *model,
                              const cJSON    *messages,   /* JSON array */
                              llm_token_cb_t  on_token,
                              void           *ctx,
                              llm_timing_t   *timing);    /* out, may be NULL */
} llm_provider_t;

void             llm_register(const llm_provider_t *provider);
llm_provider_t  *llm_get_provider(const char *name);
