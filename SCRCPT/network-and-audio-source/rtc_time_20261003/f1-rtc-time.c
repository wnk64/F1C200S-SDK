#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/rtc.h>
#include <netdb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static int build_time(const char *version, time_t *result)
{
    const char *start = strrchr(version, '#');
    char weekday[4], month[4], zone[16];
    int day, hour, minute, second, year, offset;
    if (!start || sscanf(start, "%*s %3s %3s %d %d:%d:%d %15s %d",
        weekday, month, &day, &hour, &minute, &second, zone, &year) != 8) return -1;
    const char *names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int m;
    for (m = 0; m < 12 && strcmp(month, names[m]); ++m) {}
    if (m == 12 || year < 2020 || year > 2037 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59) return -1;
    if (!strcmp(zone, "UTC") || !strcmp(zone, "GMT")) offset = 0;
    else if (!strcmp(zone, "PDT")) offset = -7 * 3600;
    else if (!strcmp(zone, "PST")) offset = -8 * 3600;
    else if (!strcmp(zone, "CST")) offset = 8 * 3600;
    else if (strlen(zone) == 5 && (zone[0] == '+' || zone[0] == '-') &&
             zone[1] >= '0' && zone[1] <= '9' && zone[2] >= '0' && zone[2] <= '9' &&
             zone[3] >= '0' && zone[3] <= '9' && zone[4] >= '0' && zone[4] <= '9') {
        int zh = (zone[1] - '0') * 10 + zone[2] - '0';
        int zm = (zone[3] - '0') * 10 + zone[4] - '0';
        if (zh > 23 || zm > 59) return -1;
        offset = (zh * 3600 + zm * 60) * (zone[0] == '-' ? -1 : 1);
    } else return -1;
    struct tm date = {.tm_year = year - 1900, .tm_mon = m, .tm_mday = day,
                      .tm_hour = hour, .tm_min = minute, .tm_sec = second};
    time_t t = timegm(&date);
    if (t < 0) return -1;
    *result = t - offset;
    return 0;
}

static int read_rtc(int fd, time_t *result)
{
    struct rtc_time rtc;
    if (ioctl(fd, RTC_RD_TIME, &rtc)) return -1;
    struct tm date = {.tm_year = rtc.tm_year, .tm_mon = rtc.tm_mon,
        .tm_mday = rtc.tm_mday, .tm_hour = rtc.tm_hour,
        .tm_min = rtc.tm_min, .tm_sec = rtc.tm_sec};
    if (rtc.tm_year < 120 || rtc.tm_year > 137 || rtc.tm_mon < 0 || rtc.tm_mon > 11 ||
        rtc.tm_mday < 1 || rtc.tm_mday > 31 || rtc.tm_hour < 0 || rtc.tm_hour > 23 ||
        rtc.tm_min < 0 || rtc.tm_min > 59 || rtc.tm_sec < 0 || rtc.tm_sec > 59) return -1;
    time_t t = timegm(&date);
    if (t < 0 || date.tm_year != rtc.tm_year || date.tm_mon != rtc.tm_mon ||
        date.tm_mday != rtc.tm_mday) return -1;
    *result = t;
    return 0;
}

