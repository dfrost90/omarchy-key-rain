#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libudev.h>
#include <systemd/sd-login.h>

static uid_t owner = 1000, active_owner = 1000;
static int active = 1, remote = 0, fail = 0, session_count = 1, monitor_fd;
static const char *seat = "seat0", *current = "s1", *type = "wayland", *kind = "user";
static uint64_t started = 123;
static int fake_uid(const char *s, uid_t *out) { (void)s; *out = owner; return fail ? -EIO : 0; }
static int string_result(char **out, const char *value) { if (fail) return -EIO; *out = strdup(value); return 0; }
static int fake_seat(const char *s, char **out) { (void)s; return string_result(out, seat); }
static int fake_type(const char *s, char **out) { (void)s; return string_result(out, type); }
static int fake_class(const char *s, char **out) { (void)s; return string_result(out, kind); }
static int fake_active(const char *s) { (void)s; return fail ? -EIO : active; }
static int fake_remote(const char *s) { (void)s; return fail ? -EIO : remote; }
static int fake_start(const char *s, uint64_t *out) { (void)s; *out = started; return fail ? -EIO : 0; }
static int fake_seat_active(const char *s, char **out, uid_t *uid) { (void)s; *uid = active_owner; return string_result(out, current); }
static int fake_sessions(uid_t uid, int required, char ***out) {
    (void)uid; assert(required == 1);
    if (fail) return -EIO;
    *out = calloc((size_t)session_count + 1, sizeof(char *));
    for (int i = 0; i < session_count; i++) (*out)[i] = strdup(i ? "s2" : "s1");
    return session_count;
}
static int fake_monitor_new(const char *category, sd_login_monitor **out) {
    assert(!strcmp(category, "session")); *out = NULL; return fail ? -EIO : 0;
}
static int fake_monitor_fd(sd_login_monitor *m) { (void)m; return monitor_fd; }
#define sd_session_get_uid fake_uid
#define sd_session_get_type fake_type
#define sd_session_get_class fake_class
#define sd_session_get_seat fake_seat
#define sd_session_is_active fake_active
#define sd_session_is_remote fake_remote
#define sd_session_get_start_time fake_start
#define sd_seat_get_active fake_seat_active
#define sd_uid_get_sessions fake_sessions
#define sd_login_monitor_new fake_monitor_new
#define sd_login_monitor_get_fd fake_monitor_fd
#include "../SeatAccess.h"

int main(void) {
    uid_t uid = 0;
    assert(parse_caller_uid("1000", &uid) && uid == 1000);
    const char *invalid[] = {NULL, "", "0", "-1", "+1000", "1000x", "4294967295", "99999999999999999999999999"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(*invalid); i++) assert(!parse_caller_uid(invalid[i], &uid));
    assert(!access_device_valid(0));
    assert(access_init(1000)); assert(access_session_valid());
    owner = 1001; assert(!access_session_valid()); owner = 1000;
    active_owner = 1001; assert(!access_session_valid()); active_owner = 1000;
    current = "s2"; assert(!access_session_valid()); current = "s1";
    active = 0; assert(!access_session_valid()); active = 1;
    remote = 1; assert(!access_session_valid()); remote = 0;
    type = "tty"; assert(!access_session_valid()); type = "wayland";
    kind = "greeter"; assert(!access_session_valid()); kind = "user";
    seat = "seat1"; assert(!access_session_valid()); seat = "seat0";
    started++; assert(!access_session_valid()); started--;
    fail = 1; assert(!access_session_valid()); fail = 0;
    assert(access_session_valid());
    assert(device_matches(1, "input", "1", NULL, "seat0"));
    assert(device_matches(1, "input", "1", "seat1", "seat1"));
    assert(!device_matches(1, "input", "1", "seat1", "seat0"));
    assert(!device_matches(1, "input", "1", NULL, "seat1"));
    assert(!device_matches(0, "input", "1", "seat0", "seat0"));
    assert(!device_matches(-EIO, "input", "1", "seat0", "seat0"));
    assert(!device_matches(1, "input", NULL, "seat0", "seat0"));
    assert(!device_matches(1, NULL, "1", "seat0", "seat0"));
    assert(!device_matches(1, "input", "1", "seat0", NULL));
    int pipes[2]; assert(pipe(pipes) == 0); monitor_fd = pipes[0];
    assert(!access_changed()); assert(write(pipes[1], "x", 1) == 1); assert(access_changed());
    close(pipes[0]); close(pipes[1]); assert(access_changed());
    monitor_fd = -1; assert(access_changed());
    access_destroy();
    session_count = 2; assert(!access_init(1000)); access_destroy();
    session_count = 0; assert(!access_init(1000)); access_destroy();
    session_count = 1; fail = 1; assert(!access_init(1000)); access_destroy();
    puts("PASS: caller UID, cross-seat denial, session takeover/reuse, ambiguity, missing metadata, monitor failure");
    return 0;
}
