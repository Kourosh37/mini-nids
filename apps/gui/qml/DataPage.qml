import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

ColumnLayout {
  id: page
  property var dataModel
  property int pageIndex: 2
  property string title
  signal inspectRequested(string details)
  spacing: 12
  RowLayout {
    Label { text: page.title; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
    Label { text: qsTr("Retained rows") + ": " + (page.dataModel ? page.dataModel.count : 0); color: Theme.muted }
  }
  TextField {
    id: search
    Layout.fillWidth: true; implicitHeight: Theme.controlHeight
    placeholderText: qsTr("Filter by protocol, endpoint, rule, severity or capture time…")
    Accessible.name: qsTr("Filter table")
    onTextChanged: if (page.dataModel) page.dataModel.filter = text
  }
  Label {
    text: qsTr("Rows are bounded by the configured history limit. Full flow, DNS and alert reports are saved to disk.")
    color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12
  }
  Rectangle {
    Layout.fillWidth: true; Layout.fillHeight: true
    color: Theme.surface; radius: Theme.radius; border.color: Theme.border
    ColumnLayout {
      anchors.fill: parent; anchors.margins: 1; spacing: 0
      HorizontalHeaderView {
        id: header
        visible: page.width >= Theme.compactBreakpoint
        Layout.fillWidth: true; syncView: table; clip: true
        delegate: Rectangle {
          required property int column
          required property var display
          implicitWidth: 160; implicitHeight: 44
          color: Theme.sidebar
          Label { anchors.fill: parent; anchors.margins: 12; text: display; color: Theme.muted; font.weight: Font.DemiBold; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
          TapHandler { onTapped: page.dataModel.sortColumn(column) }
          Accessible.role: Accessible.Button; Accessible.name: qsTr("Sort") + " " + display
        }
      }
      TableView {
        id: table
        objectName: "dataTable"
        visible: page.width >= Theme.compactBreakpoint
        Layout.fillWidth: true; Layout.fillHeight: true
        clip: true; model: page.dataModel; columnSpacing: 1; rowSpacing: 1
        columnWidthProvider: function(column) { return Math.max(140, (table.width - 6) / 6) }
        selectionBehavior: TableView.SelectRows
        selectionModel: ItemSelectionModel { model: page.dataModel }
        keyNavigationEnabled: true
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
        delegate: Rectangle {
          required property int row
          required property int column
          required property var display
          implicitWidth: 160; implicitHeight: 46
          color: row % 2 ? Theme.background : Theme.surface
          border.color: table.currentRow === row && table.activeFocus ? Theme.accent : "transparent"
          border.width: 1
          Label {
            anchors.fill: parent; anchors.margins: 12
            text: display; textFormat: Text.PlainText
            font.family: Theme.codeFontFamily
            font.pixelSize: 12; color: page.pageIndex === 4 && column === 1 ? Theme.warning : Theme.text
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
          }
          TapHandler { onTapped: page.inspectRequested(controller.inspect(page.pageIndex, row)) }
          Accessible.name: String(display); Accessible.role: Accessible.Cell
        }
        Keys.onReturnPressed: page.inspectRequested(controller.inspect(page.pageIndex, Math.max(0, currentRow)))
      }
      ListView {
        visible: page.width < Theme.compactBreakpoint
        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
        model: page.dataModel ? page.dataModel.count : 0
        delegate: ItemDelegate {
          required property int index
          width: ListView.view.width; height: 88
          contentItem: Label { text: page.dataModel.rowSummary(index); wrapMode: Text.Wrap; color: Theme.text; maximumLineCount: 3; textFormat: Text.PlainText }
          onClicked: page.inspectRequested(controller.inspect(page.pageIndex, index))
        }
      }
    }
    Label {
      anchors.centerIn: parent
      visible: !page.dataModel || page.dataModel.count === 0
      text: controller.busy ? qsTr("Analysis is running. Tables update in a batch when it finishes.") : qsTr("No matching rows. Open a capture or clear the filter.")
      color: Theme.muted; width: Math.min(parent.width - 40, 460); wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
    }
  }
}
