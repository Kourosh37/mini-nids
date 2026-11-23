pragma Singleton
import QtQuick

QtObject {
  readonly property bool dark: controller.dark
  readonly property color background: dark ? "#10151e" : "#f1f4f8"
  readonly property color surface: dark ? "#18202d" : "#ffffff"
  readonly property color sidebar: dark ? "#111923" : "#e7edf4"
  readonly property color border: dark ? "#2b394b" : "#cbd5e1"
  readonly property color text: dark ? "#edf3fc" : "#172638"
  readonly property color muted: dark ? "#a6b6cc" : "#4b617a"
  readonly property color accent: dark ? "#69dbcb" : "#086f65"
  readonly property color selected: dark ? "#203e43" : "#d4eee9"
  readonly property color warning: dark ? "#ffc078" : "#874800"
  readonly property color danger: dark ? "#ffa8a8" : "#a31e35"
  readonly property int gap: 16
  readonly property int radius: 10
  readonly property int controlHeight: 44
  readonly property int compactBreakpoint: 760
}