static int local_time(const char *text, time_t floor, struct timeval *result)
{
    int year, month, day, hour, minute, second, end = 0;
    if (sscanf(text, "%d-%d-%dT%d:%d:%d%n", &year, &month, &day,
               &hour, &minute, &second, &end) != 6 || text[end] ||
        year < 2020 || year > 2037 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return -1;
    struct tm date = {.tm_year = year - 1900, .tm_mon = month - 1,
        .tm_mday = day, .tm_hour = hour, .tm_min = minute, .tm_sec = second};
    time_t stamp = timegm(&date);
    if (stamp < 8 * 3600 || date.tm_year != year - 1900 || date.tm_mon != month - 1 ||
        date.tm_mday != day || stamp - 8 * 3600 < floor) return -1;
    result->tv_sec = stamp - 8 * 3600; result->tv_usec = 0;
    return 0;
}

static int write_clocks(int fd, const struct timeval *t)
{
    if (settimeofday(t, NULL)) { perror("set system time"); return 1; }
    struct tm date;
    if (!gmtime_r(&t->tv_sec, &date)) return 1;
    struct rtc_time written = {.tm_year = date.tm_year, .tm_mon = date.tm_mon,
        .tm_mday = date.tm_mday, .tm_hour = date.tm_hour,
        .tm_min = date.tm_min, .tm_sec = date.tm_sec};
    time_t rtc;
    if (ioctl(fd, RTC_SET_TIME, &written) || read_rtc(fd, &rtc) ||
        rtc < t->tv_sec || rtc > t->tv_sec + 2) {
        fputs("System time updated, RTC write/readback failed\n", stderr); return 3;
    }
    printf("PCF8563 write/readback OK UTC=%ld\n", (long)rtc);
    return 0;
}

static int decode_ntp(const uint8_t *packet, size_t length, const uint8_t *nonce,
                      time_t floor, struct timeval *result)
{
    if (length < 48 || (packet[0] >> 6) == 3 || (packet[0] & 7) != 4 ||
        ((packet[0] >> 3) & 7) < 3 || ((packet[0] >> 3) & 7) > 4 ||
        packet[1] == 0 || packet[1] > 15 || memcmp(packet + 24, nonce, 8)) return -1;
    uint32_t seconds, fraction;
    memcpy(&seconds, packet + 40, 4); memcpy(&fraction, packet + 44, 4);
    seconds = ntohl(seconds); fraction = ntohl(fraction);
    if (!seconds && !fraction) return -1;
    uint64_t ntp = seconds;
    if (ntp < 2208988800ULL) ntp += 1ULL << 32;
    uint64_t unix_time = ntp - 2208988800ULL;
    time_t converted = (time_t)unix_time;
    if (converted < floor || converted < 0 || (uint64_t)converted != unix_time) return -1;
    result->tv_sec = converted;
    result->tv_usec = ((uint64_t)fraction * 1000000) >> 32;
    return 0;
}

static int query_ntp(const char *host, time_t floor, struct timeval *result)
{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM}, *addresses;
    if (getaddrinfo(host, "123", &hints, &addresses)) return -1;
    int success = -1;
    for (struct addrinfo *a = addresses; a; a = a->ai_next) {
        int sock = socket(a->ai_family, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (sock < 0) continue;
        uint8_t request[48] = {0}, reply[512];
        request[0] = (4 << 3) | 3;
        int random = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        ssize_t count = random < 0 ? -1 : read(random, request + 40, 8);
        if (random >= 0) close(random);
        if (count != 8) { close(sock); break; }
        struct timespec before, after;
        clock_gettime(CLOCK_MONOTONIC, &before);
        if (!connect(sock, a->ai_addr, a->ai_addrlen) && send(sock, request, 48, 0) == 48) {
            struct pollfd p = {.fd = sock, .events = POLLIN};
            if (poll(&p, 1, 3000) > 0 && (p.revents & POLLIN)) {
                ssize_t received = recv(sock, reply, sizeof(reply), 0);
                clock_gettime(CLOCK_MONOTONIC, &after);
                if (received >= 48 && !decode_ntp(reply, received, request + 40, floor, result)) {
                    int64_t us = (after.tv_sec - before.tv_sec) * 1000000LL +
                                  (after.tv_nsec - before.tv_nsec) / 1000;
                    if (us >= 0 && us <= 3000000) {
                        result->tv_usec += us / 2;
                        result->tv_sec += result->tv_usec / 1000000;
                        result->tv_usec %= 1000000;
                        success = 0;
                    }
                }
            }
        }
        close(sock);
        if (!success) break;
    }
    freeaddrinfo(addresses);
    return success;
}

static int selftest(void)
{
    time_t a, b;
    if (build_time("#250 Wed Sep 30 21:08:27 PDT 2026", &a) ||
        build_time("#250 Thu Oct 1 04:08:27 UTC 2026", &b) || a != b ||
        !build_time("#250 Wed Sep 30 21:08:27 UNKNOWN 2026", &b)) return 1;
    uint8_t packet[48] = {0}, nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    packet[0] = 0x24; packet[1] = 2; memcpy(packet + 24, nonce, 8);
    uint32_t stamp = htonl((uint32_t)((uint64_t)a + 2208988800ULL));
    memcpy(packet + 40, &stamp, 4);
    struct timeval time;
    if (decode_ntp(packet, 48, nonce, a, &time) || time.tv_sec != a ||
        !decode_ntp(packet, 47, nonce, a, &time) ||
        !decode_ntp(packet, 48, nonce, a + 1, &time)) return 1;
    packet[24] ^= 1;
    if (!decode_ntp(packet, 48, nonce, a, &time)) return 1;
    if (local_time("2026-10-01T12:08:27", a, &time) || time.tv_sec != a ||
        !local_time("2026-02-30T12:08:27", 0, &time) ||
        !local_time("2026-10-01T12:08:26", a, &time) ||
        !local_time("2026-10-01T12:08:27junk", a, &time)) return 1;
    puts("SELFTEST PASS: kernel timezone, NTP validation, manual date and UTC+8");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--selftest")) return selftest();
    int manual = argc == 3 && !strcmp(argv[1], "set-local");
    if (!manual && (argc != 2 || (strcmp(argv[1], "restore") && strcmp(argv[1], "sync")))) return 1;
    struct utsname kernel;
    time_t floor, rtc;
    if (uname(&kernel) || build_time(kernel.version, &floor)) {
        fputs("Cannot parse running kernel build time; no clock changed\n", stderr); return 1;
    }
    printf("kernel_build_utc=%ld\n", (long)floor);
    struct timeval t;
    if (manual && local_time(argv[2], floor, &t)) {
        fputs("Invalid date or earlier than kernel build\n", stderr); return 5;
    }
    int lock = open("/run/f1-clock.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) {
        fputs("Another time operation is in progress\n", stderr); return 4;
    }
    int fd = open("/dev/rtc0", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open rtc0"); return 2; }
    if (manual) { int rc = write_clocks(fd, &t); close(fd); return rc; }
    if (!strcmp(argv[1], "restore")) {
        if (read_rtc(fd, &rtc) || rtc < floor) {
            puts("RTC invalid or older than kernel build; network time required"); close(fd); return 2;
        }
        struct timeval t = {.tv_sec = rtc};
        int rc = settimeofday(&t, NULL);
        close(fd);
        if (rc) { perror("restore system time"); return 1; }
        printf("RTC restored system UTC=%ld\n", (long)rtc); return 0;
    }
    const char *servers[] = {"ntp.aliyun.com", "time.cloudflare.com", "ntp.tencent.com", "pool.ntp.org"};
    int synced = 0;
    for (size_t i = 0; i < sizeof(servers) / sizeof(servers[0]); ++i) {
        if (!query_ntp(servers[i], floor, &t)) {
            printf("NTP server=%s UTC=%ld\n", servers[i], (long)t.tv_sec); synced = 1; break;
        }
    }
    if (!synced) { close(fd); fputs("No valid network time; retry later\n", stderr); return 2; }
    int rc = write_clocks(fd, &t);
    close(fd); return rc;
}
