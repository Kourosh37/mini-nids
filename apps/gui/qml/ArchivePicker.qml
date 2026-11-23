import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
  id: archive
  spacing: 10
  property string sourcePath: ""
  readonly property int selectedSourceRow: captures.currentIndex >= 0 && captures.currentIndex < files.count ? files.get(captures.currentIndex).sourceRow : -1
  readonly property string selectedName: selectedSourceRow >= 0 ? files.get(captures.currentIndex).entryName : ""
  readonly property bool selectedReady: selectedSourceRow >= 0 && selectedSourceRow === controller.preparedEntry
  readonly property bool canPrepare: !controller.busy && selectedSourceRow >= 0 && !selectedReady && files.get(captures.currentIndex).entrySupported
  function prepareSelected() {
    if (canPrepare)
      controller.prepareArchiveEntry(selectedSourceRow);
  }

  // Update rows in place: progress and warning updates must not reset scroll.
  function refreshFiles() {
    if (sourcePath !== controller.inputFile) {
      sourcePath = controller.inputFile;
      if (search.text.length) {
        search.clear();
        return;
      }
    }
    const entries = controller.archiveEntries;
    const query = search.text.trim().toLowerCase();
    const rows = [];
    for (let i = 0; i < entries.length; ++i) {
      const entry = entries[i];
      if (String(entry.name).toLowerCase().includes(query))
        rows.push({
          sourceRow: i,
          entryName: String(entry.name),
          entryBytes: Number(entry.bytes),
          entrySupported: Boolean(entry.supported),
          entryPcapng: Boolean(entry.pcapng),
          entryReason: String(entry.reason || ""),
          entryWarning: String(entry.warning || "")
        });
    }
    let sameRows = files.count === rows.length;
    for (let i = 0; sameRows && i < rows.length; ++i)
      sameRows = files.get(i).sourceRow === rows[i].sourceRow;
    if (!sameRows)
      files.clear();
    for (let i = 0; i < rows.length; ++i) {
      if (sameRows)
        files.set(i, rows[i]);
      else
        files.append(rows[i]);
    }
  }
  ListModel {
    id: files
  }
  Component.onCompleted: refreshFiles()
  Connections {
    target: controller
    function onArchiveEntriesChanged() {
      archive.refreshFiles();
    }
  }

  RowLayout {
    Layout.fillWidth: true
    Label {
      text: qsTr("Files in this ZIP")
      font.pixelSize: 17
      font.weight: Font.Bold
      color: Theme.text
      Layout.fillWidth: true
    }
    Label {
      text: files.count + " / " + controller.archiveEntries.length
      color: Theme.muted
      font.family: Theme.codeFontFamily
      font.pixelSize: 12
    }
  }
  RowLayout {
    Layout.fillWidth: true
    spacing: 8
    InputField {
      id: search
      objectName: "archiveSearch"
      Layout.fillWidth: true
      placeholderText: qsTr("Find a capture…")
      Accessible.name: qsTr("Search files in ZIP")
      onTextChanged: archive.refreshFiles()
    }
    ActionButton {
      text: qsTr("Clear")
      flat: true
      implicitWidth: 76
      enabled: search.text.length > 0
      onClicked: search.clear()
    }
  }
  Item {
    Layout.fillWidth: true
    Layout.fillHeight: true
    ListView {
      id: captures
      objectName: "archiveList"
      anchors.fill: parent
      clip: true
      spacing: 8
      model: files
      boundsBehavior: Flickable.StopAtBounds
      activeFocusOnTab: true
      keyNavigationEnabled: true
      ScrollBar.vertical: ScrollBar {}
      Keys.onReturnPressed: {
        if (archive.selectedReady && controller.canAnalyze)
          controller.start();
        else
          archive.prepareSelected();
      }
      delegate: Rectangle {
        required property int index
        required property int sourceRow
        required property string entryName
        required property double entryBytes
        required property bool entrySupported
        required property bool entryPcapng
        required property string entryReason
        required property string entryWarning
        readonly property bool ready: controller.preparedEntry === sourceRow
        width: captures.width
        height: rowContent.implicitHeight + 24
        radius: 9
        color: captures.currentIndex === index ? Theme.selected : rowHover.hovered ? Theme.hover : Theme.surface
        border.color: captures.currentIndex === index ? Theme.accent : Theme.border
        HoverHandler {
          id: rowHover
        }
        TapHandler {
          onTapped: {
            captures.currentIndex = index;
            captures.forceActiveFocus();
          }
        }
        Accessible.role: Accessible.ListItem
        Accessible.name: entryName + " " + (ready ? qsTr("Ready") : entryReason)
        Accessible.selected: captures.currentIndex === index
        ToolTip {
          visible: rowHover.hovered && fileName.truncated && !captures.moving
          delay: 700
          width: Math.min(360, archive.width)
          text: entryName
          contentItem: Label {
            text: entryName
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            color: Theme.text
            font.family: Theme.codeFontFamily
            font.pixelSize: 12
          }
        }
        RowLayout {
          id: rowContent
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.top: parent.top
          anchors.margins: 12
          spacing: 12
          ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
              id: fileName
              text: entryName
              textFormat: Text.PlainText
              font.family: Theme.codeFontFamily
              font.pixelSize: 12
              color: Theme.text
              Layout.fillWidth: true
              elide: Text.ElideMiddle
            }
            Label {
              text: (entryBytes / 1048576).toFixed(2) + " MiB · " + (entrySupported ? (entryPcapng ? "PCAPNG" : "PCAP") : entryReason)
              textFormat: Text.PlainText
              color: entrySupported ? Theme.muted : Theme.warning
              Layout.fillWidth: true
              wrapMode: Text.WordWrap
              font.pixelSize: 11
            }
            Label {
              visible: entryWarning.length > 0
              text: entryWarning
              textFormat: Text.PlainText
              color: Theme.warning
              Layout.fillWidth: true
              wrapMode: Text.WordWrap
              font.pixelSize: 11
            }
          }
          Label {
            text: ready ? qsTr("Ready") : !entrySupported ? qsTr("Unavailable") : ""
            color: ready ? Theme.accent : Theme.warning
            font.weight: Font.DemiBold
            font.pixelSize: 11
          }
        }
      }
    }
    Label {
      anchors.centerIn: parent
      width: Math.min(parent.width - 24, 360)
      visible: files.count === 0
      text: search.text.length ? qsTr("No matching captures. Try a different name.") : controller.busy ? qsTr("Reading the ZIP…") : qsTr("No captures found. Choose another file.")
      color: Theme.muted
      horizontalAlignment: Text.AlignHCenter
      wrapMode: Text.WordWrap
    }
  }
}
