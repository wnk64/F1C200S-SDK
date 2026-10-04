#include "native.h"
#include "build/gbk_map.h"
int ons_gbk_decode(const char *text, uint32_t *codes, size_t capacity)
{
    if (!text || !codes) return -1;
    const uint8_t *input = (const uint8_t *)text; size_t written = 0;
    while (*input) {
        if (written == capacity) return -1;
        unsigned code = *input++;
        if (code >= 0x80) {
            if (code < 0x81 || code > 0xfe) return -1;
            unsigned trail = *input;
            if (trail < 0x40 || trail > 0xfe || trail == 0x7f) return -1;
            ++input;
            unsigned slot = (code - 0x81) * 190 + trail - 0x40 - (trail > 0x7f);
            code = gbk_map[slot]; if (!code) return -1;
        }
        codes[written++] = code;
    }
    return (int)written;
}
