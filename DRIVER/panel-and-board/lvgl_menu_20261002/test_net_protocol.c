#include "net_protocol.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    char value[200], hex[200];
    assert(!net_unhex("776e6b", value, sizeof(value)) && !strcmp(value, "wnk"));
    assert(net_unhex("0012", value, sizeof(value)) == -1);
    assert(net_unhex("0", value, sizeof(value)) == -1);
    assert(net_unhex("xx", value, sizeof(value)) == -1);
    assert(net_unhex("6162", value, 2) == -1);
    net_hex("a\";$(x)\\b", hex, sizeof(hex));
    assert(!net_unhex(hex, value, sizeof(value)) && !strcmp(value, "a\";$(x)\\b"));
    assert(net_mac_valid("12:11:71:41:9C:4A"));
    assert(!net_mac_valid("12:11:71:41:9C:4A;reboot"));
    assert(!net_mac_valid("12:11:71:41:9X:4A"));
    assert(!net_psk("ab\"c\\def", value, sizeof(value)) && !strcmp(value, "\"ab\\\"c\\\\def\""));
    assert(net_psk("short", value, sizeof(value)) == -1);
    assert(net_psk("password\n", value, sizeof(value)) == -1);
    assert(!net_ssid_decode("a\\x22\\x5cb\\t", value, sizeof(value)) && !strcmp(value, "a\"\\b\t"));
    assert(net_ssid_decode("a\\xZ1", value, sizeof(value)) == -1);
    assert(net_ssid_decode("a\\", value, sizeof(value)) == -1);
    return 0;
}
