#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ENCODING_UTF8 = 0,
    ENCODING_CP1250,
    ENCODING_ISO_8859_2,
} encoding_t;

void        encoding_set(encoding_t enc);
encoding_t  encoding_get(void);

encoding_t  encoding_from_str(const char *name);
const char *encoding_to_str(encoding_t enc);

/*
 * Transcode UTF-8 input to the currently selected target encoding.
 * Output is always <= input length (multi-byte → single byte).
 * Returns number of bytes written (NUL not appended).
 */
size_t encoding_utf8_to_target(const char *utf8_in, size_t in_len,
                                char *out, size_t out_max);

/*
 * Transcode raw bytes in the currently selected encoding to UTF-8.
 * Used for user input coming from the vintage terminal.
 * Returns number of bytes written (NUL not appended; caller must append).
 */
size_t encoding_raw_to_utf8(const uint8_t *in, size_t in_len,
                             char *out, size_t out_max);
