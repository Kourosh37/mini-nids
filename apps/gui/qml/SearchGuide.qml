import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
  id: guide
  parent: Overlay.overlay
  anchors.centerIn: parent
  modal: true
  focus: true
  width: Math.min(parent ? parent.width - 24 : 720, 720)
  height: Math.min(parent ? parent.height - 32 : 620, 620)
  standardButtons: Dialog.Close
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
  property string explanation
  property var examples: []
  signal exampleChosen(string query)
  contentItem: ScrollView {
    id: helpScroll
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
      width: helpScroll.availableWidth
      spacing: 16
      Label {
        text: guide.explanation
        color: Theme.text; textFormat: Text.PlainText; wrapMode: Text.WordWrap
        Layout.fillWidth: true; lineHeight: 1.3
        Accessible.name: qsTr("Search instructions")
      }
      Label { text: qsTr("Try an example"); color: Theme.text; font.weight: Font.Bold; visible: guide.examples.length > 0 }
      Repeater {
        model: guide.examples
        ActionButton {
          id: exampleButton
          objectName: "searchExample"
          required property string modelData
          text: modelData; font.family: Theme.codeFontFamily
          Layout.fillWidth: true; Layout.minimumWidth: 0
          implicitHeight: Math.max(48, implicitContentHeight + 16)
          Accessible.name: qsTr("Use example") + ": " + modelData
          contentItem: Label {
            text: exampleButton.text; textFormat: Text.PlainText
            font.family: Theme.codeFontFamily; font.pixelSize: 12
            wrapMode: Text.WrapAnywhere; color: Theme.accent
            verticalAlignment: Text.AlignVCenter
          }
          onClicked: { guide.close(); guide.exampleChosen(modelData) }
          ToolTip.visible: hovered
          ToolTip.text: modelData
        }
      }
      Label {
        visible: guide.examples.length > 0
        text: qsTr("Choose an example to place it in the search bar. You can edit it before refining your search.")
        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
      }
    }
  }
}
