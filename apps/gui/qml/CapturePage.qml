import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
  id: capture
  objectName: "capturePage"
  signal browseRequested
  signal outputRequested
  signal truthRequested
  readonly property bool compact: width < 620
  background: null

  header: Rectangle {
    objectName: "captureSourcePanel"
    implicitHeight: sourceContent.implicitHeight + 28
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    ColumnLayout {
      id: sourceContent
      anchors.fill: parent
      anchors.margins: 14
      spacing: 10
      RowLayout {
        Label {
          text: qsTr("Capture source")
          color: Theme.text
          font.pixelSize: 17
          font.weight: Font.Bold
          Layout.fillWidth: true
        }
        ActionButton {
          id: recentButton
          text: qsTr("Recent")
          flat: true
          implicitWidth: 80
          implicitHeight: 44
          leftPadding: 10
          rightPadding: 10
          visible: controller.recentFiles.length > 0
          enabled: !controller.busy
          onClicked: recentMenu.popup()
          Menu {
            id: recentMenu
            y: recentButton.height
            width: Math.min(capture.width - 28, 420)
            Repeater {
              model: controller.recentFiles
              MenuItem {
                required property string modelData
                text: modelData
                enabled: !controller.busy
                font.family: Theme.codeFontFamily
                font.pixelSize: 12
                onTriggered: controller.inputFile = modelData
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: modelData
              }
            }
          }
        }
        ActionButton {
          objectName: "captureOptionsButton"
          text: qsTr("Options")
          flat: true
          implicitWidth: 88
          implicitHeight: 44
          leftPadding: 10
          rightPadding: 10
          enabled: !controller.busy
          onClicked: options.open()
        }
      }
      RowLayout {
        Layout.fillWidth: true
        spacing: 10
        InputField {
          objectName: "captureInput"
          text: controller.inputFile
          onTextEdited: if (text !== controller.inputFile)
            controller.inputFile = text
          placeholderText: qsTr("Choose a .pcap or .zip file")
          enabled: !controller.busy
          Layout.fillWidth: true
          Accessible.name: qsTr("Capture file path")
        }
        ActionButton {
          text: qsTr("Browse")
          implicitWidth: 100
          enabled: !controller.busy
          onClicked: capture.browseRequested()
        }
      }
    }
  }

  contentItem: Item {
    clip: true
    StackLayout {
      anchors.fill: parent
      anchors.topMargin: 14
      anchors.bottomMargin: 14
      currentIndex: controller.archiveInput ? 1 : 0
      Item {
        id: emptyState
        ColumnLayout {
          anchors.centerIn: parent
          width: Math.min(parent.width - 32, 420)
          spacing: 14
          Image {
            visible: emptyState.height > 260
            source: "qrc:/icons/org.mininids.MiniNIDS.png"
            sourceSize.width: 64
            sourceSize.height: 64
            Layout.alignment: Qt.AlignHCenter
          }
          Label {
            text: controller.inputFile.length ? qsTr("PCAP selected") : qsTr("Open a capture to begin")
            color: Theme.text
            font.pixelSize: 22
            font.weight: Font.Bold
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
          }
          Label {
            text: controller.inputFile.length ? qsTr("Start analysis to explore packets, flows and alerts.") : qsTr("Choose a PCAP, or open a ZIP and prepare one capture. Your files stay on this device.")
            color: Theme.muted
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
          }
          ActionButton {
            text: qsTr("Choose capture")
            primary: true
            visible: !controller.inputFile.length
            enabled: !controller.busy
            Layout.alignment: Qt.AlignHCenter
            onClicked: capture.browseRequested()
          }
        }
      }
      ArchivePicker {
        id: archiveFiles
        Layout.fillWidth: true
        Layout.fillHeight: true
      }
    }
  }

  footer: Rectangle {
    objectName: "captureActionPanel"
    implicitHeight: actionContent.implicitHeight + 28
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    ColumnLayout {
      id: actionContent
      anchors.fill: parent
      anchors.margins: 14
      spacing: 10
      ProgressBar {
        id: taskProgress
        visible: controller.busy
        value: controller.progress
        Layout.fillWidth: true
        padding: 0
        Accessible.name: qsTr("Task progress")
        background: Rectangle {
          implicitHeight: 6
          radius: 3
          color: Theme.progressTrack
        }
        contentItem: Item {
          implicitHeight: 6
          Rectangle {
            width: parent.width * taskProgress.visualPosition
            height: parent.height
            radius: 3
            color: Theme.progressFill
          }
        }
      }
      GridLayout {
        Layout.fillWidth: true
        columns: capture.compact ? 1 : 2
        columnSpacing: 16
        rowSpacing: 10
        ColumnLayout {
          Layout.fillWidth: true
          spacing: 4
          Label {
            text: controller.inputFile.length ? controller.status : qsTr("Choose a capture to continue")
            textFormat: Text.PlainText
            color: Theme.text
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            maximumLineCount: 2
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            Accessible.description: controller.status
            ToolTip.visible: statusHover.hovered
            ToolTip.text: controller.status
            ToolTip.delay: 600
            HoverHandler {
              id: statusHover
            }
          }
          Label {
            text: controller.archiveInput && archiveFiles.selectedName.length ? qsTr("Selected:") + " " + archiveFiles.selectedName : qsTr("Reports:") + " " + controller.outputRoot
            textFormat: Text.PlainText
            color: Theme.muted
            font.pixelSize: 11
            Layout.fillWidth: true
            elide: Text.ElideMiddle
          }
        }
        RowLayout {
          Layout.fillWidth: capture.compact
          Layout.alignment: Qt.AlignRight
          ActionButton {
            objectName: "analyzeButton"
            text: qsTr("Start analysis")
            primary: true
            visible: !controller.busy && (!controller.archiveInput || archiveFiles.selectedReady)
            enabled: controller.canAnalyze
            implicitWidth: 180
            Layout.fillWidth: capture.compact
            onClicked: controller.start()
          }
          ActionButton {
            objectName: "prepareCaptureButton"
            text: qsTr("Prepare capture")
            primary: true
            visible: !controller.busy && controller.archiveInput && !archiveFiles.selectedReady
            enabled: archiveFiles.canPrepare
            implicitWidth: 180
            Layout.fillWidth: capture.compact
            onClicked: archiveFiles.prepareSelected()
          }
          ActionButton {
            objectName: "cancelTaskButton"
            text: qsTr("Cancel task")
            destructive: true
            visible: controller.busy
            implicitWidth: 180
            Layout.fillWidth: capture.compact
            onClicked: controller.cancel()
          }
        }
      }
    }
  }

  Dialog {
    id: options
    objectName: "captureOptionsDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    title: qsTr("Analysis options")
    modal: true
    width: Math.min(560, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 32)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    contentItem: ScrollView {
      id: optionsScroll
      clip: true
      contentWidth: availableWidth
      implicitHeight: Math.min(optionFields.implicitHeight, 400)
      ColumnLayout {
        id: optionFields
        width: optionsScroll.availableWidth
        spacing: 12
        Label {
          text: qsTr("Report folder")
          color: Theme.text
          font.weight: Font.Bold
        }
        InputField {
          objectName: "reportOutputInput"
          text: controller.outputRoot
          enabled: !controller.busy
          Layout.fillWidth: true
          onEditingFinished: if (text !== controller.outputRoot)
            controller.outputRoot = text
          Accessible.name: qsTr("Report destination")
        }
        ActionButton {
          text: qsTr("Choose folder")
          enabled: !controller.busy
          onClicked: capture.outputRequested()
        }
        Label {
          text: qsTr("A new session folder is created for each analysis.")
          color: Theme.muted
          Layout.fillWidth: true
          wrapMode: Text.WordWrap
          font.pixelSize: 12
        }
        Rectangle {
          Layout.fillWidth: true
          implicitHeight: 1
          color: Theme.border
          Layout.topMargin: 8
          Layout.bottomMargin: 8
        }
        Label {
          text: qsTr("Reference labels")
          color: Theme.text
          font.weight: Font.Bold
        }
        Label {
          text: qsTr("Optional. Add labeled intervals to calculate precision and recall.")
          color: Theme.muted
          Layout.fillWidth: true
          wrapMode: Text.WordWrap
        }
        Label {
          visible: controller.groundTruth.length > 0
          text: controller.groundTruth
          textFormat: Text.PlainText
          color: Theme.accent
          wrapMode: Text.WrapAnywhere
          Layout.fillWidth: true
          font.family: Theme.codeFontFamily
          font.pixelSize: 12
        }
        GridLayout {
          columns: options.width < 440 ? 1 : 2
          Layout.fillWidth: true
          ActionButton {
            text: controller.groundTruth.length ? qsTr("Replace labels") : qsTr("Import labels")
            enabled: !controller.busy
            onClicked: capture.truthRequested()
          }
          ActionButton {
            text: qsTr("Remove labels")
            visible: controller.groundTruth.length > 0
            enabled: !controller.busy
            onClicked: controller.groundTruth = ""
          }
        }
      }
    }
    footer: DialogButtonBox {
      ActionButton {
        objectName: "captureOptionsDone"
        text: qsTr("Done")
        primary: true
        DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
      }
      onAccepted: options.close()
    }
  }
}
