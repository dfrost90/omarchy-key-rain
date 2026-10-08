pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import qs.Ui
import qs.Commons

Panel {
    id: root
    moduleName: "io.github.dfrost90.falling-keys"
    ipcTarget: moduleName
    readonly property var effect: bar && bar.shell ? bar.shell.serviceFor(moduleName) : null
    readonly property color foreground: Color.popups.text
    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    function restoreSettings() {
        if (!effect) return;
        effect.setLayout(setting("layout", "matrix"));
        effect.followTheme = setting("followTheme", false) === true;
        effect.setSplitMode(setting("splitMode", false) === true);
        effect.setFontSize(Number(setting("fontSize", 22)));
        effect.setOpacity(Number(setting("opacityPercent", 100)));
        effect.setFallHeight(Number(setting("fallHeightPercent", 100)));
    }
    function saveSettings() {
        if (!effect || !bar || !bar.shell) return;
        var next = Object.assign({}, settings, {
            layout: effect.layout, followTheme: effect.followTheme, splitMode: effect.splitMode, fontSize: effect.fontSize, opacityPercent: effect.opacityPercent, fallHeightPercent: effect.fallHeightPercent
        });
        bar.shell.updateEntryInline(moduleName, next);
    }
    Component.onCompleted: restoreSettings()
    onEffectChanged: restoreSettings()
    onSettingsChanged: restoreSettings()

    WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        text: "A↓"
        fontFamily: root.bar ? root.bar.fontFamily : Style.font.family
        fontSize: Style.font.body
        active: root.opened || !!(root.effect && root.effect.active)
        tooltipText: "Key Rain · " + (root.effect && root.effect.active ? "enabled" : "disabled")
        onPressed: root.toggle()
    }
    KeyboardPanel {
        id: popup
        anchorItem: button
        owner: root
        bar: root.bar
        open: root.opened
        contentWidth: fittedContentWidth(Style.space(380))
        contentHeight: fittedContentHeight(column.implicitHeight, Style.space(740))
        focusTarget: body
        Item {
            id: body
            anchors.fill: parent
            focus: true
            Keys.onEscapePressed: root.close()
            Flickable {
                id: scroll
                anchors.fill: parent
                contentWidth: width
                contentHeight: column.implicitHeight
                clip: true
                interactive: contentHeight > height
                Controls.ScrollBar.vertical: Controls.ScrollBar { policy: Controls.ScrollBar.AsNeeded }
                ColumnLayout {
                    id: column
                    width: scroll.width
                    spacing: Style.space(12)
                    PanelHero {
                        Layout.fillWidth: true
                        title: "Key Rain"
                        meta: root.effect && root.effect.starting ? "Awaiting access" : root.effect && root.effect.active ? "Enabled" : "Disabled"
                        foreground: root.foreground
                        iconSize: Style.font.title
                        iconComponent: Component {
                            Text {
                                text: "󰌌"; color: root.foreground
                                font.family: "FiraCode Nerd Font"; font.pixelSize: Style.font.title
                            }
                        }
                        trailingControl: Component {
                            ToggleSwitch {
                                id: powerSwitch
                                checked: !!(root.effect && (root.effect.active || root.effect.starting))
                                enabled: !!root.effect
                                foreground: root.foreground
                                activeFocusOnTab: true
                                hasCursor: activeFocus
                                Accessible.role: Accessible.CheckBox
                                Accessible.name: "Enable Key Rain"
                                Accessible.checked: checked
                                function activate() { if (root.effect) root.effect.setActive(!(root.effect.active || root.effect.starting)); }
                                onToggled: activate()
                                Keys.onSpacePressed: activate()
                                Keys.onReturnPressed: activate()
                                PanelToolTip {
                                    visible: powerSwitch.containsMouse
                                    text: root.effect && root.effect.starting ? "Cancel keyboard access" : powerSwitch.checked ? "Disable Key Rain" : "Enable Key Rain"
                                }
                            }
                        }
                    }
                    PanelSeparator { Layout.fillWidth: true; foreground: root.foreground }
                    PanelSectionHeader { text: "LAYOUT"; foreground: root.foreground }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.space(8)
                        Repeater {
                            model: root.effect ? root.effect.layouts : []
                            Button {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                text: modelData.name
                                foreground: root.foreground
                                selected: !!root.effect && root.effect.layout === modelData.id
                                bordered: true
                                focusable: true
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: text
                                Accessible.checked: selected
                                onClicked: { root.effect.setLayout(modelData.id); root.saveSettings(); }
                            }
                        }
                    }
                    PanelSectionHeader { text: "FONT SIZE"; foreground: root.foreground }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.space(8)
                        Repeater {
                            model: root.effect ? root.effect.fontSizes : []
                            Button {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                text: modelData.name
                                foreground: root.foreground
                                selected: !!root.effect && root.effect.fontSize === modelData.size
                                bordered: true
                                focusable: true
                                tooltipText: modelData.size + " px"
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: modelData.name + " font, " + modelData.size + " pixels"
                                Accessible.checked: selected
                                onClicked: { root.effect.setFontSize(modelData.size); root.saveSettings(); }
                            }
                        }
                    }
                    PercentControl {
                        Layout.fillWidth: true
                        label: "Opacity"
                        bar: root.bar
                        foreground: root.foreground
                        enabled: !!root.effect
                        value: root.effect ? root.effect.opacityPercent : 100
                        onMoved: value => root.effect.setOpacity(value)
                        onReleased: root.saveSettings()
                    }
                    PercentControl {
                        Layout.fillWidth: true
                        label: "Fall height"
                        bar: root.bar
                        foreground: root.foreground
                        enabled: !!root.effect
                        value: root.effect ? root.effect.fallHeightPercent : 100
                        onMoved: value => root.effect.setFallHeight(value)
                        onReleased: root.saveSettings()
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Distance down the screen. Fades out near the end."
                        color: root.foreground; opacity: 0.8
                        font.family: Style.font.family; font.pixelSize: Style.font.caption
                        wrapMode: Text.Wrap; textFormat: Text.PlainText
                    }
                    PanelSeparator { Layout.fillWidth: true; foreground: root.foreground }
                    Toggle {
                        Layout.fillWidth: true
                        label: "Follow theme"
                        description: "Use the current Omarchy colors."
                        foreground: root.foreground
                        checked: !!(root.effect && root.effect.followTheme)
                        enabled: !!root.effect
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: label
                        Accessible.description: description
                        Accessible.checked: checked
                        onClicked: { root.effect.followTheme = !root.effect.followTheme; root.saveSettings(); }
                    }
                    Toggle {
                        Layout.fillWidth: true
                        label: "Split keyboard"
                        description: "Left-hand letters fall on the left; right-hand letters on the right. Other keys use the full screen."
                        foreground: root.foreground
                        checked: !!(root.effect && root.effect.splitMode)
                        enabled: !!root.effect
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: label
                        Accessible.description: description
                        Accessible.checked: checked
                        onClicked: { root.effect.setSplitMode(!root.effect.splitMode); root.saveSettings(); }
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Preview animation"
                        iconText: "󰐊"
                        foreground: root.foreground
                        bordered: true
                        focusable: true
                        enabled: !!root.effect
                        Accessible.role: Accessible.Button
                        Accessible.name: text
                        onClicked: { root.effect.preview(); root.close(); }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !!root.effect && !root.effect.active && root.effect.status !== "Disabled"
                        text: root.effect ? root.effect.status : ""
                        color: root.foreground
                        font.family: Style.font.family; font.pixelSize: Style.font.bodySmall
                        wrapMode: Text.Wrap; textFormat: Text.PlainText
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Ctrl+Alt+Esc to disable · Letters are click-through.\nPause before typing passwords."
                        color: root.foreground; opacity: 0.8
                        font.family: Style.font.family; font.pixelSize: Style.font.caption
                        wrapMode: Text.Wrap; textFormat: Text.PlainText
                    }
                }
            }
        }
    }
}
