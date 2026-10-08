import QtQuick
import QtQuick.Layouts
import qs.Ui
import qs.Commons

ColumnLayout {
    id: root
    property var bar: null
    property string label: ""
    property int value: 100
    property int minimum: 10
    property int maximum: 100
    property int step: 5
    property string suffix: "%"
    property string zeroText: ""
    property bool snap: false
    readonly property string valueText: value === 0 && zeroText ? zeroText : value + suffix
    function normalize(value) {
        return Math.max(minimum, Math.min(maximum, snap ? Math.round(value / step) * step : Math.round(value)));
    }
    property color foreground: Color.popups.text
    signal moved(int value)
    signal released()
    spacing: Style.space(8)
    RowLayout {
        Layout.fillWidth: true
        PanelSectionHeader { Layout.fillWidth: true; text: root.label.toUpperCase(); foreground: root.foreground }
        Text {
            text: root.valueText
            color: root.foreground
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            textFormat: Text.PlainText
        }
    }
    PanelSlider {
        id: slider
        Layout.fillWidth: true
        bar: root.bar
        minimum: root.minimum
        maximum: root.maximum
        integer: true
        step: root.step
        value: root.value
        activeFocusOnTab: true
        Accessible.role: Accessible.Slider
        Accessible.name: "Falling letters " + root.label.toLowerCase()
        Accessible.description: root.valueText + "; use arrow keys to adjust"
        onMoved: value => root.moved(root.normalize(value))
        onReleased: root.released()
        function adjust(delta) { root.moved(root.normalize(value + delta)); root.released(); }
        Keys.onLeftPressed: adjust(-root.step)
        Keys.onRightPressed: adjust(root.step)
        Keys.onDownPressed: adjust(-root.step)
        Keys.onUpPressed: adjust(root.step)
        Keys.onPressed: event => {
            if (event.key !== Qt.Key_Home && event.key !== Qt.Key_End) return;
            root.moved(event.key === Qt.Key_Home ? root.minimum : root.maximum);
            root.released();
            event.accepted = true;
        }
        BorderSurface {
            anchors.fill: parent
            visible: slider.activeFocus
            color: "transparent"
            radius: Style.cornerRadius
            borderSpec: Border.controlSpec("focus", root.foreground, Color.accent)
        }
    }
}
