pragma ComponentBehavior: Bound
import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons

Item {
    id: root
    property var shell: null
    property bool active: false
    property bool starting: false
    property string status: "Disabled"
    property string layout: "matrix"
    property string colorMode: "matrix"
    property color customColor: "#36e568"
    readonly property bool followTheme: colorMode === "theme"
    property bool splitMode: false
    property int scrambleDurationMs: 300
    readonly property bool scrambleOnEntry: scrambleDurationMs > 0
    property int fontSize: 22
    property int opacityPercent: 100
    property int fallHeightPercent: 100
    readonly property var fontSizes: [{size: 16, name: "Small"}, {size: 22, name: "Medium"}, {size: 30, name: "Large"}]
    readonly property var layouts: [{id: "matrix", name: "Matrix"}, {id: "cascade", name: "Cascade"}]
    readonly property string readerPath: decodeURIComponent(Qt.resolvedUrl("falling-keys-reader").toString().replace(/^file:\/\//, ""))
    property bool lockSafe: false
    property bool previewPending: false
    readonly property string directory: decodeURIComponent(Qt.resolvedUrl(".").toString().replace(/^file:\/\//, ""))
    property string keyboardLayout: "us"
    property string keyboardOptions: ""
    property string activeKeymap: ""
    property int previewTicks: 0
    signal glyph(string text, int code)
    signal clear()

    IpcHandler {
        target: "falling-keys"
        function preview(): void { root.preview() }
        function disable(): void { root.setActive(false) }
        function enable(): void { root.setActive(true) }
        function setLayout(value: string): void { root.setLayout(value) }
        function setFollowTheme(value: bool): void { root.setColorMode(value ? "theme" : "matrix") }
        function setColorMode(value: string): void { root.setColorMode(value) }
        function setCustomColor(value: string): void { root.setCustomColor(value) }
        function setScrambleDuration(value: int): void { root.setScrambleDuration(value) }
        function setScrambleOnEntry(value: bool): void { root.setScrambleDuration(value ? 300 : 0) }
        function setSplitMode(value: bool): void { root.setSplitMode(value) }
        function setFontSize(value: int): void { root.setFontSize(value) }
        function setOpacity(value: int): void { root.setOpacity(value) }
        function setFallHeight(value: int): void { root.setFallHeight(value) }
        function status(): string { return JSON.stringify({active: root.active, starting: root.starting, layout: root.layout, followTheme: root.followTheme, colorMode: root.colorMode, customColor: root.customColor.toString(), splitMode: root.splitMode, scrambleOnEntry: root.scrambleOnEntry, scrambleDurationMs: root.scrambleDurationMs, fontSize: root.fontSize, opacityPercent: root.opacityPercent, fallHeightPercent: root.fallHeightPercent, status: root.status}) }
    }
    Process {
        id: lockWatcher
        command: ["python3", root.directory + "backend.py", "watch"]
        running: root.active || root.starting || root.previewPending || root.previewTicks > 0
        stdout: SplitParser {
            onRead: line => {
                if (line === "unlocked") {
                    root.lockSafe = true;
                    if (root.starting) builder.running = true;
                    if (root.previewPending) { root.previewTicks = 24; root.previewPending = false; }
                } else {
                    root.setActive(false);
                    root.status = line === "locked" ? "Disabled · screen locked" : "Disabled · cannot verify screen lock";
                }
            }
        }
        onExited: {
            root.lockSafe = false;
            if (root.active || root.starting || root.previewPending) {
                root.setActive(false);
                root.status = "Disabled · lock watcher stopped";
            }
        }
    }
    Process {
        id: builder
        command: ["python3", root.directory + "backend.py", "build"]
        stderr: StdioCollector { onStreamFinished: if (root.starting && text.trim()) root.status = text.trim().slice(-240) }
        onExited: (code, exitStatus) => {
            if (!root.starting) return;
            if (code === 0) layoutProbe.running = true;
            else { root.starting = false; root.status = "Build failed · install gcc, pkgconf and libxkbcommon; see README"; }
        }
    }

    function setActive(value) {
        if (!value) {
            active = false; starting = false; previewTicks = 0; previewPending = false; lockSafe = false;
            // Closing the pipe exits the privileged reader even if it cannot be signalled.
            if (reader.running) reader.write("stop\n");
            reader.running = false;
            clear(); status = "Disabled"; return;
        }
        if (reader.running || starting || builder.running || layoutProbe.running || optionsProbe.running || devicesProbe.running || (lockWatcher.running && !lockSafe)) return;
        starting = true; status = "Requesting keyboard access…";
        if (lockSafe) builder.running = true;
    }
    function setScrambleDuration(value) {
        if (!Number.isFinite(value)) return;
        scrambleDurationMs = Math.max(0, Math.min(1000, Math.round(value / 50) * 50));
    }
    function setColorMode(value) {
        if (["theme", "matrix", "custom"].indexOf(value) >= 0) colorMode = value;
    }
    function setCustomColor(value) {
        if (typeof value === "string" && /^#[0-9a-fA-F]{6}$/.test(value)) customColor = value;
    }
    function setLayout(value) {
        if (!layouts.some(function(item) { return item.id === value })) return;
        if (layout === value) return;
        layout = value; clear();
    }
    function setSplitMode(value) {
        if (splitMode === value) return;
        splitMode = value; clear();
    }
    function setFontSize(value) {
        if (!fontSizes.some(function(item) { return item.size === value })) return;
        if (fontSize === value) return;
        fontSize = value; clear();
    }
    function setOpacity(value) {
        if (!Number.isFinite(value)) return;
        opacityPercent = Math.max(10, Math.min(100, Math.round(value)));
    }
    function setFallHeight(value) {
        if (!Number.isFinite(value)) return;
        fallHeightPercent = Math.max(10, Math.min(100, Math.round(value)));
    }
    function preview() { if (lockSafe) previewTicks = 24; else previewPending = true; }
    function accept(line) {
        try {
            var event = JSON.parse(line);
            if (event.type === "ready" && starting && lockSafe) { active = true; starting = false; status = "Enabled · Ctrl+Alt+Esc to disable"; }
            else if (event.type === "stop") setActive(false);
            else if (event.type === "key" && active && lockSafe && typeof event.text === "string" && event.text.length <= 128 && Number.isInteger(event.code)) glyph(event.text, event.code);
        } catch (error) { status = "Could not read keyboard events"; }
    }
    Process {
        id: layoutProbe
        command: ["hyprctl", "-j", "getoption", "input:kb_layout"]
        stdout: StdioCollector {
            onStreamFinished: {
                try { root.keyboardLayout = JSON.parse(text).str || "us"; } catch (e) { root.keyboardLayout = "us"; }
            }
        }
        onExited: if (root.starting) optionsProbe.running = true
    }
    Process {
        id: optionsProbe
        command: ["hyprctl", "-j", "getoption", "input:kb_options"]
        stdout: StdioCollector {
            onStreamFinished: {
                try { root.keyboardOptions = JSON.parse(text).str || ""; } catch (e) { root.keyboardOptions = ""; }
            }
        }
        onExited: {
            if (!root.starting) return;
            devicesProbe.running = true;
        }
    }
    Process {
        id: devicesProbe
        command: ["hyprctl", "-j", "devices"]
        stdout: StdioCollector {
            onStreamFinished: {
                root.activeKeymap = "";
                try {
                    var keyboards = JSON.parse(text).keyboards || [];
                    var keyboard = keyboards.find(function(k) { return k.main; }) || keyboards[0];
                    if (keyboard) root.activeKeymap = keyboard.active_keymap || "";
                } catch (e) {}
            }
        }
        onExited: {
            if (!root.starting) return;
            reader.command = ["pkexec", root.readerPath, root.keyboardLayout, root.keyboardOptions, root.activeKeymap];
            reader.running = true;
        }
    }
    Process {
        id: reader
        stdinEnabled: true
        stdout: SplitParser { onRead: line => root.accept(line) }
        stderr: StdioCollector { onStreamFinished: if ((root.active || root.starting) && text.trim()) root.status = text.trim().slice(0, 200) }
        onExited: {
            root.active = false; root.starting = false; root.clear();
            if (root.status.indexOf("Enabled") === 0 || root.status.indexOf("Requesting") === 0)
                root.status = "Disabled · keyboard access ended";
        }
    }
    Timer {
        interval: 100; repeat: true; running: root.previewTicks > 0
        onTriggered: {
            root.previewTicks--;
            var letters = "QWERTYUIOPASDFGHJKLZXCVBNM";
            var codes = [16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50];
            var index = Math.floor(Math.random() * letters.length);
            root.glyph(letters.charAt(index), codes[index]);
        }
    }
    Variants {
        model: Quickshell.screens
        PanelWindow {
            id: surface
            required property var modelData
            screen: modelData
            anchors { top: true; bottom: true; left: true; right: true }
            visible: root.previewTicks > 0 || rain.hasParticles
            color: "transparent"
            exclusionMode: ExclusionMode.Ignore
            WlrLayershell.namespace: "omarchy-falling-keys"
            WlrLayershell.layer: WlrLayer.Overlay
            WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
            mask: Region {}
            Rain {
                id: rain
                anchors.fill: parent
                layout: root.layout
                splitMode: root.splitMode
                scrambleDurationMs: root.scrambleDurationMs
                glyphSize: root.fontSize
                intensity: root.opacityPercent / 100
                fallHeight: root.fallHeightPercent / 100
                headColor: root.followTheme ? Color.foreground : root.colorMode === "custom" ? Qt.tint(root.customColor, "#bbffffff") : "#cbffd5"
                trailColor: root.followTheme ? Color.accent : root.colorMode === "custom" ? root.customColor : "#36e568"
            }
            Connections {
                target: root
                function onGlyph(text, code) { rain.spawn(text, code); }
                function onClear() { rain.clear(); }
            }
        }
    }
}
