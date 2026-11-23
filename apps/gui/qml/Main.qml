import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
  id: window
  objectName: "mainWindow"
  width: 1320; height: 860; minimumWidth: 360; minimumHeight: 600
  visible: true; title: "Mini-NIDS · " + qsTr("Network analysis")
  color: Theme.background
  font.family: "Segoe UI"; font.pixelSize: 14
  property int page: 0
  property var navigation: [qsTr("Dashboard"), qsTr("Capture"), qsTr("Packets"), qsTr("Flows"), qsTr("Alerts"), qsTr("DNS analysis"), qsTr("Evaluation"), qsTr("Reports"), qsTr("Settings")]
  palette.window: Theme.background; palette.windowText: Theme.text
  palette.base: Theme.surface; palette.alternateBase: Theme.background
  palette.text: Theme.text; palette.button: Theme.surface; palette.buttonText: Theme.text
  palette.highlight: Theme.accent; palette.highlightedText: Theme.background
  palette.placeholderText: Theme.muted; palette.mid: Theme.border
  LayoutMirroring.enabled: controller.persian
  LayoutMirroring.childrenInherit: true

  FileDialog { id: captureDialog; title: qsTr("Open capture"); nameFilters: ["PCAP files (*.pcap)", "All files (*)"]; onAccepted: { controller.inputFile = selectedFile.toString(); window.page = 1 } }
  FileDialog { id: truthDialog; title: qsTr("Import ground truth"); nameFilters: ["JSON files (*.json)"]; onAccepted: controller.groundTruth = selectedFile.toString() }
  FolderDialog { id: outputDialog; title: qsTr("Choose report folder"); onAccepted: controller.outputRoot = selectedFolder.toString() }
  Dialog {
    id: inspector
    title: qsTr("Evidence and packet details")
    modal: true; anchors.centerIn: parent
    width: Math.min(window.width - 24, 880); height: Math.min(window.height - 40, 720)
    standardButtons: Dialog.Close
    property string details
    ScrollView {
      anchors.fill: parent; clip: true
      TextArea { text: inspector.details; readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap; font.family: "Consolas"; font.pixelSize: 12; textFormat: TextEdit.PlainText; LayoutMirroring.enabled: false; Accessible.name: qsTr("Evidence details") }
    }
  }
  Drawer {
    id: compactNav
    width: Math.min(300, window.width - 48); height: window.height
    edge: controller.persian ? Qt.RightEdge : Qt.LeftEdge
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
            background: Rectangle { color: window.page === index ? Theme.selected : "transparent"; radius: 7; border.color: parent.activeFocus ? Theme.accent : "transparent" }
            contentItem: Label { text: (index + 1).toString().padStart(2, "0") + "   " + modelData; color: window.page === index ? Theme.accent : Theme.muted; verticalAlignment: Text.AlignVCenter; font.pixelSize: 13 }
            Accessible.name: modelData; onClicked: window.page = index
          }
        }
        Item { Layout.fillHeight: true }
        Rectangle { color: Theme.border; Layout.fillWidth: true; height: 1 }
        Label { text: qsTr("LOCAL · OFFLINE"); color: Theme.accent; font.pixelSize: 10; Layout.topMargin: 12 }
        Label { text: "C++20 / Qt 6.8.3"; color: Theme.muted; font.pixelSize: 10; Layout.bottomMargin: 12 }
      }
    }
    ColumnLayout {
      Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
      Rectangle {
        color: Theme.surface; Layout.fillWidth: true; implicitHeight: 74
        RowLayout {
          anchors.fill: parent; anchors.margins: 16; spacing: 12
          ToolButton { visible: window.width < 1000; text: "☰"; Accessible.name: qsTr("Navigation"); onClicked: compactNav.open() }
          Label { text: window.navigation[window.page]; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
          BusyIndicator { running: controller.busy; visible: running; implicitWidth: 32; implicitHeight: 32 }
          Button { text: controller.dark ? qsTr("Light") : qsTr("Dark"); onClicked: controller.dark = !controller.dark; implicitHeight: 44; Accessible.name: qsTr("Switch theme") }
          Button { text: qsTr("Open PCAP"); enabled: !controller.busy; onClicked: captureDialog.open(); implicitHeight: 44 }
        }
      }
      ProgressBar { Layout.fillWidth: true; value: controller.progress; visible: controller.busy; implicitHeight: 3 }
      StackLayout {
        id: pages
        currentIndex: window.page
        Layout.fillWidth: true; Layout.fillHeight: true
        Layout.margins: window.width < 600 ? 12 : 26
        Dashboard {}
        ScrollView {
          id: capturePage; clip: true; contentWidth: availableWidth
          ColumnLayout {
            width: capturePage.availableWidth; spacing: 20
            Label { text: qsTr("Analyze a capture"); font.pixelSize: 25; font.weight: Font.DemiBold; color: Theme.text }
            Label { text: qsTr("Open a classic Ethernet PCAP. Analysis uses capture timestamps and stays on this device."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { text: qsTr("Input capture"); color: Theme.text }
            TextField { text: controller.inputFile; onEditingFinished: controller.inputFile = text; Layout.fillWidth: true; implicitHeight: 48; placeholderText: "capture.pcap"; enabled: !controller.busy; selectByMouse: true; Accessible.name: qsTr("Input capture") }
            RowLayout {
              Button { text: qsTr("Browse"); enabled: !controller.busy; onClicked: captureDialog.open(); implicitHeight: 44 }
              Button { objectName: "analyzeButton"; text: qsTr("Start analysis"); enabled: !controller.busy && controller.inputFile.length > 0; onClicked: controller.start(); implicitHeight: 44 }
              Button { text: qsTr("Cancel"); enabled: controller.busy; onClicked: controller.cancel(); implicitHeight: 44 }
            }
            Label { text: qsTr("Report destination"); color: Theme.text }
            TextField { text: controller.outputRoot; onEditingFinished: controller.outputRoot = text; enabled: !controller.busy; Layout.fillWidth: true; implicitHeight: 48; Accessible.name: qsTr("Report destination") }
            Button { text: qsTr("Choose folder"); enabled: !controller.busy; onClicked: outputDialog.open(); implicitHeight: 44 }
            Label { text: qsTr("Ground truth (optional)"); color: Theme.text }
            Label { text: controller.groundTruth || qsTr("No ground truth imported; accuracy metrics will remain unavailable."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
              Button { text: qsTr("Import ground truth"); enabled: !controller.busy; onClicked: truthDialog.open(); implicitHeight: 44 }
              Button { text: qsTr("Clear"); enabled: !controller.busy; onClicked: controller.groundTruth = ""; implicitHeight: 44 }
            }
            Label { text: qsTr("Recent captures"); font.pixelSize: 17; color: Theme.text }
            Repeater { model: controller.recentFiles; ItemDelegate { required property string modelData; text: modelData; Layout.fillWidth: true; implicitHeight: 48; enabled: !controller.busy; onClicked: controller.inputFile = modelData } }
            Label { text: qsTr("Live capture is not implemented. Offline analysis requires no administrator privileges."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          }
        }
        DataPage { title: qsTr("Packet explorer"); dataModel: packetModel; pageIndex: 2; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("Flow explorer"); dataModel: flowModel; pageIndex: 3; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("Threat evidence"); dataModel: alertModel; pageIndex: 4; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        DataPage { title: qsTr("DNS transactions"); dataModel: dnsModel; pageIndex: 5; onInspectRequested: function(details) { inspector.details = details; inspector.open() } }
        ColumnLayout {
          spacing: 16
          Label { text: qsTr("Ground-truth evaluation"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          Label { text: qsTr("Import independent attack intervals before analysis. Synthetic fixtures verify engineering behavior only."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          Button { text: qsTr("Import ground truth"); enabled: !controller.busy; onClicked: truthDialog.open(); implicitHeight: 44 }
          GridLayout {
            columns: window.width < 850 ? 2 : 4; Layout.fillWidth: true
            StatCard { Layout.fillWidth: true; title: "TP / FP / FN"; value: controller.evaluation.status === "evaluated" ? controller.evaluation.tp + " / " + controller.evaluation.fp + " / " + controller.evaluation.fn : "—"; caption: qsTr("Annotated attack intervals") }
            StatCard { Layout.fillWidth: true; title: qsTr("Precision"); value: controller.evaluation.precision === undefined || controller.evaluation.precision === null ? "—" : (controller.evaluation.precision * 100).toFixed(1) + "%"; caption: "TP / (TP + FP)" }
            StatCard { Layout.fillWidth: true; title: qsTr("Recall"); value: controller.evaluation.recall === undefined || controller.evaluation.recall === null ? "—" : (controller.evaluation.recall * 100).toFixed(1) + "%"; caption: "TP / (TP + FN)" }
          }
          ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { text: JSON.stringify(controller.evaluation, null, 2); textFormat: TextEdit.PlainText; readOnly: true; font.family: "Consolas"; wrapMode: TextEdit.Wrap; Accessible.name: qsTr("Evaluation details"); LayoutMirroring.enabled: false } }
        }
        ColumnLayout {
          spacing: 18
          Label { text: qsTr("Session reports"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          Label { text: qsTr("Each analysis writes a new session folder with CSV evidence, JSON summaries, rule configuration and evaluation results."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          Label { text: controller.reportPath || qsTr("Analyze a capture to create reports."); color: Theme.accent; wrapMode: Text.Wrap; Layout.fillWidth: true; textFormat: Text.PlainText }
          Button { text: qsTr("Open report folder"); enabled: controller.reportPath.length > 0 && !controller.busy; onClicked: controller.openReports(); implicitHeight: 44 }
          Label { text: "alerts.csv\nalerts_extended.csv\nflows.csv\ndns_log.csv\nprotocol_statistics.csv\ncapture_summary.json\ndetection_metrics.json\nperformance.json\nrules_snapshot.json\nsession_status.json"; color: Theme.muted; font.family: "Consolas"; lineHeight: 1.6 }
          Item { Layout.fillHeight: true }
        }
        ColumnLayout {
          spacing: 14
          Label { text: qsTr("Settings"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
          RowLayout {
            Switch { text: qsTr("Dark theme"); checked: controller.dark; onToggled: controller.dark = checked; implicitHeight: 44 }
            Switch { text: "فارسی / Persian"; checked: controller.persian; onToggled: controller.persian = checked; implicitHeight: 44 }
          }
          Label { text: qsTr("Detection thresholds, state timeouts, memory limits and export redaction. Values apply to the next analysis."); color: Theme.muted; wrapMode: Text.Wrap; Layout.fillWidth: true }
          ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            TextArea { id: configEditor; text: controller.configText; textFormat: TextEdit.PlainText; font.family: "Consolas"; selectByMouse: true; wrapMode: TextEdit.Wrap; enabled: !controller.busy; Accessible.name: qsTr("Engine configuration JSON"); LayoutMirroring.enabled: false }
          }
          Button { text: qsTr("Apply configuration"); enabled: !controller.busy; onClicked: controller.applyConfig(configEditor.text); implicitHeight: 44 }
        }
      }
      Rectangle {
        Layout.fillWidth: true; implicitHeight: 38; color: Theme.sidebar
        Label { anchors.fill: parent; anchors.margins: 10; text: controller.status; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText; Accessible.role: Accessible.StaticText }
      }
    }
  }
}
