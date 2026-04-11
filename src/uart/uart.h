#pragma once

#include <stddef.h>

/* Maximum bytes in a single input line (including NUL). */
#define UART_LINE_MAX 512

/*
 * Serial port selection.
 * Set UART_USE_USB_PORT to 1 to use UART0 (GPIO1/TX, GPIO3/RX, same as USB)
 * for testing without a physical RS-232 adapter.
 * Leave at 0 (default) to use UART2 on GPIO16 (RX) / GPIO17 (TX).
 */
#ifndef UART_USE_USB_PORT
#  define UART_USE_USB_PORT 0
#endif

#if UART_USE_USB_PORT
#  define UART_PORT_NUM   UART_NUM_0
#  define UART_PIN_TX     1
#  define UART_PIN_RX     3
#else
#  define UART_PORT_NUM   UART_NUM_2
#  define UART_PIN_TX     17
#  define UART_PIN_RX     16
#endif

void uart_init(void);
void uart_write(const char *buf, size_t len);
void uart_writeln(const char *msg);          /* msg + CR LF */

/*
 * Write text that came from the LLM (UTF-8) to the serial port,
 * transcoding to the currently configured encoding on the fly.
 */
void uart_write_text(const char *utf8, size_t len);

/*
 * Blocking line-read with local echo and backspace handling.
 * The returned buffer is always UTF-8 regardless of the selected
 * serial encoding (input bytes are transcoded before returning).
 * Returns number of characters in buf (excluding NUL) on success.
 * Returns -1 when Ctrl-D (0x04) is received.
 */
int uart_read_line(char *buf, size_t max_len);

/* FreeRTOS task entry — initialises UART, prints banner, runs CLI loop. */
void uart_task(void *arg);
