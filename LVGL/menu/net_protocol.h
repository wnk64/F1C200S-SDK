#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H
#include <stddef.h>
int net_unhex(const char *hex, char *out, size_t size);
void net_hex(const char *in, char *out, size_t size);
int net_mac_valid(const char *mac);
int net_psk(const char *secret, char *out, size_t size);
int net_ssid_decode(const char *text, char *out, size_t size);
#endif
