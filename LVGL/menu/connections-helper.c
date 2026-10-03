#define _POSIX_C_SOURCE 200809L
#include "net_protocol.h"
#include <dbus/dbus.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop;
static int wpa_fd = -1;
static char local_socket[100];
static DBusConnection *bus;
static char pair_target[100];
static long long milliseconds(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void on_signal(int n) { (void)n; stop = 1; }
static int error(const char *why) { printf("ERROR\t%s\n", why); fflush(stdout); return 1; }
static int wpa_open(void)
{
    struct sockaddr_un local = {.sun_family = AF_UNIX}, remote = {.sun_family = AF_UNIX};
    snprintf(local_socket, sizeof(local_socket), "/run/f1-menu-wpa-%ld", (long)getpid());
    snprintf(local.sun_path, sizeof(local.sun_path), "%s", local_socket);
    snprintf(remote.sun_path, sizeof(remote.sun_path), "/run/rtl-bs-sta/rtlwlan0");
    wpa_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    return wpa_fd < 0 || bind(wpa_fd, (struct sockaddr *)&local, sizeof(local)) ||
        connect(wpa_fd, (struct sockaddr *)&remote, sizeof(remote)) ? -1 : 0;
}
static int request(const char *command, char *reply, size_t size)
{
    if (send(wpa_fd, command, strlen(command), 0) < 0) return -1;
    fd_set reads; FD_ZERO(&reads); FD_SET(wpa_fd, &reads);
    struct timeval timeout = {3, 0};
    if (select(wpa_fd + 1, &reads, NULL, NULL, &timeout) <= 0) return -1;
    ssize_t n = recv(wpa_fd, reply, size - 1, 0);
    if (n < 0) return -1;
    reply[n] = 0; return 0;
}
static int ok(const char *command)
{
    char reply[512]; return !request(command, reply, sizeof(reply)) && !strncmp(reply, "OK", 2) ? 0 : -1;
}
struct network { char ssid[33], flags[160]; int id, signal, current; };
static int wifi_list(int scan)
{
    struct network items[32]; memset(items, 0, sizeof(items)); int count = 0;
    char reply[16000], *save, *line;
    int active = -1;
    if (!request("STATUS", reply, sizeof(reply)) && strstr(reply, "wpa_state=COMPLETED\n")) {
        char *id = strstr(reply, "\nid="); if (id) active = atoi(id + 4);
    }
    if (scan) { (void)ok("SCAN"); for (int i = 0; i < 3 && !stop; i++) sleep(1); }
    if (request("LIST_NETWORKS", reply, sizeof(reply))) return error("wifi-unavailable");
    line = strtok_r(reply, "\n", &save);
    while ((line = strtok_r(NULL, "\n", &save)) && count < 32) {
        char *a = strchr(line, '\t'); if (!a) continue; *a++ = 0;
        char *b = strchr(a, '\t'); if (!b) continue; *b++ = 0;
        char *c = strchr(b, '\t'); if (!c) continue; *c++ = 0;
        struct network *item = &items[count];
        if (net_ssid_decode(a, item->ssid, sizeof(item->ssid)) || !item->ssid[0]) continue;
        item->id = atoi(line); item->signal = -100; item->current = item->id == active;
        snprintf(item->flags, sizeof(item->flags), "[SAVED]"); count++;
    }
    if (!request("SCAN_RESULTS", reply, sizeof(reply))) {
        line = strtok_r(reply, "\n", &save);
        while ((line = strtok_r(NULL, "\n", &save))) {
            char *fields[5]; fields[0] = line;
            int valid = 1;
            for (int i = 1; i < 5; i++) {
                char *tab = strchr(fields[i-1], '\t'); if (!tab) { valid = 0; break; }
                *tab = 0; fields[i] = tab + 1;
            }
            if (!valid) continue;
            char ssid[33]; if (net_ssid_decode(fields[4], ssid, sizeof(ssid)) || !ssid[0]) continue;
            int i; for (i = 0; i < count; i++) if (!strcmp(items[i].ssid, ssid)) break;
            if (i == count) { if (count == 32) continue; items[i].id = -1; items[i].signal = -100; count++; }
            int strength = atoi(fields[2]);
            if (strength >= items[i].signal) {
                snprintf(items[i].ssid, sizeof(items[i].ssid), "%s", ssid);
                snprintf(items[i].flags, sizeof(items[i].flags), "%s", fields[3]); items[i].signal = strength;
            }
        }
    }
    for (int i = 0; i < count; i++) {
        char hex[67]; net_hex(items[i].ssid, hex, sizeof(hex));
        int secured = strstr(items[i].flags, "WPA") || strstr(items[i].flags, "WEP") || strstr(items[i].flags, "SAVED");
        int unsupported = strstr(items[i].flags, "WEP") || strstr(items[i].flags, "EAP") ||
            (strstr(items[i].flags, "SAE") && !strstr(items[i].flags, "PSK"));
        printf("DEVICE\t%s\t%s\t%d\t%d\t%d\t%d\n", hex, hex,
               items[i].current, items[i].id >= 0, unsupported ? 2 : !!secured, items[i].signal);
    }
    return 0;
}
static int find_network(const char *ssid)
{
    char reply[8000], *save;
    if (request("LIST_NETWORKS", reply, sizeof(reply))) return -1;
    strtok_r(reply, "\n", &save);
    char *line;
    while ((line = strtok_r(NULL, "\n", &save))) {
        char *a = strchr(line, '\t'); if (!a) continue; *a++ = 0;
        char *b = strchr(a, '\t'); if (!b) continue; *b = 0;
        char decoded[33]; if (!net_ssid_decode(a, decoded, sizeof(decoded)) && !strcmp(decoded, ssid)) return atoi(line);
    }
    return -1;
}
static int wifi_action(const char *action, const char *key)
{
    char ssid[33], command[400], reply[4096];
    if (net_unhex(key, ssid, sizeof(ssid)) || !ssid[0]) return error("invalid-ssid");
    int id = find_network(ssid);
    int previous_id = -1;
    if (!request("STATUS", reply, sizeof(reply))) {
        char *old = strstr(reply, "\nid="); if (old) previous_id = atoi(old + 4);
    }
    if (!strcmp(action, "wifi-disconnect")) return ok("DISCONNECT") ? error("disconnect-failed") : 0;
    if (!strcmp(action, "wifi-forget")) {
        if (id < 0) return 0;
        snprintf(command, sizeof(command), "REMOVE_NETWORK %d", id);
        return ok(command) || ok("SAVE_CONFIG") ? error("save-failed") : 0;
    }
    int added = 0;
    if (id < 0) {
        char secret[80] = {0}, psk[150] = {0};
        if (!fgets(secret, sizeof(secret), stdin)) return error("password-required");
        secret[strcspn(secret, "\r\n")] = 0;
        if (secret[0] && net_psk(secret, psk, sizeof(psk))) return error("invalid-password");
        if (request("ADD_NETWORK", reply, sizeof(reply)) || !isdigit((unsigned char)reply[0])) return error("add-failed");
        id = atoi(reply); added = 1;
        snprintf(command, sizeof(command), "SET_NETWORK %d ssid %s", id, key);
        if (ok(command)) goto configure_failed;
        if (secret[0]) {
            snprintf(command, sizeof(command), "SET_NETWORK %d psk %s", id, psk);
            if (ok(command)) goto configure_failed;
            snprintf(command, sizeof(command), "SET_NETWORK %d key_mgmt WPA-PSK", id);
        } else snprintf(command, sizeof(command), "SET_NETWORK %d key_mgmt NONE", id);
        memset(secret, 0, sizeof(secret)); memset(psk, 0, sizeof(psk));
        if (ok(command)) goto configure_failed;
    }
    snprintf(command, sizeof(command), "SELECT_NETWORK %d", id);
    if (ok(command)) goto configure_failed;
    int connected = 0;
    for (int i = 0; i < 30 && !stop; i++) {
        if (!request("STATUS", reply, sizeof(reply)) && strstr(reply, "wpa_state=COMPLETED\n")) {
            char expected[64]; snprintf(expected, sizeof(expected), "\nid=%d\n", id);
            if (strstr(reply, expected)) { connected = 1; break; }
        }
        sleep(1);
    }
    if (!connected) {
        if (added) { snprintf(command, sizeof(command), "REMOVE_NETWORK %d", id); (void)ok(command); }
        if (previous_id >= 0 && previous_id != id) {
            snprintf(command, sizeof(command), "SELECT_NETWORK %d", previous_id); (void)ok(command);
        }
        return error("connect-timeout");
    }
    if (ok("SAVE_CONFIG")) return error("save-failed");
    if (!access("/opt/f1-network/f1-ip-config", X_OK)) {
        pid_t child = fork();
        if (!child) {
            dup2(STDERR_FILENO, STDOUT_FILENO);
            execl("/opt/f1-network/f1-ip-config", "f1-ip-config", "activate", (char *)NULL);
            _exit(127);
        }
        int result;
        if (child < 0 || waitpid(child, &result, 0) != child ||
            !WIFEXITED(result) || WEXITSTATUS(result)) return error("configure-failed");
        return 0;
    }
    FILE *pidfile = fopen("/run/rtl-bs-sta/dhcp.pid", "r");
    if (pidfile) {
        int pid;
        if (fscanf(pidfile, "%d", &pid) == 1 && pid > 1) {
            char path[64], args[400]; snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                ssize_t n = read(fd, args, sizeof(args)-1); close(fd);
                if (n > 0) {
                    for (ssize_t i = 0; i < n; i++) if (!args[i]) args[i] = ' ';
                    args[n] = 0;
                    if (strstr(args, "udhcpc") && strstr(args, "rtlwlan0")) (void)kill(pid, SIGUSR1);
                }
            }
        }
        fclose(pidfile);
    }
    return 0;
configure_failed:
    if (added) { snprintf(command, sizeof(command), "REMOVE_NETWORK %d", id); (void)ok(command); }
    return error("configure-failed");
}
static DBusMessage *call(const char *path, const char *interface, const char *method, const char *arg, int type)
{
    DBusMessage *m = dbus_message_new_method_call("org.bluez", path, interface, method);
    if (!m) return NULL;
    if (arg) dbus_message_append_args(m, type, &arg, DBUS_TYPE_INVALID);
    DBusError e; dbus_error_init(&e);
    DBusMessage *r = dbus_connection_send_with_reply_and_block(bus, m, 35000, &e);
    dbus_message_unref(m);
    if (dbus_error_is_set(&e)) { fprintf(stderr, "BlueZ %s: %s\n", method, e.name); dbus_error_free(&e); }
    return r;
}
static int set_bool(const char *path, const char *interface, const char *name, dbus_bool_t value)
{
    DBusMessage *m = dbus_message_new_method_call("org.bluez", path, "org.freedesktop.DBus.Properties", "Set");
    if (!m) return -1;
    DBusMessageIter iter, variant; dbus_message_iter_init_append(m, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container(&iter, &variant);
    DBusError e; dbus_error_init(&e);
    DBusMessage *r = dbus_connection_send_with_reply_and_block(bus, m, 5000, &e);
    dbus_message_unref(m); dbus_error_free(&e);
    if (!r) return -1;
    dbus_message_unref(r); return 0;
}
static int bt_list(int scan)
{
    const char *adapter = "/org/bluez/hci0";
    int owned_scan = 0;
    if (scan) {
        (void)set_bool(adapter, "org.bluez.Adapter1", "Powered", TRUE);
        DBusMessage *r = call(adapter, "org.bluez.Adapter1", "StartDiscovery", NULL, 0);
        if (r) { dbus_message_unref(r); owned_scan = 1; }
        long long deadline = milliseconds() + 8000;
        while (!stop && milliseconds() < deadline) dbus_connection_read_write_dispatch(bus, 200);
    }
    if (owned_scan) { DBusMessage *r = call(adapter, "org.bluez.Adapter1", "StopDiscovery", NULL, 0); if (r) dbus_message_unref(r); }
    DBusMessage *r = call("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL, 0);
    if (!r) return error("bluetooth-unavailable");
    DBusMessageIter top, objects; dbus_message_iter_init(r, &top); dbus_message_iter_recurse(&top, &objects);
    int count = 0;
    while (dbus_message_iter_get_arg_type(&objects) == DBUS_TYPE_DICT_ENTRY && count < 32) {
        DBusMessageIter object, interfaces; dbus_message_iter_recurse(&objects, &object);
        const char *path; dbus_message_iter_get_basic(&object, &path); dbus_message_iter_next(&object);
        dbus_message_iter_recurse(&object, &interfaces);
        while (dbus_message_iter_get_arg_type(&interfaces) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter iface, props; dbus_message_iter_recurse(&interfaces, &iface);
            const char *name; dbus_message_iter_get_basic(&iface, &name); dbus_message_iter_next(&iface);
            if (!strcmp(name, "org.bluez.Device1") && !strncmp(path, "/org/bluez/hci0/", 16)) {
                char address[30] = "", title[160] = ""; int paired = 0, connected = 0;
                dbus_message_iter_recurse(&iface, &props);
                while (dbus_message_iter_get_arg_type(&props) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter prop, value; dbus_message_iter_recurse(&props, &prop);
                    const char *key; dbus_message_iter_get_basic(&prop, &key); dbus_message_iter_next(&prop);
                    dbus_message_iter_recurse(&prop, &value);
                    if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_STRING) {
                        const char *text; dbus_message_iter_get_basic(&value, &text);
                        if (!strcmp(key, "Address")) snprintf(address, sizeof(address), "%s", text);
                        if (!strcmp(key, "Alias")) snprintf(title, sizeof(title), "%s", text);
                    } else if (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_BOOLEAN) {
                        dbus_bool_t b; dbus_message_iter_get_basic(&value, &b);
                        if (!strcmp(key, "Connected")) connected = b;
                        if (!strcmp(key, "Paired")) paired = b;
                    }
                    dbus_message_iter_next(&props);
                }
                if (net_mac_valid(address)) {
                    char hex[321]; net_hex(title[0] ? title : address, hex, sizeof(hex));
                    printf("DEVICE\t%s\t%s\t%d\t%d\t0\t0\n", address, hex, connected, paired); count++;
                }
            }
            dbus_message_iter_next(&interfaces);
        }
        dbus_message_iter_next(&objects);
    }
    dbus_message_unref(r); return 0;
}
static int user_reply(char *text, size_t size)
{
    fd_set reads; FD_ZERO(&reads); FD_SET(STDIN_FILENO, &reads);
    struct timeval timeout = {60, 0};
    if (select(STDIN_FILENO + 1, &reads, NULL, NULL, &timeout) <= 0 || !fgets(text, size, stdin)) return -1;
    text[strcspn(text, "\r\n")] = 0; return 0;
}
static DBusHandlerResult agent(DBusConnection *connection, DBusMessage *message, void *data)
{
    (void)data;
    const char *method = dbus_message_get_member(message);
    if (!method) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    DBusMessage *reply = NULL;
    const char *target = NULL;
    DBusMessageIter iter;
    if (dbus_message_iter_init(message, &iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_OBJECT_PATH)
        dbus_message_iter_get_basic(&iter, &target);
    int allowed = target && !strcmp(target, pair_target);
    char text[80] = "";
    if ((!strcmp(method, "RequestConfirmation") || !strcmp(method, "RequestPinCode") || !strcmp(method, "RequestPasskey")) && allowed) {
        unsigned number = 0;
        if (!strcmp(method, "RequestConfirmation")) { dbus_message_iter_next(&iter); dbus_message_iter_get_basic(&iter, &number); }
        printf("%s\t%06u\n", !strcmp(method, "RequestConfirmation") ? "CONFIRM" : "PIN", number); fflush(stdout);
        if (!user_reply(text, sizeof(text))) {
            if (!strcmp(method, "RequestConfirmation") && !strcmp(text, "yes")) reply = dbus_message_new_method_return(message);
            else if (!strcmp(method, "RequestPinCode") && text[0]) {
                reply = dbus_message_new_method_return(message); const char *pin = text;
                dbus_message_append_args(reply, DBUS_TYPE_STRING, &pin, DBUS_TYPE_INVALID);
            } else if (!strcmp(method, "RequestPasskey") && strlen(text) <= 6 && text[0]) {
                int digits = 1; for (char *p = text; *p; p++) if (!isdigit((unsigned char)*p)) digits = 0;
                if (digits) { number = (unsigned)atoi(text); reply = dbus_message_new_method_return(message); dbus_message_append_args(reply, DBUS_TYPE_UINT32, &number, DBUS_TYPE_INVALID); }
            }
        }
    } else if ((!strcmp(method, "RequestAuthorization") || !strcmp(method, "AuthorizeService")) && allowed)
        reply = dbus_message_new_method_return(message);
    else if (!strcmp(method, "DisplayPinCode") && allowed) {
        const char *pin; dbus_message_iter_next(&iter); dbus_message_iter_get_basic(&iter, &pin);
        printf("DISPLAY\t%s\n", pin); fflush(stdout); reply = dbus_message_new_method_return(message);
    } else if (!strcmp(method, "DisplayPasskey") && allowed) {
        unsigned number; dbus_message_iter_next(&iter); dbus_message_iter_get_basic(&iter, &number);
        printf("DISPLAY\t%06u\n", number); fflush(stdout); reply = dbus_message_new_method_return(message);
    } else if (!strcmp(method, "Release") || !strcmp(method, "Cancel")) reply = dbus_message_new_method_return(message);
    if (!reply) reply = dbus_message_new_error(message, "org.bluez.Error.Rejected", "User declined pairing");
    dbus_connection_send(connection, reply, NULL); dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}
static int is_paired(const char *path)
{
    const char *iface = "org.bluez.Device1", *prop = "Paired";
    DBusMessage *m = dbus_message_new_method_call("org.bluez", path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(m, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
    DBusError e; dbus_error_init(&e);
    DBusMessage *r = dbus_connection_send_with_reply_and_block(bus, m, 5000, &e);
    dbus_message_unref(m); dbus_error_free(&e); if (!r) return 0;
    DBusMessageIter iter, variant; dbus_bool_t paired = FALSE;
    dbus_message_iter_init(r, &iter); dbus_message_iter_recurse(&iter, &variant);
    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_BOOLEAN) dbus_message_iter_get_basic(&variant, &paired);
    dbus_message_unref(r); return paired;
}
static int bt_action(const char *action, const char *mac)
{
    if (!net_mac_valid(mac)) return error("invalid-device");
    snprintf(pair_target, sizeof(pair_target), "/org/bluez/hci0/dev_%s", mac);
    for (char *p = pair_target + 19; *p; p++) { if (*p == ':') *p = '_'; else *p = (char)toupper((unsigned char)*p); }
    if (!strcmp(action, "bt-forget")) {
        DBusMessage *r = call("/org/bluez/hci0", "org.bluez.Adapter1", "RemoveDevice", pair_target, DBUS_TYPE_OBJECT_PATH);
        if (!r) return error("forget-failed");
        dbus_message_unref(r); return 0;
    }
    if (!strcmp(action, "bt-disconnect")) {
        DBusMessage *r = call(pair_target, "org.bluez.Device1", "Disconnect", NULL, 0);
        if (!r) return error("disconnect-failed");
        dbus_message_unref(r); return 0;
    }
    (void)set_bool("/org/bluez/hci0", "org.bluez.Adapter1", "Powered", TRUE);
    if (!is_paired(pair_target)) {
        static const DBusObjectPathVTable table = {.message_function = agent};
        const char *agent_path = "/org/f1/MenuAgent", *capability = "KeyboardDisplay";
        dbus_connection_register_object_path(bus, agent_path, &table, NULL);
        DBusMessage *m = dbus_message_new_method_call("org.bluez", "/org/bluez", "org.bluez.AgentManager1", "RegisterAgent");
        dbus_message_append_args(m, DBUS_TYPE_OBJECT_PATH, &agent_path, DBUS_TYPE_STRING, &capability, DBUS_TYPE_INVALID);
        DBusError e; dbus_error_init(&e);
        DBusMessage *r = dbus_connection_send_with_reply_and_block(bus, m, 5000, &e); dbus_message_unref(m); dbus_error_free(&e);
        if (!r) return error("agent-failed");
        dbus_message_unref(r);
        m = dbus_message_new_method_call("org.bluez", pair_target, "org.bluez.Device1", "Pair");
        DBusPendingCall *pending;
        if (!dbus_connection_send_with_reply(bus, m, &pending, 90000)) { dbus_message_unref(m); return error("pair-failed"); }
        dbus_message_unref(m);
        while (!stop && !dbus_pending_call_get_completed(pending)) dbus_connection_read_write_dispatch(bus, 100);
        r = dbus_pending_call_steal_reply(pending); dbus_pending_call_unref(pending);
        if (!r || dbus_message_get_type(r) == DBUS_MESSAGE_TYPE_ERROR) { if (r) dbus_message_unref(r); return error("pair-failed"); }
        dbus_message_unref(r);
    }
    (void)set_bool(pair_target, "org.bluez.Device1", "Trusted", TRUE);
    DBusMessage *r = call(pair_target, "org.bluez.Device1", "Connect", NULL, 0);
    if (!r) return error("connect-failed");
    dbus_message_unref(r); return 0;
}
int main(int argc, char **argv)
{
    if (argc < 2) return 1;
    int valid = !strcmp(argv[1], "wifi-list") || !strcmp(argv[1], "wifi-scan") ||
        !strcmp(argv[1], "wifi-connect") || !strcmp(argv[1], "wifi-disconnect") ||
        !strcmp(argv[1], "wifi-forget") || !strcmp(argv[1], "bt-list") ||
        !strcmp(argv[1], "bt-scan") || !strcmp(argv[1], "bt-connect") ||
        !strcmp(argv[1], "bt-disconnect") || !strcmp(argv[1], "bt-forget");
    if (!valid) return error("invalid-action");
    signal(SIGTERM, on_signal); signal(SIGINT, on_signal); signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    int result = 1;
    if (!strncmp(argv[1], "wifi-", 5)) {
        if (wpa_open()) result = error("wifi-unavailable");
        else if (!strcmp(argv[1], "wifi-scan") || !strcmp(argv[1], "wifi-list")) result = wifi_list(!strcmp(argv[1], "wifi-scan"));
        else if (argc == 3) result = wifi_action(argv[1], argv[2]);
        if (wpa_fd >= 0) close(wpa_fd);
        if (local_socket[0]) unlink(local_socket);
    } else if (!strncmp(argv[1], "bt-", 3)) {
        DBusError e; dbus_error_init(&e); bus = dbus_bus_get_private(DBUS_BUS_SYSTEM, &e); dbus_error_free(&e);
        if (!bus) result = error("bluetooth-unavailable");
        else {
            dbus_connection_set_exit_on_disconnect(bus, FALSE);
            if (!strcmp(argv[1], "bt-scan") || !strcmp(argv[1], "bt-list")) result = bt_list(!strcmp(argv[1], "bt-scan"));
            else if (argc == 3) result = bt_action(argv[1], argv[2]);
            dbus_connection_close(bus); dbus_connection_unref(bus);
        }
    }
    if (!result) puts("DONE");
    return result;
}
