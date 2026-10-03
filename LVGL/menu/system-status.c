#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "system-status.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

static int property(const char *name, int *value)
{
    char path[200];
    snprintf(path, sizeof(path), "/sys/class/power_supply/ip5209-battery/%s", name);
    FILE *file = fopen(path, "r");
    if (!file) return -1;
    int rc = fscanf(file, "%d", value) == 1 ? 0 : -1;
    fclose(file); return rc;
}
void system_ssh_address(char *text, size_t length)
{
    snprintf(text, length, "SSH --");
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses)) return;
    for (struct ifaddrs *a = addresses; a; a = a->ifa_next) {
        if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET ||
            !(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK)) continue;
        if (strcmp(a->ifa_name, "rtlwlan0")) continue;
        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *addr = (struct sockaddr_in *)a->ifa_addr;
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)))
            snprintf(text, length, "SSH root@%s", ip);
        break;
    }
    freeifaddrs(addresses);
}
void system_battery(char *text, size_t length, int *charging)
{
    *charging = -1;
    snprintf(text, length, "--%%");
    FILE *file = fopen("/sys/class/power_supply/ip5209-battery/status", "r");
    if (file) {
        char status[40];
        if (fgets(status, sizeof(status), file)) {
            status[strcspn(status, "\r\n")] = 0;
            if (!strcmp(status, "Charging")) *charging = 1;
            else if (!strcmp(status, "Full")) *charging = 2;
            else if (!strcmp(status, "Discharging") || !strcmp(status, "Not charging")) *charging = 0;
        }
        fclose(file);
    }
    int capacity;
    if (!property("capacity", &capacity) && capacity >= 0 && capacity <= 100) {
        snprintf(text, length, "%d%%", capacity); return;
    }
    /* User-confirmed 4.2 V cell; external VSET is not readable as a nominal target. */
    int ocv;
    if (property("voltage_ocv", &ocv) || ocv < 2500000 || ocv > 4350000) return;
    static const int mv[] = {3300, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200};
    static const int percent[] = {0, 5, 10, 25, 50, 70, 85, 95, 100};
    int voltage = ocv / 1000;
    capacity = 0;
    for (unsigned i = 1; i < sizeof(mv) / sizeof(mv[0]); ++i) {
        if (voltage >= mv[i]) { capacity = percent[i]; continue; }
        if (voltage > mv[i - 1])
            capacity = percent[i - 1] + (voltage - mv[i - 1]) *
                (percent[i] - percent[i - 1]) / (mv[i] - mv[i - 1]);
        break;
    }
    snprintf(text, length, "~%d%%", capacity);
}
