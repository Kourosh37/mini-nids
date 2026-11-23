import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
  id: capture
  objectName: "capturePage"
  signal browseRequested
  signal outputRequested
  signal truthRequested
  readonly property bool compact: availableWidth < 620
  background: null
  contentItem: ScrollView {
    id: viewport
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
      width: viewport.availableWidth
      spacing: 18
      Label {
        text: qsTr("Start with a capture")
        color: Theme.text
        font.pixelSize: 26
        font.weight: Font.Bold
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
      }
      Label {
        text: qsTr("Choose a file, review your options, and let Mini-NIDS find the evidence.")
        color: Theme.muted
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
      }

      Rectangle {
        Layout.fillWidth: true
        implicitHeight: inputContent.implicitHeight + 40
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
          id: inputContent
          anchors.fill: parent
          anchors.margins: 20
          spacing: 14
          RowLayout {
            Layout.fillWidth: true
            Label {
              text: "01"
              color: Theme.accent
              font.family: Theme.codeFontFamily
              font.weight: Font.Bold
            }
            Label {
              text: qsTr("Select a capture")
              color: Theme.text
              font.pixelSize: 17
              font.weight: Font.Bold
              Layout.fillWidth: true
            }
            Label {
              visible: !capture.compact
              text: "PCAP / ZIP"
              color: Theme.muted
              font.family: Theme.codeFontFamily
              font.pixelSize: 11
            }
          }
          Label {
            text: qsTr("Open a PCAP directly or choose a ZIP containing multiple captures. Your files stay on this device.")
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
          }
          InputField {
            objectName: "captureInput"
            text: controller.inputFile
          onTextEdited: if (text !== controller.inputFile)
              controller.inputFile = text
            Layout.fillWidth: true
            enabled: !controller.busy
            placeholderText: qsTr("Path to a .pcap or .zip file")
            Accessible.name: qsTr("Capture file path")
          }
          ActionButton {
            text: controller.inputFile.length ? qsTr("Choose another file") : qsTr("Choose capture file")
            primary: !controller.inputFile.length
            Layout.fillWidth: capture.compact
            enabled: !controller.busy
            onClicked: capture.browseRequested()
          }
          ArchivePicker {
            Layout.fillWidth: true
            visible: controller.archiveInput
          }
        }
      }

      Rectangle {
        Layout.fillWidth: true
        implicitHeight: outputContent.implicitHeight + 40
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
          id: outputContent
          anchors.fill: parent
          anchors.margins: 20
          spacing: 14
          RowLayout {
            Label {
              text: "02"
              color: Theme.accent
              font.family: Theme.codeFontFamily
              font.weight: Font.Bold
            }
            Label {
              text: qsTr("Choose where reports go")
              color: Theme.text
              font.pixelSize: 17
              font.weight: Font.Bold
              Layout.fillWidth: true
              wrapMode: Text.WordWrap
            }
          }
          Label {
            text: qsTr("Each analysis creates its own session folder. Your original capture is kept intact.")
            color: Theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
          }
          InputField {
            text: controller.outputRoot
            onEditingFinished: controller.outputRoot = text
            enabled: !controller.busy
            Layout.fillWidth: true
            Accessible.name: qsTr("Report destination")
          }
          ActionButton {
            text: qsTr("Change report folder")
            Layout.fillWidth: capture.compact
            enabled: !controller.busy
            onClicked: capture.outputRequested()
          }
          ActionButton {
            id: referenceToggle
            text: (checked ? "−  " : "+  ") + qsTr("Reference labels (optional)")
            checkable: true
            checked: controller.groundTruth.length > 0
            flat: true
            Layout.fillWidth: true
            implicitHeight: 44
            Accessible.description: qsTr("Expand optional ground-truth settings")
          }
          ColumnLayout {
            visible: referenceToggle.checked
            Layout.fillWidth: true
            spacing: 10
            Label {
              text: qsTr("Import labeled attack intervals to calculate precision and recall. You can analyze without them.")
              color: Theme.muted
              wrapMode: Text.WordWrap
              Layout.fillWidth: true
            }
            Label {
              visible: controller.groundTruth.length > 0
              text: controller.groundTruth
              textFormat: Text.PlainText
              color: Theme.text
              wrapMode: Text.WrapAnywhere
              Layout.fillWidth: true
              font.family: Theme.codeFontFamily
              font.pixelSize: 12
            }
            GridLayout {
              columns: capture.compact ? 1 : 2
              Layout.fillWidth: true
              ActionButton {
                text: controller.groundTruth.length ? qsTr("Replace reference labels") : qsTr("Import reference labels")
                enabled: !controller.busy
                Layout.fillWidth: capture.compact
                onClicked: capture.truthRequested()
              }
              ActionButton {
                text: qsTr("Remove labels")
                visible: controller.groundTruth.length > 0
                enabled: !controller.busy
                Layout.fillWidth: capture.compact
                onClicked: controller.groundTruth = ""
              }
            }
          }
        }
      }

      ColumnLayout {
        visible: controller.recentFiles.length > 0
        Layout.fillWidth: true
        spacing: 8
        Label {
          text: qsTr("Recent captures")
          color: Theme.text
          font.pixelSize: 17
          font.weight: Font.DemiBold
        }
        Repeater {
          model: controller.recentFiles
          ItemDelegate {
            required property string modelData
            Layout.fillWidth: true
            implicitHeight: 48
            enabled: !controller.busy
            hoverEnabled: true
            contentItem: Label {
              text: modelData
              elide: Text.ElideMiddle
              textFormat: Text.PlainText
              color: Theme.muted
              verticalAlignment: Text.AlignVCenter
              font.family: Theme.codeFontFamily
              font.pixelSize: 12
            }
            background: Rectangle {
              color: parent.hovered ? Theme.hover : Theme.surface
              radius: 8
              border.color: parent.visualFocus ? Theme.accent : Theme.border
            }
            onClicked: controller.inputFile = modelData
            Accessible.name: qsTr("Reopen") + " " + modelData
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: modelData
          }
        }
      }
      Item {
        implicitHeight: 8
      }
    }
  }
  footer: Rectangle {
    implicitHeight: actionContent.implicitHeight + 32
    radius: Theme.radius
    color: Theme.selected
    border.color: controller.canAnalyze ? Theme.accent : Theme.border
    ColumnLayout {
      id: actionContent
      anchors.fill: parent
      anchors.margins: 16
      spacing: 12
      Label {
        text: controller.busy ? controller.status : controller.canAnalyze ? qsTr("Ready to analyze your capture") : controller.archiveInput ? qsTr("Prepare a capture from the ZIP to continue") : qsTr("Choose a capture file to get started")
        textFormat: Text.PlainText
        color: Theme.text
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        font.weight: Font.DemiBold
        Accessible.role: Accessible.StaticText
      }
      ProgressBar {
        id: taskProgress
        visible: controller.busy
        value: controller.progress
        Layout.fillWidth: true
        Accessible.name: qsTr("Task progress")
        background: Rectangle {
          implicitHeight: 8
          radius: 4
          color: Theme.progressTrack
        }
        contentItem: Item {
          implicitHeight: 8
          Rectangle {
            width: parent.width * taskProgress.visualPosition
            height: parent.height
            radius: 4
            color: Theme.progressFill
          }
        }
      }
      GridLayout {
        columns: capture.compact ? 1 : 2
        Layout.fillWidth: true
        columnSpacing: 12
        rowSpacing: 10
        ActionButton {
          objectName: "analyzeButton"
          text: controller.busy ? qsTr("Working…") : qsTr("Start analysis")
          primary: true
          enabled: controller.canAnalyze
          Layout.fillWidth: capture.compact
          implicitWidth: 220
          onClicked: controller.start()
        }
        ActionButton {
          text: qsTr("Cancel task")
          visible: controller.busy
          destructive: true
          Layout.fillWidth: capture.compact
          onClicked: controller.cancel()
        }
      }
    }
  }
}
