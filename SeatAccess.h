#ifndef KEY_RAIN_SEAT_ACCESS_H
#define KEY_RAIN_SEAT_ACCESS_H

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <libudev.h>
#include <systemd/sd-login.h>
#include <stdint.h>
#include <limits.h>

/* All authority comes from pkexec, logind and udev, never a UI-supplied seat. */
struct seat_access {
    uid_t uid;
    char *session;
    char *seat;
    uint64_t started;
    struct udev *udev;
    sd_login_monitor *monitor;
};
static struct seat_access access_scope;

static int parse_caller_uid(const char *text, uid_t *uid) {
    if (!text || !*text) return 0;
    for (const char *p = text; *p; p++) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (errno || *end || !value || value >= UINT_MAX || (unsigned long)(uid_t)value != value) return 0;
    *uid = (uid_t)value;
    return 1;
}

static int session_matches(uid_t expected, uid_t actual, int active, int remote,
                           const char *type, const char *kind, const char *seat) {
    return expected == actual && active > 0 && remote == 0 && type &&
        !strcmp(type, "wayland") && kind && !strcmp(kind, "user") && seat && *seat;
}

static int session_details(const char *session, uid_t uid, char **seat) {
    uid_t owner;
    char *type = NULL, *kind = NULL, *found_seat = NULL;
    int ok = sd_session_get_uid(session, &owner) >= 0 &&
        sd_session_get_type(session, &type) >= 0 &&
        sd_session_get_class(session, &kind) >= 0 &&
        sd_session_get_seat(session, &found_seat) >= 0 &&
        session_matches(uid, owner, sd_session_is_active(session),
                        sd_session_is_remote(session), type, kind, found_seat);
    free(type); free(kind);
    if (ok) *seat = found_seat;
    else free(found_seat);
    return ok;
}

static void access_destroy(void) {
    free(access_scope.session); free(access_scope.seat);
    udev_unref(access_scope.udev);
    sd_login_monitor_unref(access_scope.monitor);
    memset(&access_scope, 0, sizeof(access_scope));
}

static int access_session_valid(void) {
    char *seat = NULL, *active_session = NULL;
    uid_t active_uid = (uid_t)-1;
    uint64_t started = 0;
    int ok = access_scope.session && access_scope.seat &&
        session_details(access_scope.session, access_scope.uid, &seat) &&
        !strcmp(seat, access_scope.seat) &&
        sd_session_get_start_time(access_scope.session, &started) >= 0 && started == access_scope.started &&
        sd_seat_get_active(access_scope.seat, &active_session, &active_uid) >= 0 &&
        active_session && !strcmp(active_session, access_scope.session) && active_uid == access_scope.uid;
    free(seat); free(active_session);
    return ok;
}

static int access_init(uid_t uid) {
    access_scope.uid = uid;
    /* Monitor before taking the snapshot; any intervening session change aborts. */
    if (sd_login_monitor_new("session", &access_scope.monitor) < 0) return 0;
    char **sessions = NULL;
    int total = sd_uid_get_sessions(uid, 1, &sessions), matches = 0;
    for (int i = 0; i < total; i++) {
        char *seat = NULL;
        if (session_details(sessions[i], uid, &seat)) {
            matches++;
            free(access_scope.session); free(access_scope.seat);
            access_scope.session = strdup(sessions[i]);
            access_scope.seat = seat;
        }
        free(sessions[i]);
    }
    free(sessions);
    if (matches != 1 || !access_scope.session || !access_scope.seat) return 0;
    if (sd_session_get_start_time(access_scope.session, &access_scope.started) < 0) return 0;
    access_scope.udev = udev_new();
    return access_scope.udev && access_session_valid();
}

static int device_matches(int initialized, const char *subsystem, const char *keyboard,
                          const char *device_seat, const char *session_seat) {
    /* udev's documented default seat is seat0 when ID_SEAT is absent. */
    const char *seat = device_seat && *device_seat ? device_seat : "seat0";
    return initialized > 0 && subsystem && !strcmp(subsystem, "input") &&
        keyboard && !strcmp(keyboard, "1") && session_seat && !strcmp(seat, session_seat);
}

static int access_device_valid(dev_t number) {
    if (!access_scope.udev) return 0;
    struct udev_device *device = udev_device_new_from_devnum(access_scope.udev, 'c', number);
    if (!device) return 0;
    int ok = device_matches(udev_device_get_is_initialized(device),
        udev_device_get_subsystem(device),
        udev_device_get_property_value(device, "ID_INPUT_KEYBOARD"),
        udev_device_get_property_value(device, "ID_SEAT"), access_scope.seat);
    udev_device_unref(device);
    return ok;
}

static int access_changed(void) {
    struct pollfd monitor = {sd_login_monitor_get_fd(access_scope.monitor), POLLIN, 0};
    if (monitor.fd < 0) return 1;
    int result = poll(&monitor, 1, 0);
    return result != 0; /* Includes monitoring errors; re-enable after changes. */
}
#endif
