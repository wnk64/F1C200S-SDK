#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONFIG "/etc/f1-ip.conf"
struct config { char mode[12], cidr[32], gateway[20], dns[20], ssid[256]; };
static int run(char *const args[])
{
    pid_t pid = fork();
    if (!pid) { execv(args[0], args); _exit(127); }
    int status;
    if (pid < 0 || waitpid(pid, &status, 0) != pid) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
static int network(char *ssid, size_t length)
{
    int pipes[2]; if (pipe(pipes)) return -1;
    pid_t pid = fork();
    if (!pid) {
        dup2(pipes[1], STDOUT_FILENO); close(pipes[0]); close(pipes[1]);
        execl("/root/aic_miracast/wpa24_aic_wfd_cli", "wpa_cli", "-p", "/run/rtl-bs-sta",
              "-i", "rtlwlan0", "status", (char *)NULL); _exit(127);
    }
    close(pipes[1]);
    FILE *f = fdopen(pipes[0], "r");
    char line[400]; int ready = 0; ssid[0] = 0;
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strcmp(line, "wpa_state=COMPLETED")) ready = 1;
            if (!strncmp(line, "ssid=", 5)) snprintf(ssid, length, "%s", line + 5);
        }
        fclose(f);
    } else close(pipes[0]);
    int status; if (pid > 0) waitpid(pid, &status, 0);
    return ready && ssid[0] ? 0 : -1;
}
static int ip(const char *text, uint32_t *out)
{
    struct in_addr a;
    if (inet_pton(AF_INET, text, &a) != 1) return -1;
    *out = ntohl(a.s_addr);
    return (*out >> 24) == 0 || (*out >> 24) == 127 || (*out >> 24) >= 224 ? -1 : 0;
}
static int validate(struct config *c)
{
    if (!strcmp(c->mode, "dhcp")) return 0;
    if (strcmp(c->mode, "static")) return -1;
    char address[32]; snprintf(address, sizeof(address), "%s", c->cidr);
    char *slash = strchr(address, '/'), *end;
    if (!slash) return -1;
    *slash++ = 0; long prefix = strtol(slash, &end, 10);
    uint32_t host, gateway, dns;
    if (!*slash || *end || prefix < 1 || prefix > 30 || ip(address, &host) ||
        ip(c->gateway, &gateway) || ip(c->dns, &dns)) return -1;
    uint32_t mask = 0xffffffffU << (32 - prefix), part = host & ~mask;
    return !part || part == ~mask || host == gateway ||
        (host & mask) != (gateway & mask) ? -1 : 0;
}
static int load(struct config *c)
{
    FILE *f = fopen(CONFIG, "r"); if (!f) return -1;
    char *fields[] = {c->mode, c->cidr, c->gateway, c->dns, c->ssid};
    size_t sizes[] = {sizeof(c->mode), sizeof(c->cidr), sizeof(c->gateway), sizeof(c->dns), sizeof(c->ssid)};
    int rc = 0;
    for (int i = 0; i < 5; ++i) {
        if (!fgets(fields[i], sizes[i], f)) { rc = -1; break; }
        fields[i][strcspn(fields[i], "\r\n")] = 0;
    }
    fclose(f); return rc || validate(c) ? -1 : 0;
}
static void stop_dhcp(void)
{
    FILE *f = fopen("/run/rtl-bs-sta/dhcp.pid", "r"); long pid;
    if (!f) return;
    int valid = fscanf(f, "%ld", &pid) == 1 && pid > 1; fclose(f);
    if (!valid) return;
    char path[80], cmd[512]; snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
    f = fopen(path, "r"); if (!f) return;
    size_t count = fread(cmd, 1, sizeof(cmd) - 1, f); fclose(f); cmd[count] = 0;
    for (size_t i = 0; i < count; ++i) if (!cmd[i]) cmd[i] = ' ';
    if (!strstr(cmd, "udhcpc") || !strstr(cmd, "rtlwlan0")) return;
    kill(pid, SIGTERM);
    for (int i = 0; i < 100 && kill(pid, 0) == 0; ++i) usleep(20000);
    unlink("/run/rtl-bs-sta/dhcp.pid");
    f = fopen("/run/rtl-bs-sta/worker.pid", "r");
    if (f) {
        long worker;
        int same = fscanf(f, "%ld", &worker) == 1 && worker == pid;
        fclose(f); if (same) unlink("/run/rtl-bs-sta/worker.pid");
    }
}
static int apply(struct config *c)
{
    char ssid[256];
    if (network(ssid, sizeof(ssid))) return 2;
    if (strcmp(c->mode, "static") || strcmp(ssid, c->ssid)) return 2;
    stop_dhcp();
    char host_text[32], broadcast[INET_ADDRSTRLEN];
    snprintf(host_text, sizeof(host_text), "%s", c->cidr);
    char *slash = strchr(host_text, '/');
    *slash++ = 0;
    uint32_t host; if (ip(host_text, &host)) return 1;
    struct in_addr b = {.s_addr = htonl(host | ~(0xffffffffU << (32 - atoi(slash))))};
    if (!inet_ntop(AF_INET, &b, broadcast, sizeof(broadcast))) return 1;
    char *address[] = {"/sbin/ip", "addr", "replace", c->cidr, "broadcast", broadcast, "dev", "rtlwlan0", NULL};
    char *route[] = {"/sbin/ip", "route", "replace", "default", "via", c->gateway, "dev", "rtlwlan0", NULL};
    if (run(address) || run(route)) return 1;
    struct ifaddrs *list;
    if (!getifaddrs(&list)) {
        for (struct ifaddrs *a = list; a; a = a->ifa_next) {
            if (!a->ifa_addr || !a->ifa_netmask || a->ifa_addr->sa_family != AF_INET ||
                strcmp(a->ifa_name, "rtlwlan0")) continue;
            char old[48], iptext[INET_ADDRSTRLEN];
            struct sockaddr_in *addr = (struct sockaddr_in *)a->ifa_addr;
            struct sockaddr_in *mask = (struct sockaddr_in *)a->ifa_netmask;
            if (!inet_ntop(AF_INET, &addr->sin_addr, iptext, sizeof(iptext))) continue;
            snprintf(old, sizeof(old), "%s/%d", iptext, __builtin_popcount(ntohl(mask->sin_addr.s_addr)));
            if (!strcmp(old, c->cidr)) continue;
            char *remove[] = {"/sbin/ip", "addr", "del", old, "dev", "rtlwlan0", NULL};
            if (run(remove)) { freeifaddrs(list); return 1; }
        }
        freeifaddrs(list);
    }
    FILE *f = fopen("/etc/resolv.conf", "w");
    if (!f) return 1;
    fprintf(f, "nameserver %s # rtlwlan0\n", c->dns);
    if (fclose(f)) return 1;
    puts("Static IPv4 applied"); return 0;
}
int main(int argc, char **argv)
{
    struct config c = {0};
    if (argc == 2 && !strcmp(argv[1], "show")) {
        if (load(&c)) { strcpy(c.mode, "dhcp"); strcpy(c.cidr, "10.0.0.234/24");
            strcpy(c.gateway, "10.0.0.1"); strcpy(c.dns, "10.0.0.1"); }
        printf("%s\n%s\n%s\n%s\n", c.mode, c.cidr, c.gateway, c.dns); return 0;
    }
    if (argc == 6 && (!strcmp(argv[1], "validate") || !strcmp(argv[1], "configure"))) {
        if (strlen(argv[2]) >= sizeof(c.mode) || strlen(argv[3]) >= sizeof(c.cidr) ||
            strlen(argv[4]) >= sizeof(c.gateway) || strlen(argv[5]) >= sizeof(c.dns)) return 3;
        strcpy(c.mode, argv[2]); strcpy(c.cidr, argv[3]); strcpy(c.gateway, argv[4]); strcpy(c.dns, argv[5]);
        if (validate(&c)) { fputs("Invalid IPv4/CIDR, subnet gateway or DNS\n", stderr); return 3; }
        if (!strcmp(argv[1], "validate")) return 0;
        if (network(c.ssid, sizeof(c.ssid))) return 4;
        int lock = open("/run/f1-ip.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) return 5;
        char temp[] = "/etc/f1-ip.conf.XXXXXX"; int fd = mkstemp(temp);
        if (fd < 0) return 1;
        FILE *f = fdopen(fd, "w"); if (!f) { close(fd); unlink(temp); return 1; }
        fprintf(f, "%s\n%s\n%s\n%s\n%s\n", c.mode, c.cidr, c.gateway, c.dns, c.ssid);
        fflush(f); int rc = ferror(f) || fsync(fd); if (fclose(f)) rc = 1;
        if (rc || rename(temp, CONFIG)) { unlink(temp); return 1; }
        if (!strcmp(c.mode, "static")) return apply(&c);
        stop_dhcp();
        char *dhcp[] = {"/sbin/udhcpc", "-b", "-i", "rtlwlan0", "-p", "/run/rtl-bs-sta/dhcp.pid", NULL};
        return run(dhcp);
    }
    if (argc == 2 && !strcmp(argv[1], "apply")) return load(&c) ? 2 : apply(&c);
    if (argc == 2 && !strcmp(argv[1], "activate")) {
        if (!load(&c)) {
            int rc = apply(&c); if (rc != 2) return rc;
        }
        stop_dhcp();
        char *dhcp[] = {"/sbin/udhcpc", "-b", "-i", "rtlwlan0", "-p", "/run/rtl-bs-sta/dhcp.pid", NULL};
        return run(dhcp);
    }
    return 1;
}
