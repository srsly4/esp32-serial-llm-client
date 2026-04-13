# Vintage Serial LLM

A hobby project that lets retro PCs and vintage computers chat with large language models over an RS-232 serial connection — no direct internet access required on the old machine. An ESP32 sits between them, handling Wi-Fi, the LLM API calls, and character encoding translation.

```
Retro PC  ──RS-232──  ESP32  ──WiFi──  OpenRouter  ──  LLM
```

## Hardware

- Tested on an **MH ET LIVE ESP32 DevKit** (any standard ESP32 board should work)
- RS-232 level shifter (e.g. MAX3232) between the ESP32 and the vintage computer's serial port

### Serial ports

| Mode | UART | TX | RX | Notes |
|------|------|----|----|-------|
| Default (RS-232) | UART2 | GPIO 17 | GPIO 16 | Use with a level shifter |
| USB / testing | UART0 | GPIO 1 | GPIO 3 | Same pins as USB-Serial; set `UART_USE_USB_PORT=1` |

Baud rate: **9600**, 8N1, no flow control.

## Features

- Converse with any LLM available on [OpenRouter](https://openrouter.ai) from any terminal emulator
- Wi-Fi credentials and API token survive reboots (stored in NVS flash)
- Character encoding translation: **UTF-8**, **CP1250**, **ISO-8859-2** — so accented characters display correctly on old code-page terminals
- Pluggable LLM provider interface — easy to add providers beyond OpenRouter

## Building & Flashing

Requires [PlatformIO](https://platformio.org/).

```bash
pio run -t upload
```

To monitor via USB instead of the RS-232 port, build with the `UART_USE_USB_PORT` flag:

```ini
; platformio.ini
build_flags = -D UART_USE_USB_PORT=1
```

## First-Time Setup

Connect a terminal to the ESP32's serial port at **9600 baud**. After reset you will see:

```
====================================
  Vintage Serial LLM  (ESP32)
  Type /help for commands
====================================
```

### 1. Configure Wi-Fi

```
/wifi set <ssid> <password>
/wifi connect
```

### 2. Set the LLM provider and API key

```
/provider openrouter <api_key>
```

Get a free API key at <https://openrouter.ai/keys>.

### 3. Choose a model

```
/model openai/gpt-4o-mini
```

Any model slug from OpenRouter works.

### 4. (Optional) Set character encoding

```
/encoding cp1250
```

Available options: `utf-8` (default), `cp1250`, `iso-8859-2`.

### 5. Start chatting

```
/chat
```

Type your messages and press Enter. To end the session type `/end` or press **Ctrl-D**.

## Command Reference (`/help`)

| Command | Arguments | Description |
|---------|-----------|-------------|
| `/help` | — | Show all available commands |
| `/status` | — | Show current Wi-Fi status, provider and model |
| `/wifi` | `set <ssid> <pass>` \| `connect` \| `disconnect` \| `status` | Manage Wi-Fi |
| `/provider` | `<name> [api_key]` | Select provider and store API key |
| `/model` | `<model_name>` | Select the LLM model |
| `/encoding` | `[utf-8\|cp1250\|iso-8859-2]` | Set serial character encoding |
| `/chat` | — | Start an interactive chat session |

## Adding a New LLM Provider

Implement the interface declared in `src/llm/llm.h` and register the provider alongside `openrouter` in `src/llm/llm.c`. The OpenRouter implementation in `src/llm/openrouter.c` is a good starting point.
