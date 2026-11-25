import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
  id: page
  spacing: 14
  property var values: controller.analysisSettings
  property string validationError: ""
  readonly property var thresholds: [
    {key: "scan_ports", label: qsTr("Distinct ports to detect a scan"), minimum: 2, maximum: 65535},
    {key: "syn_count", label: qsTr("SYN attempts to detect a flood"), minimum: 2, maximum: 4294967295},
    {key: "half_open", label: qsTr("Incomplete SYN attempts"), minimum: 2, maximum: 4294967295},
    {key: "arp_conflicts", label: qsTr("Unsolicited ARP conflicts"), minimum: 1, maximum: 4294967295},
    {key: "slowloris_connections", label: qsTr("Concurrent slow HTTP requests"), minimum: 2, maximum: 1000000},
    {key: "slowloris_max_header_rate", label: qsTr("Slow HTTP header rate, bytes/s"), minimum: 1, maximum: 65536},
    {key: "slowloris_min_progress", label: qsTr("Distinct header progress events"), minimum: 1, maximum: 1000}
  ]
  readonly property var intervals: [
    {key: "window_s", label: qsTr("Scan and SYN observation window")},
    {key: "dedup_window_s", label: qsTr("Alert grouping window")},
    {key: "dns_timeout_s", label: qsTr("DNS query context")},
    {key: "arp_timeout_s", label: qsTr("ARP mapping lifetime")},
    {key: "idle_timeout_s", label: qsTr("Flow idle timeout")},
    {key: "active_timeout_s", label: qsTr("Flow maximum lifetime")},
    {key: "slowloris_min_age_s", label: qsTr("Minimum incomplete HTTP header age")},
    {key: "http_idle_timeout_s", label: qsTr("HTTP stream idle timeout")},
    {key: "http_active_timeout_s", label: qsTr("HTTP stream maximum lifetime")}
  ]
  readonly property var resourceLimits: [
    {key: "max_flows", label: qsTr("Tracked flow entries")},
    {key: "max_dns", label: qsTr("Pending DNS contexts")},
    {key: "max_arp", label: qsTr("ARP mapping and solicitation entries")},
    {key: "max_window_entries", label: qsTr("Scan / SYN history entries")},
    {key: "max_alerts", label: qsTr("Pending alert groups")},
    {key: "max_packet", label: qsTr("Bytes per captured packet")},
    {key: "max_report_bytes", label: qsTr("Total report bytes / statistics disk budget")},
    {key: "max_http_streams", label: qsTr("Tracked HTTP streams")},
    {key: "max_http_header_bytes", label: qsTr("Bytes per HTTP header")},
    {key: "max_http_memory_bytes", label: qsTr("HTTP byte storage budget")}
  ]
  function apply() {
    let settings = {};
    for (let i = 0; i < thresholdFields.count; ++i) {
      const field = thresholdFields.itemAt(i);
      const value = Number(field.input.text);
      if (!Number.isInteger(value) || value < thresholds[i].minimum || value > thresholds[i].maximum) {
        validationError = qsTr("Check the value for") + " " + thresholds[i].label;
        return;
      }
      settings[thresholds[i].key] = value;
    }
    for (let i = 0; i < durationFields.count; ++i) {
      const field = durationFields.itemAt(i);
      if (!field.editor.valid) {
        validationError = qsTr("Durations must be between 1 ms and 24 h.");
        return;
      }
      settings[intervals[i].key] = field.editor.seconds;
    }
    const percentage = Number(ratio.text);
    if (!ratio.acceptableInput || !(percentage > 0 && percentage <= 100)) {
      validationError = qsTr("The incomplete SYN threshold must be greater than 0% and at most 100%.");
      return;
    }
    settings.syn_incomplete_ratio = percentage / 100;
    for (let i = 0; i < detectorSwitches.count; ++i)
      settings[detectorSwitches.model[i].key] = detectorSwitches.itemAt(i).checked;
    settings.redact = redact.checked;
    for (let i = 0; i < resourceFields.count; ++i) {
      const value = Number(resourceFields.itemAt(i).input.text);
      if (!Number.isSafeInteger(value) || value < 1) { validationError = qsTr("Check the advanced resource limits."); return }
      settings[resourceLimits[i].key] = value;
    }
    validationError = controller.applySettings(settings) ? "" : controller.status;
  }
  Label { text: qsTr("Settings"); color: Theme.text; font.pixelSize: 25; font.weight: Font.DemiBold }
  Label {
    text: qsTr("Detection rules and time windows. Changes apply to the next analysis.")
    color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
  }
  ScrollView {
    id: settingsScroll
    objectName: "settingsScroll"
    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
    ColumnLayout {
      width: settingsScroll.availableWidth
      enabled: !controller.busy
      spacing: 18
      Label { text: qsTr("Detectors"); color: Theme.text; font.weight: Font.Bold }
      Flow {
        Layout.fillWidth: true; spacing: 12
        Repeater {
          id: detectorSwitches
          model: [{key: "arp_enabled", label: "ARP"}, {key: "scan_enabled", label: "Port scan"},
                  {key: "syn_enabled", label: "SYN flood"}, {key: "dns_enabled", label: "DNS"},
                  {key: "slowloris_enabled", label: "Slowloris"}]
          CheckBox { required property var modelData; text: modelData.label; checked: Boolean(page.values[modelData.key]) }
        }
      }
      Label { text: qsTr("Detection thresholds"); color: Theme.text; font.weight: Font.Bold }
      Repeater {
        id: thresholdFields
        model: page.thresholds
        ColumnLayout {
          required property var modelData
          property alias input: thresholdInput
          Layout.fillWidth: true
          Label { text: modelData.label; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
          InputField {
            id: thresholdInput
            objectName: modelData.key
            text: String(page.values[modelData.key] === undefined ? "" : page.values[modelData.key])
            Layout.fillWidth: true; font.family: Theme.codeFontFamily
            validator: RegularExpressionValidator { regularExpression: /^[0-9]+$/ }
            Accessible.name: modelData.label
          }
        }
      }
      Label { text: qsTr("Incomplete SYN threshold (%)"); color: Theme.muted }
      InputField {
        id: ratio; objectName: "syn_incomplete_percentage"
        text: String(Number(page.values.syn_incomplete_ratio || 0) * 100)
        Layout.fillWidth: true; Accessible.name: qsTr("Incomplete SYN threshold, percent")
        validator: DoubleValidator { bottom: 0; top: 100; decimals: 9; locale: "en_US"; notation: DoubleValidator.StandardNotation }
      }
      Label { text: qsTr("Time windows"); color: Theme.text; font.weight: Font.Bold }
      Label {
        text: qsTr("Choose ms, s, min or h. Changing a unit keeps the same duration.")
        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
      }
      Repeater {
        id: durationFields
        model: page.intervals
        ColumnLayout {
          required property var modelData
          property alias editor: durationEditor
          Layout.fillWidth: true
          Label { text: modelData.label; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
          DurationField {
            id: durationEditor
            objectName: modelData.key
            label: modelData.label
            initialSeconds: Number(page.values[modelData.key] || 0)
            Layout.fillWidth: true
          }
        }
      }
      CheckBox { id: redact; text: qsTr("Redact identities in exported reports"); checked: Boolean(page.values.redact) }
      ActionButton {
        objectName: "advancedResourceSettings"
        text: qsTr("Advanced resource limits")
        onClicked: { page.validationError = ""; resources.open() }
      }
      Label {
        text: qsTr("Tables show all records through pagination. Advanced resource limits affect analysis state and disk output; they do not cap table rows.")
        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
      }
    }
  }
  Label {
    objectName: "settingsValidationError"
    visible: page.validationError.length > 0; text: page.validationError
    color: Theme.warning; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true
  }
  ActionButton {
    objectName: "applyAnalysisSettings"
    text: qsTr("Apply settings"); primary: true; enabled: !controller.busy
    onClicked: page.apply()
  }
  Dialog {
    id: resources
    objectName: "resourceLimitsDialog"
    parent: Overlay.overlay; anchors.centerIn: parent; modal: true; focus: true
    title: qsTr("Analysis resource limits")
    width: Math.min(parent ? parent.width - 24 : 660, 660)
    height: Math.min(parent ? parent.height - 32 : 680, 680)
    standardButtons: Dialog.Close
    contentItem: ScrollView {
      id: resourcesScroll
      clip: true; contentWidth: availableWidth
      ColumnLayout {
        width: resourcesScroll.availableWidth; spacing: 12
        Label {
          text: qsTr("These limits are active. They bound in-memory tracking and report storage independently of table pagination. Capacity drops are reported on Dashboard.")
          wrapMode: Text.WordWrap; color: Theme.muted; Layout.fillWidth: true
        }
        Repeater {
          id: resourceFields; model: page.resourceLimits
          ColumnLayout {
            required property var modelData
            property alias input: resourceInput
            Layout.fillWidth: true
            Label { text: modelData.label; color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            InputField {
              id: resourceInput; objectName: modelData.key
              Layout.fillWidth: true
              text: String(page.values[modelData.key] || "")
              validator: RegularExpressionValidator { regularExpression: /^[0-9]+$/ }
              Accessible.name: modelData.label
              enabled: !controller.busy
            }
          }
        }
        Label {
          text: qsTr("Changes stay in this form until you choose Apply settings.")
          color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        ActionButton {
          text: qsTr("Done"); onClicked: resources.close()
        }
      }
    }
  }
}
