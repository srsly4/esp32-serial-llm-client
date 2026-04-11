#include "cli.h"
#include "uart/uart.h"
#include "config/config.h"
#include "wifi/wifi_manager.h"
#include "llm/llm.h"
#include "chat/chat.h"
#include "encoding/encoding.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Forward declarations ────────────────────────────────────────────── */
static void cmd_help    (const char *args);
static void cmd_status  (const char *args);
static void cmd_wifi    (const char *args);
static void cmd_provider(const char *args);
static void cmd_model   (const char *args);
static void cmd_encoding(const char *args);
static void cmd_chat    (const char *args);

/* ── Command table ───────────────────────────────────────────────────── */
typedef struct {
    const char *name;
    void (*handler)(const char *args);
    const char *help;
} cmd_entry_t;

static const cmd_entry_t s_cmds[] = {
    { "help",     cmd_help,
      "Show this help" },
    { "status",   cmd_status,
      "Show WiFi status, provider and model" },
    { "wifi",     cmd_wifi,
      "set <ssid> <pass> | connect | disconnect | status" },
    { "provider", cmd_provider,
      "<name> [api_key]  — e.g. /provider openrouter sk-or-..." },
    { "model",    cmd_model,
      "<model_name>  — e.g. /model openai/gpt-4o-mini" },
    { "encoding", cmd_encoding,
      "[utf-8|cp1250|iso-8859-2]  — serial character encoding" },
    { "chat",     cmd_chat,
      "Start a chat session (/end or Ctrl-D to exit)" },
};

#define CMD_COUNT (sizeof(s_cmds) / sizeof(s_cmds[0]))

/* ── Public functions ────────────────────────────────────────────────── */

void cli_init(void) { /* nothing to initialise */ }

void cli_dispatch(const char *line)
{
    if (!line || line[0] == '\0') return;

    if (line[0] != '/') {
        uart_writeln("Commands must start with /. Type /help for a list.");
        return;
    }

    const char *cmd_start = line + 1;

    /* Find end of command token */
    size_t cmd_len = 0;
    while (cmd_start[cmd_len] && cmd_start[cmd_len] != ' ') cmd_len++;

    /* Arguments start after the first space, or empty string */
    const char *args = (cmd_start[cmd_len] == ' ') ? &cmd_start[cmd_len + 1] : "";

    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (strncmp(cmd_start, s_cmds[i].name, cmd_len) == 0 &&
            strlen(s_cmds[i].name) == cmd_len) {
            s_cmds[i].handler(args);
            return;
        }
    }

    uart_writeln("Unknown command. Type /help.");
}

/* ── Command handlers ────────────────────────────────────────────────── */

static void cmd_help(const char *args)
{
    (void)args;
    uart_writeln("Available commands:");
    char buf[96];
    for (size_t i = 0; i < CMD_COUNT; i++) {
        snprintf(buf, sizeof(buf), "  /%s  -  %s",
                 s_cmds[i].name, s_cmds[i].help);
        uart_writeln(buf);
    }
}

static void cmd_status(const char *args)
{
    (void)args;
    char buf[160];

    snprintf(buf, sizeof(buf), "Provider : %s", config_get_provider());
    uart_writeln(buf);

    snprintf(buf, sizeof(buf), "Model    : %s", config_get_model());
    uart_writeln(buf);

    snprintf(buf, sizeof(buf), "API key  : %s",
             config_get_api_key()[0] ? "[set]" : "[not set]");
    uart_writeln(buf);

    snprintf(buf, sizeof(buf), "Encoding : %s", config_get_encoding());
    uart_writeln(buf);

    wifi_manager_status(buf, sizeof(buf));
    uart_writeln(buf);
}

