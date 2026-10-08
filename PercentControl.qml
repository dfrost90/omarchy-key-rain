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
    property color foreground: Color.popups.text
    signal moved(int value)
    signal released()
    spacing: Style.space(8)
    RowLayout {
        Layout.fillWidth: true
        PanelSectionHeader { Layout.fillWidth: true; text: root.label.toUpperCase(); foreground: root.foreground }
        Text {
            text: root.value + "%"
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
        maximum: 100
        integer: true
        step: 5
        value: root.value
        activeFocusOnTab: true
        Accessible.role: Accessible.Slider
        Accessible.name: "Falling letters " + root.label.toLowerCase()
        Accessible.description: value + " percent; use arrow keys to adjust"
        onMoved: value => root.moved(Math.round(value))
        onReleased: root.released()
        function adjust(delta) { root.moved(value + delta); root.released(); }
        Keys.onLeftPressed: adjust(-5)
        Keys.onRightPressed: adjust(5)
        Keys.onDownPressed: adjust(-5)
        Keys.onUpPressed: adjust(5)
        Keys.onPressed: event => {
            if (event.key !== Qt.Key_Home && event.key !== Qt.Key_End) return;
            root.moved(event.key === Qt.Key_Home ? root.minimum : 100);
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
