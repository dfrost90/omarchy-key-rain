pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import qs.Ui
import qs.Commons

ColumnLayout {
    id: root
    property color value: "#36e568"
    property color foreground: Color.popups.text
    signal selected(string value)
    spacing: Style.space(8)
    RowLayout {
        Layout.fillWidth: true
        spacing: Style.space(8)
        Repeater {
            model: ["#36e568", "#55ccff", "#a78bfa", "#ff70ab", "#ffb454", "#ffffff"]
            Button {
                required property string modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: "●"
                foreground: modelData
                selected: root.value.toString() === modelData
                bordered: true
                focusable: true
                tooltipText: modelData.toUpperCase()
                Accessible.role: Accessible.RadioButton
                Accessible.name: "Color " + modelData
                Accessible.checked: selected
                onClicked: root.selected(modelData)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Style.space(8)
        Rectangle {
            Layout.preferredWidth: Style.space(28)
            Layout.preferredHeight: Style.space(28)
            color: root.value
            border.color: root.foreground
            border.width: 1
        }
        TextField {
            id: hex
            Layout.fillWidth: true
            foreground: root.foreground
            text: root.value.toString().toUpperCase()
            maximumLength: 7
            selectByMouse: true
            Accessible.name: "Custom hex color"
            Accessible.description: "Enter a six-digit hexadecimal color, such as #55CCFF"
            validator: RegularExpressionValidator { regularExpression: /#[0-9a-fA-F]{6}/ }
            function commit() {
                if (acceptableInput) root.selected(text);
                text = root.value.toString().toUpperCase();
            }
            onAccepted: commit()
            onEditingFinished: commit()
            Connections {
                target: root
                function onValueChanged() { hex.text = root.value.toString().toUpperCase(); }
            }
        }
    }
}
