import QtQuick
import QtQuick.Controls

Button {
  id: control
  property bool primary: false
  property bool destructive: false
  hoverEnabled: true
  implicitHeight: 48
  implicitWidth: Math.max(120, implicitContentWidth + leftPadding + rightPadding)
  leftPadding: 20
  rightPadding: 20
  font.family: Theme.uiFontFamily
  font.pixelSize: 13
  font.weight: Font.Bold
  opacity: enabled ? 1 : 0.42
  scale: down ? 0.97 : 1
  Behavior on scale {
    NumberAnimation {
      duration: 110
      easing.type: Easing.OutCubic
    }
  }
  Behavior on opacity {
    NumberAnimation {
      duration: 120
    }
  }
  contentItem: Text {
    text: control.text
    font: control.font
    color: control.primary ? "#09221e" : control.destructive ? Theme.danger : control.flat ? Theme.accent : Theme.text
    horizontalAlignment: control.flat ? Text.AlignLeft : Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
  }
  background: Rectangle {
    radius: 9
    color: control.primary ? (control.down ? "#43b8a9" : control.hovered ? "#8aebdd" : Theme.accent) : control.down ? Theme.selected : control.hovered ? Theme.hover : control.flat ? "transparent" : Theme.surface
    border.width: control.visualFocus ? 2 : 1
    border.color: control.visualFocus ? Theme.text : control.primary ? "transparent" : control.destructive ? Theme.danger : control.hovered ? Theme.accent : control.flat ? "transparent" : Theme.border
    Behavior on color {
      ColorAnimation {
        duration: 120
      }
    }
    Rectangle {
      id: flash
      anchors.fill: parent
      radius: parent.radius
      color: "#ffffff"
      opacity: 0
    }
  }
  HoverHandler {
    cursorShape: Qt.PointingHandCursor
    enabled: control.enabled
  }
  Connections {
    target: control
    function onClicked() {
      feedback.restart();
    }
  }
  SequentialAnimation {
    id: feedback
    PropertyAction {
      target: flash
      property: "opacity"
      value: 0.22
    }
    NumberAnimation {
      target: flash
      property: "opacity"
      to: 0
      duration: 240
      easing.type: Easing.OutCubic
    }
  }
}
