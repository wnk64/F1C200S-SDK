#include "net_protocol.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
static int nibble(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
int net_unhex(const char *hex, char *out, size_t size)
{
    size_t n = strlen(hex);
    if (n % 2 || n / 2 >= size) return -1;
    for (size_t i = 0; i < n; i += 2) {
        int a = nibble(hex[i]), b = nibble(hex[i + 1]);
        if (a < 0 || b < 0 || !(a | b)) return -1;
        out[i / 2] = (char)((a << 4) | b);
    }
    out[n / 2] = 0; return 0;
}
void net_hex(const char *in, char *out, size_t size)
{
    size_t n = strlen(in); if (n * 2 + 1 > size) { out[0] = 0; return; }
    for (size_t i = 0; i < n; i++) snprintf(out + i * 2, 3, "%02x", (unsigned char)in[i]);
    out[n * 2] = 0;
}
int net_mac_valid(const char *mac)
{
    if (strlen(mac) != 17) return 0;
    for (int i = 0; i < 17; i++) if (i % 3 == 2 ? mac[i] != ':' : !isxdigit((unsigned char)mac[i])) return 0;
    return 1;
}
int net_psk(const char *secret, char *out, size_t size)
{
    size_t n = strlen(secret);
    if (n == 64) {
        for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)secret[i])) return -1;
        if (size <= n) return -1;
        memcpy(out, secret, n + 1); return 0;
    }
    if (n < 8 || n > 63 || size < n * 2 + 3) return -1;
    char *p = out; *p++ = '"';
    for (size_t i = 0; i < n; i++) {
        if ((unsigned char)secret[i] < 32 || (unsigned char)secret[i] > 126) return -1;
        if (secret[i] == '"' || secret[i] == '\\') *p++ = '\\';
        *p++ = secret[i];
    }
    *p++ = '"'; *p = 0; return 0;
}
int net_ssid_decode(const char *text, char *out, size_t size)
{
    size_t n = 0;
    while (*text) {
        int value = (unsigned char)*text++;
        if (value == '\\') {
            value = (unsigned char)*text++;
            if (!value) return -1;
            if (value == 'x') {
                if (!text[0] || !text[1]) return -1;
                int a = nibble(text[0]), b = nibble(text[1]);
                if (a < 0 || b < 0) return -1;
                value = (a << 4) | b; text += 2;
            } else if (value == 'n') value = '\n';
            else if (value == 'r') value = '\r';
            else if (value == 't') value = '\t';
            else if (value == 'e') value = 27;
        }
        if (!value || n + 1 >= size) return -1;
        out[n++] = (char)value;
    }
    out[n] = 0; return 0;
}
