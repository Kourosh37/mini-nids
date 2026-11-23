import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
  id: dashboard
  clip: true
  contentWidth: availableWidth
  ColumnLayout {
    width: dashboard.availableWidth
    spacing: 20
    RowLayout {
      ColumnLayout {
        Layout.fillWidth: true
        Label { text: qsTr("Traffic overview"); font.pixelSize: 25; font.weight: Font.DemiBold; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
        Label { text: qsTr("Capture evidence, stateful analysis, local reports."); font.pixelSize: 13; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
      }
      Label { text: controller.summary.status || qsTr("No session"); color: Theme.accent; font.pixelSize: 13 }
    }
    GridLayout {
      Layout.fillWidth: true
      columns: dashboard.width < 760 ? 2 : 4; columnSpacing: 12; rowSpacing: 12
      StatCard { Layout.fillWidth: true; title: qsTr("Packets analyzed"); value: Number(controller.summary.packets || 0).toLocaleString(); caption: qsTr("Sequential packet processing") }
      StatCard { Layout.fillWidth: true; title: qsTr("Capture bytes"); value: Number(controller.summary.captured_bytes || 0).toLocaleString(); caption: qsTr("Captured length, before decoding") }
      StatCard { Layout.fillWidth: true; title: qsTr("Retained flows"); value: Number(controller.summary.active_flows || 0).toLocaleString(); caption: qsTr("Bounded by configured limits") }
      StatCard { Layout.fillWidth: true; title: qsTr("Alert groups"); value: String(controller.summary.alerts || 0); caption: qsTr("Evidence requires analyst review"); valueColor: Theme.warning }
    }
    Rectangle {
      Layout.fillWidth: true; implicitHeight: 270; color: Theme.surface; border.color: Theme.border; radius: Theme.radius
      ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        RowLayout {
          Label { text: qsTr("Traffic timeline"); color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true }
          Label { text: qsTr("Packets / capture-time bucket"); color: Theme.muted; font.pixelSize: 11 }
        }
        Canvas {
          id: chart
          Layout.fillWidth: true; Layout.fillHeight: true
          property var points: controller.timeline
          onPointsChanged: requestPaint()
          onWidthChanged: requestPaint()
          Connections { target: controller; function onChanged() { chart.requestPaint() } }
          onPaint: {
            let ctx = getContext("2d"); ctx.reset()
            let w = width, h = height - 26
            ctx.strokeStyle = Theme.border; ctx.lineWidth = 1
            for (let i=0;i<4;i++) {let y=i*h/3;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}
            if (!points.length) {ctx.fillStyle=Theme.muted;ctx.font="13px '"+Theme.uiFontFamily+"'";ctx.fillText(qsTr("Open a PCAP to view observed traffic."),12,h/2);return}
            let max = 1; for (let p of points) max=Math.max(max,Number(p.count))
            let bucket = Number(controller.summary.timeline_bucket_width_ns || 1000000000) / 1000000000
            let span = Math.max(bucket, Number(points[points.length-1].time)-Number(points[0].time)+bucket)
            let step=w*bucket/span
            ctx.fillStyle=Theme.accent
            for (let j=0;j<points.length;j++) {let bh=Number(points[j].count)/max*(h-12);let x=(Number(points[j].time)-Number(points[0].time))/span*w;ctx.fillRect(x+2,h-bh,Math.max(1,step-4),bh)}
            ctx.fillStyle=Theme.muted;ctx.font="11px '"+Theme.codeFontFamily+"'"
            ctx.fillText(String(points[0].time)+" s",0,height-5)
            ctx.fillText(String(points[points.length-1].time)+" s",Math.max(0,w-90),height-5)
          }
          Accessible.name: qsTr("Traffic timeline"); Accessible.description: JSON.stringify(controller.timeline)
        }
      }
    }
    GridLayout {
      columns: dashboard.width < 760 ? 1 : 2
      columnSpacing: 16; rowSpacing: 16; Layout.fillWidth: true
      Rectangle {
        Layout.fillWidth: true; implicitHeight: 230; color: Theme.surface; radius: Theme.radius; border.color: Theme.border
        ColumnLayout {
          anchors.fill: parent; anchors.margins: 20
          Label { text: qsTr("Protocol distribution"); color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
          Repeater {
            model: controller.protocols
            RowLayout {
              required property var modelData
              Label { text: modelData.name; color: Theme.muted; Layout.preferredWidth: 65 }
              ProgressBar { value: Number(modelData.count) / Math.max(1,Number(controller.summary.packets)); Layout.fillWidth: true }
              Label { text: modelData.count; color: Theme.text }
            }
          }
          Label { visible: !controller.protocols.length; text: qsTr("No capture has been analyzed."); color: Theme.muted }
          Item { Layout.fillHeight: true }
        }
      }
      Rectangle {
        Layout.fillWidth: true; implicitHeight: 230; color: Theme.surface; radius: Theme.radius; border.color: Theme.border
        ColumnLayout {
          anchors.fill: parent; anchors.margins: 20; spacing: 10
          Label { text: qsTr("Analysis health"); color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
          Label { text: qsTr("Duration") + ": " + Number(controller.summary.duration_s || 0).toFixed(3) + " s"; color: Theme.muted }
          Label { text: qsTr("Throughput") + ": " + Math.round(controller.summary.throughput || 0).toLocaleString() + " pkt/s"; color: Theme.muted }
          Label { text: qsTr("Malformed / truncated") + ": " + (controller.summary.malformed || 0) + " / " + (controller.summary.truncated || 0); color: Theme.muted }
          Label { text: qsTr("State drops / time regressions") + ": " + (controller.summary.state_drops || 0) + " / " + (controller.summary.clock_regressions || 0); color: Theme.muted }
          Item { Layout.fillHeight: true }
        }
      }
    }
    Label { text: qsTr("Recent alert evidence"); color: Theme.text; font.pixelSize: 16; font.weight: Font.DemiBold }
    Repeater {
      model: Math.min(alertModel.count, 4)
      Label { required property int index; text: alertModel.rowSummary(index); textFormat: Text.PlainText; color: Theme.warning; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
    Label { visible: !alertModel.count; text: qsTr("No alert groups in the current session."); color: Theme.muted }
    Item { implicitHeight: 8 }
  }
}
