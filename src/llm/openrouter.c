#include "openrouter.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define OPENROUTER_URL      "https://openrouter.ai/api/v1/chat/completions"
#define HTTP_READ_BUF_SIZE  1024
#define SSE_LINE_BUF_SIZE   4096

/* Parse one complete SSE line and fire on_token for any content fragment. */
static void process_sse_line(const char *line, llm_token_cb_t on_token, void *ctx)
{
    if (strncmp(line, "data: ", 6) != 0) return;

    const char *json_str = line + 6;
    if (strcmp(json_str, "[DONE]") == 0) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (choices && cJSON_IsArray(choices)) {
        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        if (choice) {
            cJSON *delta = cJSON_GetObjectItem(choice, "delta");
            if (delta) {
                cJSON *content = cJSON_GetObjectItem(delta, "content");
                if (content && cJSON_IsString(content) && content->valuestring) {
                    size_t len = strlen(content->valuestring);
                    if (len > 0) {
                        on_token(content->valuestring, len, ctx);
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
}

static esp_err_t openrouter_stream_chat(const char     *api_key,
                                         const char     *model,
                                         const cJSON    *messages,
                                         llm_token_cb_t  on_token,
                                         void           *ctx)
{
    if (!api_key || api_key[0] == '\0') return ESP_ERR_INVALID_ARG;
    if (!model   || model[0]   == '\0') return ESP_ERR_INVALID_ARG;

    /* Build JSON request body using a deep copy of messages so we own it. */
    cJSON *body_json = cJSON_CreateObject();
    if (!body_json) return ESP_ERR_NO_MEM;

    cJSON_AddStringToObject(body_json, "model", model);

    cJSON *messages_copy = cJSON_Duplicate((cJSON *)messages, 1);
    if (!messages_copy) {
        cJSON_Delete(body_json);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(body_json, "messages", messages_copy);
    cJSON_AddBoolToObject(body_json, "stream", cJSON_True);

    char *body = cJSON_PrintUnformatted(body_json);
    cJSON_Delete(body_json);
    if (!body) return ESP_ERR_NO_MEM;

    int body_len = (int)strlen(body);

    /* Build Authorization header value */
    size_t auth_len = strlen("Bearer ") + strlen(api_key) + 1;
    char  *auth_header = malloc(auth_len);
    if (!auth_header) {
        free(body);
        return ESP_ERR_NO_MEM;
    }
    snprintf(auth_header, auth_len, "Bearer %s", api_key);

    /* Configure HTTP client */
    esp_http_client_config_t http_cfg = {
        .url               = OPENROUTER_URL,
        .method            = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 60000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        free(auth_header);
        free(body);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type",  "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "HTTP-Referer",
                               "https://github.com/vintage-serial-llm");

    esp_err_t err = esp_http_client_open(client, body_len);
    if (err != ESP_OK) goto cleanup_client;

    if (esp_http_client_write(client, body, body_len) < 0) {
        err = ESP_FAIL;
        goto cleanup_close;
    }

    free(body);
    body = NULL;
    free(auth_header);
    auth_header = NULL;

    esp_http_client_fetch_headers(client);

    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) {
        err = ESP_FAIL;
        goto cleanup_close;
    }

    /* Allocate SSE parsing buffers on heap to avoid blowing the task stack. */
    char *line_buf = malloc(SSE_LINE_BUF_SIZE);
    char *read_buf = malloc(HTTP_READ_BUF_SIZE);
    if (!line_buf || !read_buf) {
        free(line_buf);
        free(read_buf);
        err = ESP_ERR_NO_MEM;
        goto cleanup_close;
    }

    int line_pos = 0;
    int read_len;

    while ((read_len = esp_http_client_read(client, read_buf,
                                             HTTP_READ_BUF_SIZE)) > 0) {
        for (int i = 0; i < read_len; i++) {
            char c = read_buf[i];
            if (c == '\n') {
                /* Strip trailing CR if present */
                if (line_pos > 0 && line_buf[line_pos - 1] == '\r') {
                    line_pos--;
                }
                line_buf[line_pos] = '\0';
                if (line_pos > 0) {
                    process_sse_line(line_buf, on_token, ctx);
                }
                line_pos = 0;
            } else if (line_pos < SSE_LINE_BUF_SIZE - 1) {
                line_buf[line_pos++] = c;
            }
        }
    }

    free(line_buf);
    free(read_buf);
    err = ESP_OK;

cleanup_close:
    esp_http_client_close(client);
cleanup_client:
    esp_http_client_cleanup(client);
    free(auth_header);
    free(body);
    return err;
}

const llm_provider_t openrouter_provider = {
    .name        = "openrouter",
    .stream_chat = openrouter_stream_chat,
};
