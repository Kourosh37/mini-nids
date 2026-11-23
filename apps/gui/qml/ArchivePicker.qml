import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    spacing: 10
    Label { text: qsTr("Captures in this ZIP"); font.pixelSize: 18; color: Theme.text }
    Label {
        text: qsTr("Header compatibility is checked first. Prepare checks CRC and every PCAP record, then enables analysis. Only the selected file is extracted.")
        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
    }
    Label {
        visible: controller.archiveEntries.length === 0
        text: controller.busy ? qsTr("Inspecting archive...") : qsTr("No captures available. See the status below.")
        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
    }
    ListView {
        id: captures
        Layout.fillWidth: true; Layout.preferredHeight: Math.min(320, contentHeight)
        clip: true; spacing: 8
        model: controller.archiveEntries
        currentIndex: controller.preparedEntry
        onCurrentIndexChanged: {
            if (currentIndex >= 0) Qt.callLater(function() { captures.positionViewAtIndex(captures.currentIndex, ListView.Contain) })
        }
        ScrollBar.vertical: ScrollBar { policy: captures.contentHeight > captures.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded }
        delegate: Rectangle {
            required property int index
            required property var modelData
            width: captures.width; height: contents.implicitHeight + 24
            color: Theme.surface; radius: 7
            border.color: controller.preparedEntry === index ? Theme.accent : Theme.border
            ColumnLayout {
                id: contents
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                anchors.margins: 12; spacing: 6
                Label {
                    text: modelData.name; textFormat: Text.PlainText; font.family: Theme.codeFontFamily
                    color: Theme.text; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true
                }
                Label {
                    text: (modelData.bytes / (1024 * 1024)).toFixed(2) + " MiB | " +
                        (modelData.supported ? (modelData.pcapng ? "PCAPNG / interfaces checked on prepare / converts to nanosecond PCAP" :
                            "PCAP / Ethernet / " + modelData.precision + " / snaplen " + modelData.snaplen) : modelData.reason)
                    textFormat: Text.PlainText; color: modelData.supported ? Theme.muted : Theme.warning
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Button {
                    text: controller.preparedEntry === index ? qsTr("Validated and ready") : qsTr("Prepare capture")
                    enabled: !controller.busy && modelData.supported && controller.preparedEntry !== index
                    implicitHeight: 44
                    Accessible.name: text + " " + modelData.name
                    onClicked: controller.prepareArchiveEntry(index)
                }
                Label {
                    visible: Boolean(modelData.warning)
                    text: modelData.warning || ""; textFormat: Text.PlainText
                    color: Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
            }
        }
    }
}