static void cmd_wifi(const char *args)
{
    if (strncmp(args, "set ", 4) == 0) {
        const char *rest  = args + 4;
        const char *space = strchr(rest, ' ');
        if (!space) {
            uart_writeln("Usage: /wifi set <ssid> <password>");
            return;
        }

        size_t ssid_len = (size_t)(space - rest);
        if (ssid_len == 0 || ssid_len >= CFG_SSID_MAX) {
            uart_writeln("SSID is empty or too long (max 63 chars).");
            return;
        }

        char ssid[CFG_SSID_MAX];
        char pass[CFG_PASS_MAX];
        strncpy(ssid, rest, ssid_len);
        ssid[ssid_len] = '\0';
        strncpy(pass, space + 1, sizeof(pass) - 1);
        pass[sizeof(pass) - 1] = '\0';

        config_save_ssid(ssid);
        config_save_pass(pass);
        uart_writeln("WiFi credentials saved.");

    } else if (strcmp(args, "connect") == 0) {
        uart_writeln("Connecting...");
        esp_err_t err = wifi_manager_connect();
        if (err == ESP_OK) {
            char buf[160];
            wifi_manager_status(buf, sizeof(buf));
            uart_writeln(buf);
        } else if (err == ESP_ERR_INVALID_ARG) {
            uart_writeln("No SSID configured. Use /wifi set <ssid> <pass> first.");
        } else {
            uart_writeln("Connection failed. Check SSID and password.");
        }

    } else if (strcmp(args, "disconnect") == 0) {
        wifi_manager_disconnect();
        uart_writeln("Disconnected.");

    } else if (strcmp(args, "status") == 0) {
        char buf[160];
        wifi_manager_status(buf, sizeof(buf));
        uart_writeln(buf);

    } else {
        uart_writeln("Usage: /wifi set <ssid> <pass> | connect | disconnect | status");
    }
}

static void cmd_provider(const char *args)
{
    if (args[0] == '\0') {
        char buf[48];
        snprintf(buf, sizeof(buf), "Current provider: %s", config_get_provider());
        uart_writeln(buf);
        return;
    }

    /* Format: <provider_name> [api_key] */
    const char *space = strchr(args, ' ');
    char provider[CFG_PROVIDER_MAX];

    if (space) {
        size_t name_len = (size_t)(space - args);
        if (name_len >= sizeof(provider)) {
            uart_writeln("Provider name too long.");
            return;
        }
        strncpy(provider, args, name_len);
        provider[name_len] = '\0';
        config_save_api_key(space + 1);
        uart_writeln("API key saved.");
    } else {
        strncpy(provider, args, sizeof(provider) - 1);
        provider[sizeof(provider) - 1] = '\0';
    }

    config_save_provider(provider);
    char buf[64];
    snprintf(buf, sizeof(buf), "Provider set to: %s", provider);
    uart_writeln(buf);
}

static void cmd_model(const char *args)
{
    if (args[0] == '\0') {
        char buf[CFG_MODEL_MAX + 16];
        snprintf(buf, sizeof(buf), "Current model: %s", config_get_model());
        uart_writeln(buf);
        return;
    }
    config_save_model(args);
    char buf[CFG_MODEL_MAX + 16];
    snprintf(buf, sizeof(buf), "Model set to: %s", args);
    uart_writeln(buf);
}

static void cmd_encoding(const char *args)
{
    if (args[0] == '\0') {
        char buf[CFG_ENCODING_MAX + 24];
        snprintf(buf, sizeof(buf), "Current encoding: %s", config_get_encoding());
        uart_writeln(buf);
        uart_writeln("Options: utf-8, cp1250, iso-8859-2");
        return;
    }

    encoding_t enc = encoding_from_str(args);
    const char *canonical = encoding_to_str(enc);
    encoding_set(enc);
    config_save_encoding(canonical);

    char buf[CFG_ENCODING_MAX + 24];
    snprintf(buf, sizeof(buf), "Encoding set to: %s", canonical);
    uart_writeln(buf);
}

static void cmd_chat(const char *args)
{
    (void)args;
    if (!wifi_manager_is_connected()) {
        uart_writeln("Warning: WiFi not connected. "
                     "Connect first with /wifi connect.");
    }
    chat_start();
}
