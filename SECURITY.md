# Security and privacy

This plugin displays global keyboard input, including input into password
fields in applications. Disable it before entering secrets. It does not
record events to files, send them over a network, or grab input devices.
The shell receives transient UTF-8 key labels over a private process pipe.

Only `reader.c` runs with elevated privileges, through a graphical `pkexec`
authentication prompt. It opens `/dev/input/eventN` character devices read-only.
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
