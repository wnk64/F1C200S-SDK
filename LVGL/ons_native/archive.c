#define _FILE_OFFSET_BITS 64
#include "native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct bits { const uint8_t *bytes; size_t length, position; };
static int read_bits(struct bits *b, unsigned count)
{
    if (b->position > b->length * 8 || count > b->length * 8 - b->position) return -1;
    unsigned value = 0;
    while (count--) { value = value << 1 | ((b->bytes[b->position / 8] >> (7 - b->position % 8)) & 1); ++b->position; }
    return value;
}
int ons_lzss(const uint8_t *input, size_t length, uint8_t *output, size_t expected)
{
    if ((!input && length) || (!output && expected) || length > 8388608 || expected > 8388608) return -1;
    /* ONS DirectReader format: EI=8, EJ=4, P=1, MSB-first bits. */
    uint8_t history[256] = {0}; unsigned write_at = 239;
    struct bits bits = {input, length, 0}; size_t written = 0;
    while (written < expected) {
        int literal = read_bits(&bits, 1);
        if (literal < 0) return -1;
        if (literal) {
            int value = read_bits(&bits, 8);
            if (value < 0) return -1;
            output[written++] = history[write_at++ & 255] = value;
        } else {
            int at = read_bits(&bits, 8), run = read_bits(&bits, 4);
            if (at < 0 || run < 0 || (size_t)(run + 2) > expected - written) return -1;
            for (int i = 0; i < run + 2; ++i) {
                uint8_t value = history[(at + i) & 255];
                output[written++] = history[write_at++ & 255] = value;
            }
        }
    }
    return 0;
}
static int be(FILE *f, unsigned count, uint32_t *result)
{
    *result = 0;
    while (count--) { int c = fgetc(f); if (c == EOF) return -1; *result = *result << 8 | c; }
    return 0;
}
static unsigned normalized(unsigned c) { return c == '\\' ? '/' : c < 128 ? (unsigned)tolower((int)c) : c; }
static int same_name(const char *a, const char *b)
{
    while (*a && *b && normalized((uint8_t)*a) == normalized((uint8_t)*b)) { ++a; ++b; }
    return !*a && !*b;
}
int ons_nsa_read(const char *path, const char *name, uint8_t **bytes, size_t *length)
{
    if (!path || !name || !bytes || !length) return -1;
    *bytes = NULL; *length = 0;
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    uint8_t *packed = NULL, *unpacked = NULL; int result = -1;
    if (fseeko(f, 0, SEEK_END)) goto done;
    off_t file_size = ftello(f); if (file_size < 6 || fseeko(f, 0, SEEK_SET)) goto done;
    uint32_t count, base;
    if (be(f, 2, &count) || be(f, 4, &base) || base < 6 || base > (uint64_t)file_size) goto done;
    for (uint32_t i = 0; i < count; ++i) {
        char entry[512]; size_t n = 0; int c;
        while ((c = fgetc(f)) != EOF && c) { if (n + 1 >= sizeof(entry)) goto done; entry[n++] = c; }
        if (c == EOF) goto done;
        entry[n] = 0;
        int compression = fgetc(f); uint32_t offset, size, raw;
        if (compression == EOF || be(f, 4, &offset) || be(f, 4, &size) || be(f, 4, &raw) || ftello(f) > base) goto done;
        if (!same_name(entry, name)) continue;
        if (!size || size > 8388608 || (uint64_t)base + offset + size > (uint64_t)file_size ||
            (compression != 0 && compression != 2)) goto done;
        if (fseeko(f, (off_t)base + offset, SEEK_SET)) goto done;
        packed = malloc(size); if (!packed || fread(packed, 1, size, f) != size) goto done;
        if (compression == 2) {
            if (!raw || raw > 8388608) goto done;
            unpacked = malloc(raw); if (!unpacked || ons_lzss(packed, size, unpacked, raw)) goto done;
            *bytes = unpacked; unpacked = NULL; *length = raw;
        } else { *bytes = packed; packed = NULL; *length = size; }
        result = 0; break;
    }
done:
    free(packed); free(unpacked); fclose(f); return result;
}
