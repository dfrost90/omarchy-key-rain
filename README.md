# Key Rain for Omarchy

*A little cinema. In every keystroke.*

![Key Rain running over a clean demo terminal](preview.png)

Matrix-inspired keystroke rain with a native Omarchy control panel. The transparent
Wayland overlay is click-through: windows underneath remain fully interactive.
Each physical key press creates one drop; holding a key does not repeat it.

## Controls

| Control | Options |
| --- | --- |
| Header switch | Enable or stop keyboard input |
| Layout | Matrix or Cascade |
| Font size | 16, 22, or 30 px |
| Opacity | 10–100% |
| Fall height | 10–100% of each screen; fades over the final 20% |
| Color | Theme, classic Matrix green, or Custom swatches/hex color |
| Scramble | Off–1000 ms in 50 ms steps; defaults to 300 ms |
| Split keyboard | Left/right letter positions fall on the corresponding screen half |
| Preview | Sample letters without keyboard access or authentication |

The leading symbol scrambles, then settles on the pressed key. On short falls,
scrambling is shortened so the key can settle before disappearing. Trail symbols
keep cycling independently. Number-row, numpad, and other non-letter keys use
the full screen in split mode. Placement follows physical QWERTY positions,
independent of the selected language.

Preferences save automatically. Tab navigates controls; Space/Enter selects;
arrows adjust sliders; Home/End select their endpoints; Escape closes the panel.
The effect starts disabled after a shell restart.

## Keyboard access and privacy

Enabling starts a locally compiled reader through `pkexec`. Your system may ask
for a password or fingerprint to authorize access to physical keyboard events.
Key Rain does not receive fingerprint data. Preview needs no authorization.

**Pause before typing passwords:** global input includes application password
fields. Events are transient, never saved to disk or sent over a network. The
reader does not grab devices. Disabling, Ctrl+Alt+Esc, or closing its input pipe
stops it. Detected screen lock or an unknown lock state also disables the effect;
lock monitoring is polling, not instantaneous.

The reader verifies the invoking user through pkexec and requires exactly one
active, local Wayland user session in logind. It opens only initialized keyboard
devices assigned to that session’s udev seat. Other users’ seats are excluded.
Session metadata changes stop the reader conservatively; re-enable afterward.
Missing ownership data or multiple active Wayland sessions for the same user
prevent enabling. See [SECURITY.md](SECURITY.md).

## Install

Requires Omarchy with the plugin API, Hyprland with `solitaryBlockedBy` monitor
state, Python 3, GCC, pkgconf, libxkbcommon (including headers), and polkit
(`pkexec`), systemd-logind, and the libsystemd/libudev libraries and headers.
Install missing dependencies with Omarchy's package manager first.
No dependencies are downloaded automatically, and no compiled executable is
included in the repository.

Download or clone [this repository](https://github.com/dfrost90/omarchy-key-rain).
From its directory, as your normal desktop user:

```sh
python3 install.py
omarchy-shell shell rescanPlugins
omarchy plugin enable io.github.dfrost90.falling-keys
```

The installer builds locally and copies only plugin files into your Omarchy
configuration. It does not overwrite `shell.json`. A direct plugin-directory
installation also builds the reader on first enable. For build diagnostics,
run `python3 backend.py build` there.

## Update and remove

To update, obtain the newer source, run `python3 install.py`, then
`omarchy restart shell` to reload live service code. Your preferences remain
saved; enable the effect again when ready.

To remove:

```sh
omarchy-shell falling-keys disable
omarchy plugin disable io.github.dfrost90.falling-keys
```

Delete `~/.config/omarchy/plugins/io.github.dfrost90.falling-keys` (or its
equivalent under `$XDG_CONFIG_HOME`), then run `omarchy-shell shell rescanPlugins`.
Remove any remaining bar entry through Omarchy's bar editor. There are no system
services or privilege policies to remove.

The permanent ID and `falling-keys` IPC target retain the original Falling Keys
name for compatibility. Old theme and scramble toggles migrate automatically.

## Development

```sh
python3 tests/check.py
QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME= QT_QUICK_CONTROLS_STYLE=Basic /usr/lib/qt6/bin/qmltestrunner -input tests
omarchy plugin validate .
```

Tests additionally require Node.js and Qt's QML test runner. They cover reader
state/JSON, English/Ukrainian/Russian switching, seat/session ownership denial,
lock-state handling, split placement, fades, scramble duration, and
short falls. The C tests use AddressSanitizer and UndefinedBehaviorSanitizer.

`Service.qml` owns input and shared settings; `Rain.qml` renders particles;
`Layouts.js` handles placement/fades; `BarWidget.qml`, `ColorPicker.qml`, and
`PercentControl.qml` implement the panel. `backend.py` builds and watches lock
state without privileges; only `reader.c` runs through `pkexec`. `SeatAccess.h`
enforces the privileged reader’s logind/udev ownership checks.

IPC: `omarchy-shell falling-keys status`, `preview`, `enable`, `disable`,
`setLayout cascade`, `setColorMode custom`, `setCustomColor '#55ccff'`, or
`setScrambleDuration 300`. IPC settings apply live; panel changes persist them.

Rendering targets 30 FPS with at most 96 streams per monitor. Keyboard configuration
is captured when enabling; re-enable after changes. IME composition, per-device
remappings are not fully tracked. Keyboard language changes follow Hyprland live,
including switches from the bar. Virtual input-method keyboards are excluded
when choosing the initial language. Changing the configured list of layouts
still requires re-enabling.

Licensed under [MIT](LICENSE). Marketplace approval requires maintainer review
of privileged keyboard access and the installer.
