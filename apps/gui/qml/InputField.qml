import QtQuick
import QtQuick.Controls

TextField {
  id: control
  implicitHeight: 48
  leftPadding: 14
  rightPadding: 14
  selectByMouse: true
  color: Theme.text
  placeholderTextColor: Theme.muted
  selectionColor: Theme.accent
  selectedTextColor: Theme.background
  font.family: Theme.codeFontFamily
  font.pixelSize: 12
  hoverEnabled: true
  opacity: enabled ? 1 : 0.5
  background: Rectangle {
    radius: 8
    color: Theme.background
    border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.muted : Theme.border
    border.width: control.activeFocus ? 2 : 1
    Behavior on border.color {
      ColorAnimation {
        duration: 120
      }
    }
  }
}
