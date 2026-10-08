#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "SeatAccess.h"

/* Reads events without grabbing devices; stdin EOF is the lifetime contract.
 * No disk writes, shell commands, network access, or arbitrary file paths. */
#define MAX_DEVICES 64
static struct pollfd fds[MAX_DEVICES + 1];
static char paths[MAX_DEVICES + 1][64];
static int count = 1;
static dev_t device_numbers[MAX_DEVICES + 1];
static unsigned char held[MAX_DEVICES + 1][KEY_MAX + 1];
static unsigned short references[KEY_MAX + 1];
static int dropped[MAX_DEVICES + 1];
static struct xkb_state *state;
static xkb_layout_index_t active_layout;
static void apply_layout(void) {
    xkb_state_update_mask(state,
        xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED),
        xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED),
        xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED), 0, 0, active_layout);
}
static int set_layout_name(const char *name) {
    struct xkb_keymap *map = xkb_state_get_keymap(state);
    for (xkb_layout_index_t i = 0; i < xkb_keymap_num_layouts(map); i++) {
        const char *candidate = xkb_keymap_layout_get_name(map, i);
        if (candidate && !strcmp(candidate, name)) { active_layout = i; apply_layout(); return 1; }
    }
    return 0;
}
/* Bounded, line-framed control input. No paths, commands, or key text accepted. */
static int control_bytes(const char *bytes, size_t length) {
    static char line[256];
    static size_t used;
    for (size_t i = 0; i < length; i++) {
        if (bytes[i] == '\n') {
            line[used] = 0;
            used = 0;
            if (!strncmp(line, "layout ", 7) && set_layout_name(line + 7)) {
                printf("{\"type\":\"layout\",\"index\":%u}\n", active_layout);
                continue;
            }
            return 0;
        }
        if ((unsigned char)bytes[i] < 32 || used >= sizeof(line) - 1) return 0;
        line[used++] = bytes[i];
    }
    return 1;
}
static int transition(int device, unsigned int code, int down) {
    if (code > KEY_MAX || held[device][code] == down) return 0;
    held[device][code] = down;
    if (down) {
        if (references[code]++ == 0) xkb_state_update_key(state, code + 8, XKB_KEY_DOWN);
    } else if (references[code] && --references[code] == 0) {
        xkb_state_update_key(state, code + 8, XKB_KEY_UP);
    }
    return 1;
}
static void remove_device(int index) {
    for (unsigned int key = 0; key <= KEY_MAX; key++) transition(index, key, 0);
    close(fds[index].fd);
    int last = --count;
    fds[index] = fds[last];
    device_numbers[index] = device_numbers[last];
    memcpy(paths[index], paths[last], sizeof(paths[index]));
    memcpy(held[index], held[last], sizeof(held[index]));
    dropped[index] = dropped[last];
    memset(held[last], 0, sizeof(held[last]));
    dropped[last] = 0;
}
static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping = 1; }
static int hasbit(unsigned long *bits, int bit) {
    return (bits[bit / (8 * sizeof(long))] >> (bit % (8 * sizeof(long)))) & 1;
}
static void sync_device(int index) {
    unsigned long bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    if (ioctl(fds[index].fd, EVIOCGKEY(sizeof(bits)), bits) < 0) return;
    for (unsigned int key = 0; key <= KEY_MAX; key++) transition(index, key, hasbit(bits, key));
    unsigned long leds = 0;
    if (ioctl(fds[index].fd, EVIOCGLED(sizeof(leds)), &leds) >= 0) {
        xkb_mod_mask_t locked = xkb_state_serialize_mods(state, XKB_STATE_MODS_LOCKED);
        const char *names[] = {XKB_MOD_NAME_CAPS, XKB_MOD_NAME_NUM};
        const int indicators[] = {LED_CAPSL, LED_NUML};
        for (int n = 0; n < 2; n++) {
            xkb_mod_index_t mod = xkb_keymap_mod_get_index(xkb_state_get_keymap(state), names[n]);
            if (mod == XKB_MOD_INVALID || mod >= 32) continue;
            if (leds & (1UL << indicators[n])) locked |= 1U << mod;
            else locked &= ~(1U << mod);
        }
        xkb_state_update_mask(state,
            xkb_state_serialize_mods(state, XKB_STATE_MODS_DEPRESSED),
            xkb_state_serialize_mods(state, XKB_STATE_MODS_LATCHED), locked,
            xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_DEPRESSED),
            xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_LATCHED),
            xkb_state_serialize_layout(state, XKB_STATE_LAYOUT_LOCKED));
    }
}
static void discover(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count <= MAX_DEVICES) {
        if (strncmp(entry->d_name, "event", 5) || !entry->d_name[5]) continue;
        int valid = 1;
        for (const char *c = entry->d_name + 5; *c; c++) if (!isdigit((unsigned char)*c)) valid = 0;
        if (!valid) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%.40s", entry->d_name);
        int known = 0;
        for (int i = 1; i < count; i++) if (!strcmp(paths[i], path)) known = 1;
        if (known) continue;
        struct stat before;
        if (!access_session_valid() || access_changed()) { stopping = 1; break; }
        // Check seat BEFORE open, then recheck the actual fd to prevent path replacement.
        if (lstat(path, &before) < 0 || !S_ISCHR(before.st_mode) || !access_device_valid(before.st_rdev)) continue;
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) continue;
        struct stat info;
        if (fstat(fd, &info) < 0 || !S_ISCHR(info.st_mode) ||
            info.st_rdev != before.st_rdev || !access_device_valid(info.st_rdev) || !access_session_valid()) { close(fd); continue; }
        unsigned long bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0 ||
            (!(hasbit(bits, KEY_A) && hasbit(bits, KEY_Z) && hasbit(bits, KEY_SPACE)) &&
             !(hasbit(bits, KEY_KP1) && hasbit(bits, KEY_KP0) && hasbit(bits, KEY_KPENTER)))) {
            close(fd); continue;
        }
        fds[count] = (struct pollfd){fd, POLLIN, 0};
        device_numbers[count] = info.st_rdev;
        strcpy(paths[count], path);
        sync_device(count);
        count++;
    }
    closedir(dir);
}
static void json_key(const char *text, int code) {
    printf("{\"type\":\"key\",\"code\":%d,\"text\":\"", code);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        if (*p >= 32) putchar(*p);
    }
    puts("\"}");
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--self-test")) {
        json_key("A", KEY_A); json_key("\"\\", KEY_BACKSLASH);
        json_key("ї", KEY_Q); return 0;
    }
    if (argc > 4) return 2;
    uid_t caller;
    // PKEXEC_UID is set by pkexec to the invoking user's real UID.
    if (geteuid() != 0 || !parse_caller_uid(getenv("PKEXEC_UID"), &caller)) {
        fputs("Enable through pkexec as a non-root desktop user.\n", stderr); return 1;
    }
    atexit(access_destroy);
    if (!access_init(caller)) {
        fputs("Cannot verify one active local Wayland session and seat for this user.\n", stderr); return 1;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGTERM, stop); signal(SIGINT, stop);
    struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    if (!ctx) return 1;
    struct xkb_rule_names names = {.layout = argc > 1 ? argv[1] : "us",
                                 .options = argc > 2 ? argv[2] : ""};
    struct xkb_keymap *map = xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!map) { fputs("Could not load keyboard layout.\n", stderr); xkb_context_unref(ctx); return 1; }
    state = xkb_state_new(map);
    if (!state) { xkb_keymap_unref(map); xkb_context_unref(ctx); return 1; }
    if (argc > 3) set_layout_name(argv[3]);
    fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};
    discover();
    if (count == 1) { fputs("No accessible keyboards found.\n", stderr); xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(ctx); return 1; }
    if (stopping || access_changed() || !access_session_valid()) {
        for (int i = 1; i < count; i++) close(fds[i].fd);
        xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(ctx);
        fputs("Session ownership changed before keyboard access started.\n", stderr); return 1;
    }
    puts("{\"type\":\"ready\"}");
    struct timespec last_scan;
    clock_gettime(CLOCK_MONOTONIC, &last_scan);
    while (!stopping) {
        struct pollfd waiting[MAX_DEVICES + 2];
        memcpy(waiting, fds, (size_t)count * sizeof(*fds));
        waiting[count] = (struct pollfd){sd_login_monitor_get_fd(access_scope.monitor), POLLIN, 0};
        int result = poll(waiting, count + 1, 250);
        for (int i = 0; i < count; i++) fds[i].revents = waiting[i].revents;
        if (result < 0) { if (errno == EINTR) continue; break; }
        if (waiting[count].revents || !access_session_valid()) {
            fputs("Keyboard access stopped: session ownership changed or is unavailable.\n", stderr); break;
        }
        if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) break;
        if (fds[0].revents & POLLIN) {
            char buffer[256];
            ssize_t length = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (length <= 0 || !control_bytes(buffer, (size_t)length)) break;
        }
        for (int i = 1; i < count && !stopping; i++) {
            if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                remove_device(i--); continue;
            }
            if (!(fds[i].revents & POLLIN)) continue;
            struct input_event ev;
            while (!stopping) {
                if (access_changed() || !access_session_valid() || !access_device_valid(device_numbers[i])) {
                    fputs("Keyboard access stopped: session or device seat changed.\n", stderr);
                    stopping = 1; break;
                }
                if (read(fds[i].fd, &ev, sizeof(ev)) != sizeof(ev)) break;
                // Revalidate after read: never forward queued input across ownership changes.
                if (access_changed() || !access_session_valid() || !access_device_valid(device_numbers[i])) {
                    stopping = 1; break;
                }
                if (ev.type == EV_SYN && ev.code == SYN_DROPPED) { dropped[i] = 1; continue; }
                if (dropped[i]) {
                    if (ev.type == EV_SYN && ev.code == SYN_REPORT) { sync_device(i); dropped[i] = 0; }
                    continue;
                }
                if (ev.type != EV_KEY || ev.code > KEY_MAX) continue;
                // Linux value 2 is auto-repeat: only a new press spawns a drop.
                if (ev.value != 0 && ev.value != 1) continue;
                if (!transition(i, ev.code, ev.value)) continue;
                // Hyprland owns layout changes; do not toggle a second time locally.
                apply_layout();
                xkb_keycode_t code = ev.code + 8;
                if (!ev.value) continue;
                if (ev.code == KEY_ESC &&
                    xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0 &&
                    xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0) {
                    puts("{\"type\":\"stop\"}"); stopping = 1; break;
                }
                char text[128] = {0};
                xkb_state_key_get_utf8(state, code, text, sizeof(text));
                // Keep letters visible when Ctrl produces a control character.
                if (text[0] && ((unsigned char)text[0] < 32 || text[0] == 127))
                    xkb_keysym_to_utf8(xkb_state_key_get_one_sym(state, code), text, sizeof(text));
                if (text[0] && (unsigned char)text[0] >= 32 && text[0] != 127 && strcmp(text, " ")) {
                    json_key(text, ev.code);
                } else {
                    const char *label = NULL;
                    switch (ev.code) {
                        case KEY_SPACE: label = "␣"; break;
                        case KEY_KPENTER:
                        case KEY_ENTER: label = "↵"; break;
                        case KEY_BACKSPACE: label = "⌫"; break;
                        case KEY_TAB: label = "⇥"; break;
                        case KEY_ESC: label = "Esc"; break;
                        case KEY_UP: label = "↑"; break;
                        case KEY_DOWN: label = "↓"; break;
                        case KEY_LEFT: label = "←"; break;
                        case KEY_RIGHT: label = "→"; break;
                        case KEY_HOME: label = "Home"; break;
                        case KEY_END: label = "End"; break;
                        case KEY_PAGEUP: label = "PgUp"; break;
                        case KEY_PAGEDOWN: label = "PgDn"; break;
                        case KEY_INSERT: label = "Ins"; break;
                        case KEY_DELETE: label = "Del"; break;
                        case KEY_KP0: label = "0"; break;
                        case KEY_KP1: label = "1"; break;
                        case KEY_KP2: label = "2"; break;
                        case KEY_KP3: label = "3"; break;
                        case KEY_KP4: label = "4"; break;
                        case KEY_KP5: label = "5"; break;
                        case KEY_KP6: label = "6"; break;
                        case KEY_KP7: label = "7"; break;
                        case KEY_KP8: label = "8"; break;
                        case KEY_KP9: label = "9"; break;
                        case KEY_KPDOT: label = "."; break;
                        default:
                            if (ev.code >= KEY_F1 && ev.code <= KEY_F10) {
                                snprintf(text, sizeof(text), "F%d", ev.code - KEY_F1 + 1); label = text;
                            } else if (ev.code == KEY_F11 || ev.code == KEY_F12) {
                                label = ev.code == KEY_F11 ? "F11" : "F12";
                            }
                            break;
                    }
                    if (label) json_key(label, ev.code);
                }
            }
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - last_scan.tv_sec >= 2) { discover(); last_scan = now; }
    }
    for (int i = 1; i < count; i++) close(fds[i].fd);
    xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(ctx);
    return 0;
}
