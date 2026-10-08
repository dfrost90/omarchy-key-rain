# Security and privacy

This plugin displays global keyboard input, including input into password
fields in applications. Disable it before entering secrets. It does not
record events to files, send them over a network, or grab input devices.
The shell receives transient UTF-8 key labels over a private process pipe.
The reader accepts only bounded layout-name updates matching its loaded keymap
on stdin; stop, EOF, or invalid control input terminates it. These updates
follow Hyprland events and do not grant any additional permissions.

Only `reader.c` runs with elevated privileges, through a graphical `pkexec`
authentication prompt. Before opening any device it derives the invoking UID from pkexec's
`PKEXEC_UID`, rejects root/direct unprivileged invocation, and requires exactly
one active, local, `wayland`/`user` session belonging to that UID. Session ID,
start time and seat are captured from logind; the seat's active session and UID
must also match. Caller-supplied seat/session identifiers are not accepted.

Only initialized udev `input` devices with `ID_INPUT_KEYBOARD=1` whose `ID_SEAT`
matches the captured session seat are eligible. An absent `ID_SEAT` means
`seat0`, following udev's convention; missing device/database information fails
closed. The reader checks the device number and seat before open and checks the
opened descriptor again. It opens `/dev/input/eventN` character devices read-only
with `O_NOFOLLOW`; paths cannot redirect it to another seat's device.

A logind session monitor is attached before the ownership snapshot. Any session
notification stops the reader conservatively, even if unrelated to this user;
there is no automatic resume after switching back. Ownership is also checked
before/after each event read, on every poll wake, and before hotplug discovery.
Each event's device seat is rechecked from udev before/after reading. Unknown
ownership, changed UID/session/seat/start time, invalid device metadata or monitor
failure stops delivery and closes all devices. Session and udev checks are
userspace checks, not a kernel-atomic session lease; no claim of atomic isolation
across an administrator reassigning devices is made. A session lock stays subject
to the lock-watcher timing described below.
There is no passwordless policy, system service, or persistent root installation.
The auditable C source is compiled locally as the desktop user; the release
does not include an executable binary. Review the source before authorizing it.

Closing the reader's stdin, disabling the effect, unloading the plugin, or
Ctrl+Alt+Esc ends input reading. A separate unprivileged Python watcher checks
Hyprland's lock state at 500 ms intervals through its local control socket.
Lock, unknown state, or watcher failure disables the effect. This polling is
not an atomic privacy boundary: transitions can take up to the polling interval
plus a one-second socket timeout. The compositor itself covers the overlay
while session-locked. Enabling requires an initial unlocked response.

Marketplace security review is expected because this plugin uses privilege
and includes an installer. A clean static scan is not a security certification.
Report security issues privately to the repository maintainer; do not include
captured keystrokes or other private input in public reports.
