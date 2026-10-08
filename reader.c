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

/* Reads events without grabbing devices; stdin EOF is the lifetime contract.
 * No disk writes, shell commands, network access, or arbitrary file paths. */
#define MAX_DEVICES 64
static struct pollfd fds[MAX_DEVICES + 1];
static char paths[MAX_DEVICES + 1][64];
static int count = 1;
static unsigned char held[MAX_DEVICES + 1][KEY_MAX + 1];
static unsigned short references[KEY_MAX + 1];
static int dropped[MAX_DEVICES + 1];
static struct xkb_state *state;
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
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
        if (fd < 0) continue;
        struct stat info;
        if (fstat(fd, &info) < 0 || !S_ISCHR(info.st_mode)) { close(fd); continue; }
        unsigned long bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0 ||
            (!(hasbit(bits, KEY_A) && hasbit(bits, KEY_Z) && hasbit(bits, KEY_SPACE)) &&
             !(hasbit(bits, KEY_KP1) && hasbit(bits, KEY_KP0) && hasbit(bits, KEY_KPENTER)))) {
            close(fd); continue;
        }
        fds[count] = (struct pollfd){fd, POLLIN, 0};
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
    if (argc > 3) {
        for (xkb_layout_index_t i = 0; i < xkb_keymap_num_layouts(map); i++) {
            const char *name = xkb_keymap_layout_get_name(map, i);
            if (name && !strcmp(name, argv[3])) xkb_state_update_mask(state, 0, 0, 0, 0, 0, i);
        }
    }
    fds[0] = (struct pollfd){STDIN_FILENO, POLLIN, 0};
    discover();
    if (count == 1) { fputs("No accessible keyboards found.\n", stderr); xkb_state_unref(state); xkb_keymap_unref(map); xkb_context_unref(ctx); return 1; }
    puts("{\"type\":\"ready\"}");
    struct timespec last_scan;
    clock_gettime(CLOCK_MONOTONIC, &last_scan);
    while (!stopping) {
        int result = poll(fds, count, 250);
        if (result < 0) { if (errno == EINTR) continue; break; }
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
            break;
        }
        for (int i = 1; i < count; i++) {
            if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                remove_device(i--); continue;
            }
            if (!(fds[i].revents & POLLIN)) continue;
            struct input_event ev;
            while (read(fds[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_SYN && ev.code == SYN_DROPPED) { dropped[i] = 1; continue; }
                if (dropped[i]) {
                    if (ev.type == EV_SYN && ev.code == SYN_REPORT) { sync_device(i); dropped[i] = 0; }
                    continue;
                }
                if (ev.type != EV_KEY || ev.code > KEY_MAX) continue;
                // Linux value 2 is auto-repeat: only a new press spawns a drop.
                if (ev.value != 0 && ev.value != 1) continue;
                if (!transition(i, ev.code, ev.value)) continue;
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
