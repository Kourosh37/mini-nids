pragma Singleton
import QtQuick

QtObject {
  readonly property FontLoader uiTypeface: FontLoader { source: "qrc:/fonts/InterVariable.ttf" }
  readonly property FontLoader codeTypeface: FontLoader { source: "qrc:/fonts/JetBrainsMono-Regular.ttf" }
  readonly property string uiFontFamily: uiTypeface.name
  readonly property string codeFontFamily: codeTypeface.name
  readonly property color background: "#10151e"
  readonly property color surface: "#18202d"
  readonly property color sidebar: "#111923"
  readonly property color border: "#34445a"
  readonly property color text: "#edf3fc"
  readonly property color muted: "#a6b6cc"
  readonly property color accent: "#69dbcb"
  readonly property color selected: "#203e43"
  readonly property color hover: "#263549"
  readonly property color warning: "#ffc078"
  readonly property color danger: "#ffa8a8"
  readonly property color progressTrack: "#090e16"
  readonly property color progressFill: "#ffffff"
  readonly property int gap: 16
  readonly property int radius: 10
  readonly property int controlHeight: 44
  readonly property int compactBreakpoint: 760
}
