import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
  property string title
  property string value: "—"
  property string caption
  property color valueColor: Theme.text
  implicitHeight: 122
  color: Theme.surface
  radius: Theme.radius
  border.color: Theme.border
  ColumnLayout {
    anchors.fill: parent; anchors.margins: 18; spacing: 6
    Label { text: title; color: Theme.muted; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
    Label { text: value; color: valueColor; font.family: Theme.codeFontFamily; font.pixelSize: 28; font.weight: Font.Medium; Accessible.name: title + ": " + value }
    Label { text: caption; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
  }
}
