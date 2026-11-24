import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
  id: field
  property real initialSeconds: 1
  property real scale: 1
  readonly property real seconds: Number(valueInput.text) * scale
  readonly property bool valid: valueInput.acceptableInput && Number.isFinite(seconds) && seconds >= 0.001 && seconds <= 86400
  property string label
  property bool initialized: false
  spacing: 6
  function numberText(value) {
    const digits = scale === 0.001 ? 6 : scale === 1 ? 9 : scale === 60 ? 11 : 13;
    const nanoseconds = Math.round(value * scale * 1000000000);
    return (nanoseconds / 1000000000 / scale).toFixed(digits).replace(/0+$/, "").replace(/\.$/, "");
  }
  function load() {
    scale = initialSeconds >= 3600 && initialSeconds % 3600 === 0 ? 3600
          : initialSeconds >= 60 && initialSeconds % 60 === 0 ? 60
          : initialSeconds >= 1 ? 1 : 0.001;
    units.currentIndex = [0.001, 1, 60, 3600].indexOf(scale);
    valueInput.text = numberText(initialSeconds / scale);
  }
  onInitialSecondsChanged: if (initialized) load()
  Component.onCompleted: { initialized = true; load() }
  InputField {
    id: valueInput
    objectName: field.objectName + "Value"
    Layout.fillWidth: true
    validator: DoubleValidator { bottom: 0; top: 86400000; decimals: 15; locale: "en_US"; notation: DoubleValidator.StandardNotation }
    Accessible.name: field.label
    font.family: Theme.codeFontFamily
    ToolTip.visible: activeFocus && !field.valid
    ToolTip.text: qsTr("Use a duration from 1 ms to 24 h.")
  }
  ComboBox {
    id: units
    objectName: field.objectName + "Unit"
    model: ["ms", "s", "min", "h"]
    implicitWidth: 82
    Accessible.name: field.label + " " + qsTr("unit")
    onActivated: {
      const previousSeconds = Number(valueInput.text) * field.scale;
      field.scale = [0.001, 1, 60, 3600][currentIndex];
      valueInput.text = field.numberText(previousSeconds / field.scale);
    }
  }
}
