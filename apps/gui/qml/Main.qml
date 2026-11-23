import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
  id: window
  objectName: "mainWindow"
  width: 1320; height: 860; minimumWidth: 360; minimumHeight: 600
  visible: false; title: "Mini-NIDS · " + qsTr("Network analysis")
  color: Theme.background
  font.family: Theme.uiFontFamily; font.pixelSize: 14
  property int page: 0
  property var navigation: [qsTr("Dashboard"), qsTr("Capture"), qsTr("Packets"), qsTr("Flows"), qsTr("Alerts"), qsTr("DNS analysis"), qsTr("Evaluation"), qsTr("Reports"), qsTr("Settings")]
  palette.window: Theme.background; palette.windowText: Theme.text
  palette.base: Theme.surface; palette.alternateBase: Theme.background
  palette.text: Theme.text; palette.button: Theme.surface; palette.buttonText: Theme.text
  palette.highlight: Theme.accent; palette.highlightedText: Theme.background
  palette.placeholderText: Theme.muted; palette.mid: Theme.border
  palette.toolTipBase: Theme.surface; palette.toolTipText: Theme.text

  FileDialog { id: captureDialog; title: qsTr("Open capture or dataset"); nameFilters: ["Captures and datasets (*.pcap *.zip)", "PCAP files (*.pcap)", "ZIP datasets (*.zip)", "All files (*)"]; onAccepted: { controller.inputFile = selectedFile.toString(); window.page = 1 } }
  FileDialog { id: truthDialog; title: qsTr("Import ground truth"); nameFilters: ["JSON files (*.json)"]; onAccepted: controller.groundTruth = selectedFile.toString() }
  FolderDialog { id: outputDialog; title: qsTr("Choose report folder"); onAccepted: controller.outputRoot = selectedFolder.toString() }
  Shortcut { sequence: StandardKey.Open; enabled: !controller.busy; onActivated: captureDialog.open() }
  Dialog {
    id: inspector
    objectName: "evidenceDialog"
    title: qsTr("Evidence and packet details")
    modal: true; anchors.centerIn: parent
    width: Math.min(window.width - 24, 880); height: Math.min(window.height - 40, 720)
    standardButtons: Dialog.Close
    property string details
    ScrollView {
      anchors.fill: parent; clip: true
      TextArea { text: inspector.details; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; font.family: Theme.codeFontFamily; font.pixelSize: 12; textFormat: TextEdit.PlainText; Accessible.name: qsTr("Evidence details") }
    }
  }
  Drawer {
    id: compactNav
    width: Math.min(300, window.width - 48); height: window.height
    edge: Qt.LeftEdge
    ColumnLayout {
      anchors.fill: parent; anchors.margins: 16
      Label { text: "MINI / NIDS"; font.pixelSize: 20; color: Theme.accent; font.weight: Font.Bold }
      Repeater {
        model: window.navigation
        ItemDelegate { required property int index; required property string modelData; text: modelData; Layout.fillWidth: true; implicitHeight: 48; highlighted: window.page === index; onClicked: { window.page = index; compactNav.close() } }
      }
      Item { Layout.fillHeight: true }
    }
  }
  RowLayout {
    anchors.fill: parent; spacing: 0
    Rectangle {
      visible: window.width >= 1000
      Layout.preferredWidth: 218; Layout.fillHeight: true
      color: Theme.sidebar
      ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 5
        Label { text: "MINI / NIDS"; color: Theme.accent; font.pixelSize: 21; font.weight: Font.Bold; Layout.topMargin: 16; Layout.bottomMargin: 4 }
        Label { text: qsTr("NETWORK OBSERVATORY"); color: Theme.muted; font.pixelSize: 9; font.letterSpacing: 1.4; Layout.bottomMargin: 26 }
        Repeater {
          model: window.navigation
          ItemDelegate {
            required property int index
            required property string modelData
            Layout.fillWidth: true; implicitHeight: 46
            highlighted: window.page === index
            hoverEnabled: true
            scale: down ? 0.98 : 1
            Behavior on scale { NumberAnimation { duration: 100 } }
            background: Rectangle {
              color: window.page === index ? Theme.selected : parent.hovered ? Theme.hover : "transparent"
              radius: 7; border.color: parent.visualFocus ? Theme.accent : "transparent"
              Behavior on color { ColorAnimation { duration: 120 } }
            }
            contentItem: Label { text: (index + 1).toString().padStart(2, "0") + "   " + modelData; color: window.page === index ? Theme.accent : Theme.muted; verticalAlignment: Text.AlignVCenter; font.pixelSize: 13 }
            Accessible.name: modelData; onClicked: window.page = index
          }
        }
        Item { Layout.fillHeight: true }
      }
    }
    ColumnLayout {
      Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
      Rectangle {
        color: Theme.surface; Layout.fillWidth: true; implicitHeight: 74
        RowLayout {
          anchors.fill: parent; anchors.margins: 16; spacing: 12
          ActionButton { visible: window.width < 1000; text: qsTr("Menu"); flat: true; implicitWidth: 72; leftPadding: 12; rightPadding: 12; Accessible.name: qsTr("Navigation"); onClicked: compactNav.open() }
          Label { text: window.navigation[window.page]; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
          BusyIndicator { running: controller.busy; visible: running; implicitWidth: 32; implicitHeight: 32; palette.dark: Theme.accent }
          ActionButton { text: qsTr("Open file"); enabled: !controller.busy; onClicked: captureDialog.open() }
        }
      }
      ProgressBar {
        id: globalProgress
        Layout.fillWidth: true; value: controller.progress; visible: controller.busy; implicitHeight: 3; padding: 0
        background: Rectangle { color: Theme.progressTrack }
        contentItem: Item { Rectangle { width: parent.width * globalProgress.visualPosition; height: parent.height; color: Theme.progressFill } }
      }
      StackLayout {
        id: pages
        currentIndex: window.page
        Layout.fillWidth: true; Layout.fillHeight: true
        Layout.margins: window.width < 600 ? 12 : 26
        Dashboard {}
        CapturePage {
          onBrowseRequested: captureDialog.open()
          onOutputRequested: outputDialog.open()
          onTruthRequested: truthDialog.open()
        }
        DataPage { title: qsTr("Packet explorer"); dataModel: packetModel; pageIndex: 2; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("Flow explorer"); dataModel: flowModel; pageIndex: 3; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("Threat evidence"); dataModel: alertModel; pageIndex: 4; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("DNS messages"); dataModel: dnsModel; pageIndex: 5; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        ColumnLayout {
          spacing: 16
          Label { text: qsTr("Ground-truth evaluation"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          Label { text: qsTr("Import independent attack intervals before analysis. Synthetic fixtures verify engineering behavior only."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          ActionButton { text: qsTr("Import reference labels"); primary: true; enabled: !controller.busy; onClicked: truthDialog.open() }
          GridLayout {
            columns: window.width < 850 ? 2 : 4; Layout.fillWidth: true
            StatCard { Layout.fillWidth: true; title: "TP / FP / FN"; value: controller.evaluation.status === "evaluated" ? controller.evaluation.tp + " / " + controller.evaluation.fp + " / " + controller.evaluation.fn : "—"; caption: qsTr("Annotated attack intervals") }
            StatCard { Layout.fillWidth: true; title: qsTr("Precision"); value: controller.evaluation.precision === undefined || controller.evaluation.precision === null ? "—" : (controller.evaluation.precision * 100).toFixed(1) + "%"; caption: "TP / (TP + FP)" }
            StatCard { Layout.fillWidth: true; title: qsTr("Recall"); value: controller.evaluation.recall === undefined || controller.evaluation.recall === null ? "—" : (controller.evaluation.recall * 100).toFixed(1) + "%"; caption: "TP / (TP + FN)" }
          }
          ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { text: JSON.stringify(controller.evaluation, null, 2); textFormat: TextEdit.PlainText; readOnly: true; font.family: Theme.codeFontFamily; wrapMode: TextEdit.Wrap; Accessible.name: qsTr("Evaluation details") } }
        }
        ColumnLayout {
          spacing: 18
          Label { text: qsTr("Session reports"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          Label { text: qsTr("Each analysis writes a new session folder with CSV evidence, JSON summaries, rule configuration and evaluation results."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          Label { text: controller.reportPath || qsTr("Analyze a capture to create reports."); color: Theme.accent; wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText }
          Label {
            visible: !controller.busy && controller.reportPath.length > 0 && controller.summary.status !== "complete"
            text: qsTr("This attempt did not complete. Reports may be partial; check session_status.json before using them.")
            color: Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true
          }
          ActionButton { text: qsTr("Open report folder"); primary: true; enabled: controller.reportPath.length > 0 && !controller.busy; onClicked: controller.openReports() }
          Label { text: "alerts.csv\nalerts_extended.csv\nflows.csv\ndns_log.csv\nprotocol_statistics.csv\ncapture_summary.json\ndetection_metrics.json\nperformance.json\nrules_snapshot.json\nsession_status.json"; color: Theme.muted; font.family: Theme.codeFontFamily; lineHeight: 1.6 }
          Item { Layout.fillHeight: true }
        }
        ColumnLayout {
          spacing: 14
          Label { text: qsTr("Settings"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          Label { text: qsTr("Detection thresholds, state timeouts, memory limits and export redaction. Values apply to the next analysis."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            TextArea { id: configEditor; text: controller.configText; textFormat: TextEdit.PlainText; font.family: Theme.codeFontFamily; selectByMouse: true; wrapMode: TextEdit.Wrap; enabled: !controller.busy; Accessible.name: qsTr("Engine configuration JSON") }
          }
          ActionButton { text: qsTr("Apply configuration"); primary: true; enabled: !controller.busy; onClicked: controller.applyConfig(configEditor.text) }
        }
      }
      Rectangle {
        Layout.fillWidth: true; implicitHeight: 38; color: Theme.sidebar
        Label { anchors.fill: parent; anchors.margins: 10; text: controller.status; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText; Accessible.role: Accessible.StaticText }
      }
    }
  }
}
