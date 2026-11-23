import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

ColumnLayout {
  id: page
  property var dataModel
  property int pageIndex: 2
  property string title
  readonly property int loadedRows: dataModel ? dataModel.count : 0
  readonly property real matchingRows: dataModel ? Number(dataModel.matchCount) : 0
  property bool showSearchHelp: false
  readonly property real observedRows: dataModel ? Number(dataModel.totalCount) : 0
  signal inspectRequested(string details)
  spacing: 12
  RowLayout {
    Label { text: page.title; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true }
  }
  Label {
    text: controller.summary.capture_name ? qsTr("Results:") + " " + controller.summary.capture_name : qsTr("No capture has been analyzed yet")
    textFormat: Text.PlainText; color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideMiddle; font.pixelSize: 12
  }
  Label {
    objectName: "tableRecordCounts"
    text: page.dataModel && page.dataModel.queryBusy ? qsTr("Searching all records…")
          : qsTr("%1–%2 of %3 matches · %4 total records")
              .arg(page.loadedRows ? (page.dataModel.page * page.dataModel.pageSize + 1).toLocaleString() : "0")
              .arg(page.loadedRows ? (page.dataModel.page * page.dataModel.pageSize + page.loadedRows).toLocaleString() : "0")
              .arg(page.matchingRows.toLocaleString()).arg(page.observedRows.toLocaleString())
    color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 12
  }
  RowLayout {
    Layout.fillWidth: true; spacing: 8
    InputField {
      id: search
      Layout.fillWidth: true
      placeholderText: qsTr("Search all records… e.g. protocol:TCP bytes>=1000")
      objectName: "tableSearch"; Accessible.name: qsTr("Search all table records")
      onTextChanged: if (page.dataModel) page.dataModel.filter = text
    }
    ActionButton {
      text: qsTr("Clear"); flat: true; implicitWidth: 76; leftPadding: 12; rightPadding: 12
      enabled: search.text.length > 0
      onClicked: { search.clear(); search.forceActiveFocus() }
      Accessible.name: qsTr("Clear table filter")
    }
  }
  ActionButton {
    text: page.showSearchHelp ? qsTr("Hide search help") : qsTr("Advanced search")
    flat: true; implicitHeight: 30
    onClicked: page.showSearchHelp = !page.showSearchHelp
  }
  Label {
    visible: page.showSearchHelp
    text: page.dataModel ? page.dataModel.searchHelp : ""
    textFormat: Text.PlainText; color: Theme.muted; wrapMode: Text.WordWrap
    Layout.fillWidth: true; font.pixelSize: 12
  }
  Label {
    visible: page.dataModel && page.dataModel.queryError.length > 0
    text: page.dataModel ? page.dataModel.queryError : ""
    textFormat: Text.PlainText; color: Theme.warning; wrapMode: Text.WordWrap
    Layout.fillWidth: true; font.pixelSize: 12
  }
  Label {
    visible: page.pageIndex === 4
    text: qsTr("Alerts are rule-based indicators, not confirmed attacks. Open a row to inspect the triggering evidence.")
    color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12
  }
  Label {
    visible: controller.summary.status === "cancelled"
    text: qsTr("Analysis was cancelled. These results cover only the processed portion of the capture.")
    color: Theme.warning; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12
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
          objectName: "tableHeaderCell"
          required property int column
          required property var display
          implicitWidth: 160; implicitHeight: 44
          color: headerHover.hovered ? Theme.hover : Theme.sidebar
          HoverHandler { id: headerHover; cursorShape: Qt.PointingHandCursor }
          Label {
            anchors.fill: parent; anchors.margins: 12
            text: display + (page.dataModel && page.dataModel.sortedColumn === column ? (page.dataModel.sortDescending ? " ↓" : " ↑") : "")
            color: page.dataModel && page.dataModel.sortedColumn === column ? Theme.accent : Theme.muted
            font.weight: Font.DemiBold; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
          }
          TapHandler { onTapped: page.dataModel.sortColumn(column) }
          Accessible.role: Accessible.Button; Accessible.name: qsTr("Sort") + " " + display
        }
      }
      TableView {
        id: table
        objectName: "dataTable"
        visible: page.width >= Theme.compactBreakpoint
        Layout.fillWidth: true; Layout.fillHeight: true
        enabled: page.dataModel && !page.dataModel.queryBusy
        clip: true; model: page.dataModel; columnSpacing: 1; rowSpacing: 1
        columnWidthProvider: function(column) { return Math.max(140, (table.width - 6) / 6) }
        selectionBehavior: TableView.SelectRows
        selectionModel: ItemSelectionModel { model: page.dataModel }
        keyNavigationEnabled: true
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
        delegate: Rectangle {
          objectName: "dataCell"
          required property int row
          required property int column
          required property var display
          implicitWidth: 160; implicitHeight: 46
          color: table.currentRow === row ? Theme.selected : cellHover.hovered ? Theme.hover : row % 2 ? Theme.background : Theme.surface
          border.color: table.currentRow === row && table.activeFocus ? Theme.accent : "transparent"
          border.width: 1
          HoverHandler { id: cellHover; cursorShape: Qt.PointingHandCursor }
          Label {
            anchors.fill: parent; anchors.margins: 12
            text: display; textFormat: Text.PlainText
            font.family: Theme.codeFontFamily
            font.pixelSize: 12
            color: page.pageIndex === 4 && column === 2 ? (display === "high" ? Theme.danger : display === "low" ? Theme.muted : Theme.warning) : Theme.text
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
          }
          TapHandler {
            onTapped: {
              table.selectionModel.setCurrentIndex(table.model.index(row, column), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows)
              table.forceActiveFocus()
              page.inspectRequested(controller.inspect(page.pageIndex, row))
            }
          }
          Accessible.name: String(display); Accessible.role: Accessible.Cell
        }
        Keys.onReturnPressed: page.inspectRequested(controller.inspect(page.pageIndex, Math.max(0, currentRow)))
      }
      ListView {
        visible: page.width < Theme.compactBreakpoint
        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
        enabled: page.dataModel && !page.dataModel.queryBusy
        model: page.dataModel
        delegate: ItemDelegate {
          required property int index
          width: ListView.view.width; height: 88
          hoverEnabled: true
          background: Rectangle { color: parent.down ? Theme.selected : parent.hovered ? Theme.hover : Theme.surface; border.color: parent.visualFocus ? Theme.accent : Theme.border; radius: 8 }
          contentItem: Label { objectName: "compactTableRow"; text: page.dataModel.rowSummary(index); wrapMode: Text.Wrap; color: Theme.text; maximumLineCount: 3; textFormat: Text.PlainText }
          onClicked: page.inspectRequested(controller.inspect(page.pageIndex, index))
        }
      }
    }
    Label {
      anchors.centerIn: parent
      visible: !page.dataModel || page.dataModel.count === 0
      text: page.dataModel && page.dataModel.queryBusy ? qsTr("Searching…")
            : page.dataModel && page.dataModel.queryError.length ? qsTr("Check the search expression above.")
            : page.observedRows > 0 ? qsTr("No matching records. Clear or change the search.")
            : controller.busy ? qsTr("Waiting for records…") : qsTr("No records available. Analyze a capture to populate this table.")
      color: Theme.muted; width: Math.min(parent.width - 40, 460); wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
    }
  }
  RowLayout {
    Layout.fillWidth: true; spacing: 8
    enabled: page.dataModel && !page.dataModel.queryBusy
    ActionButton {
      visible: page.width >= 680; text: qsTr("First"); implicitWidth: 64; leftPadding: 10; rightPadding: 10
      enabled: page.dataModel && page.dataModel.page > 0
      onClicked: page.dataModel.page = 0
    }
    ActionButton {
      objectName: "previousPage"; text: qsTr("Previous"); implicitWidth: 82; leftPadding: 10; rightPadding: 10
      enabled: page.dataModel && page.dataModel.page > 0
      onClicked: page.dataModel.page--
    }
    SpinBox {
      objectName: "pageNumber"; from: 1; to: page.dataModel ? page.dataModel.pageCount : 1
      value: page.dataModel ? page.dataModel.page + 1 : 1
      editable: true; implicitWidth: 110; Accessible.name: qsTr("Go to page")
      onValueModified: page.dataModel.page = value - 1
    }
    Label { text: qsTr("of %1").arg(page.dataModel ? page.dataModel.pageCount.toLocaleString() : "1"); color: Theme.muted }
    ActionButton {
      objectName: "nextPage"; text: qsTr("Next"); implicitWidth: 64; leftPadding: 10; rightPadding: 10
      enabled: page.dataModel && page.dataModel.page + 1 < page.dataModel.pageCount
      onClicked: page.dataModel.page++
    }
    ActionButton {
      visible: page.width >= 680; text: qsTr("Last"); implicitWidth: 64; leftPadding: 10; rightPadding: 10
      enabled: page.dataModel && page.dataModel.page + 1 < page.dataModel.pageCount
      onClicked: page.dataModel.page = page.dataModel.pageCount - 1
    }
    Item { Layout.fillWidth: true }
    ComboBox {
      visible: page.width >= 600; model: [100, 250, 500]; implicitWidth: 100
      currentIndex: page.dataModel ? [100, 250, 500].indexOf(page.dataModel.pageSize) : 0
      Accessible.name: qsTr("Rows per page")
      onActivated: page.dataModel.pageSize = Number(currentText)
    }
  }

}
