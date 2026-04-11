#include "encoding.h"
#include <string.h>

#ifdef __GNUC__
#  include <strings.h>   /* strcasecmp */
#endif

static encoding_t s_encoding = ENCODING_UTF8;

void encoding_set(encoding_t enc) { s_encoding = enc; }
encoding_t encoding_get(void)     { return s_encoding; }

/* ── Lookup table: Unicode codepoint → single-byte target encoding ──── */
typedef struct {
    uint32_t codepoint;
    uint8_t  cp1250;
    uint8_t  iso8859_2;
} char_map_t;

/*
 * Polish diacritics in CP1250 and ISO 8859-2.
 * Where both encodings assign the same byte the two columns are equal.
 */
static const char_map_t s_map[] = {
    /* codepoint   cp1250  iso-8859-2    glyph */
    { 0x00D3,      0xD3,   0xD3 },   /* Ó */
    { 0x00F3,      0xF3,   0xF3 },   /* ó */
    { 0x0104,      0xA5,   0xA1 },   /* Ą */
    { 0x0105,      0xB9,   0xB1 },   /* ą */
    { 0x0106,      0xC6,   0xC6 },   /* Ć */
    { 0x0107,      0xE6,   0xE6 },   /* ć */
    { 0x0118,      0xCA,   0xCA },   /* Ę */
    { 0x0119,      0xEA,   0xEA },   /* ę */
    { 0x0141,      0xA3,   0xA3 },   /* Ł */
    { 0x0142,      0xB3,   0xB3 },   /* ł */
    { 0x0143,      0xD1,   0xD1 },   /* Ń */
    { 0x0144,      0xF1,   0xF1 },   /* ń */
    { 0x015A,      0x8C,   0xA6 },   /* Ś */
    { 0x015B,      0x9C,   0xB6 },   /* ś */
    { 0x0179,      0x8F,   0xAC },   /* Ź */
    { 0x017A,      0x9F,   0xBC },   /* ź */
    { 0x017B,      0xAF,   0xAF },   /* Ż */
    { 0x017C,      0xBF,   0xBF },   /* ż */
};
#define MAP_SIZE (sizeof(s_map) / sizeof(s_map[0]))

/* ── Helpers ─────────────────────────────────────────────────────────── */

static uint8_t codepoint_to_target(uint32_t cp)
{
    for (size_t i = 0; i < MAP_SIZE; i++) {
        if (s_map[i].codepoint == cp) {
            return (s_encoding == ENCODING_CP1250)
                ? s_map[i].cp1250
                : s_map[i].iso8859_2;
        }
    }
    return '?';   /* no mapping — replacement character */
}

static uint32_t byte_to_codepoint(uint8_t b)
{
    for (size_t i = 0; i < MAP_SIZE; i++) {
        uint8_t target = (s_encoding == ENCODING_CP1250)
            ? s_map[i].cp1250
            : s_map[i].iso8859_2;
        if (target == b) return s_map[i].codepoint;
    }
    return (uint32_t)'?';
}

/* Write a Unicode codepoint as UTF-8 into out. Returns bytes written. */
static int emit_utf8(uint32_t cp, char *out, size_t out_max)
{
    if (cp < 0x80) {
        if (out_max < 1) return 0;
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        if (out_max < 2) return 0;
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        if (out_max < 3) return 0;
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    return 0; /* >U+FFFF not needed for Polish */
}

/* ── Public API ──────────────────────────────────────────────────────── */

size_t encoding_utf8_to_target(const char *in, size_t in_len,
                                char *out, size_t out_max)
{
    if (s_encoding == ENCODING_UTF8) {
        size_t n = (in_len < out_max) ? in_len : out_max;
        memcpy(out, in, n);
        return n;
    }

    size_t i = 0, o = 0;
    while (i < in_len && o < out_max) {
        uint8_t c = (uint8_t)in[i];

        if (c < 0x80) {                                 /* US-ASCII — passthrough */
            out[o++] = (char)c;
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < in_len) {    /* 2-byte */
            uint32_t cp = ((uint32_t)(c & 0x1F) << 6)
                        | ((uint32_t)((uint8_t)in[i + 1] & 0x3F));
            out[o++] = (char)codepoint_to_target(cp);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < in_len) {    /* 3-byte */
            uint32_t cp = ((uint32_t)(c & 0x0F) << 12)
                        | ((uint32_t)((uint8_t)in[i + 1] & 0x3F) << 6)
                        | ((uint32_t)((uint8_t)in[i + 2] & 0x3F));
            out[o++] = (char)codepoint_to_target(cp);
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < in_len) {    /* 4-byte */
            out[o++] = '?';
            i += 4;
        } else {                                        /* malformed — skip */
            out[o++] = '?';
            i++;
        }
    }
    return o;
}

size_t encoding_raw_to_utf8(const uint8_t *in, size_t in_len,
                             char *out, size_t out_max)
{
    if (out_max == 0) return 0;

    if (s_encoding == ENCODING_UTF8) {
        size_t n = (in_len < out_max) ? in_len : out_max;
        memcpy(out, in, n);
        return n;
    }

    /* Leave at least 1 byte so the caller can NUL-terminate. */
    size_t limit = out_max - 1;

    size_t i = 0, o = 0;
    while (i < in_len) {
        uint8_t b = in[i++];
        if (b < 0x80) {
            if (o >= limit) break;
            out[o++] = (char)b;
        } else {
            uint32_t cp = byte_to_codepoint(b);
            int written = emit_utf8(cp, out + o, limit - o);
            if (written == 0) break;
            o += (size_t)written;
        }
    }
    return o;
}

encoding_t encoding_from_str(const char *name)
{
    if (strcasecmp(name, "cp1250")       == 0 ||
        strcasecmp(name, "windows-1250") == 0 ||
        strcasecmp(name, "win1250")      == 0) {
        return ENCODING_CP1250;
    }
    if (strcasecmp(name, "iso-8859-2") == 0 ||
        strcasecmp(name, "iso8859-2")  == 0 ||
        strcasecmp(name, "latin2")     == 0) {
        return ENCODING_ISO_8859_2;
    }
    return ENCODING_UTF8;
}

const char *encoding_to_str(encoding_t enc)
{
    switch (enc) {
        case ENCODING_CP1250:     return "cp1250";
        case ENCODING_ISO_8859_2: return "iso-8859-2";
        default:                  return "utf-8";
    }
}
