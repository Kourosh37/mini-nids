# Bundled typography

- Inter Variable 4.1: navigation, headings, labels and controls.
  Source: https://github.com/rsms/inter/tree/v4.1/docs/font-files
- JetBrains Mono 2.304 Regular: addresses, packet/flow tables, counters, JSON and hex.
  Source: https://github.com/JetBrains/JetBrainsMono/tree/v2.304/fonts/ttf

Both fonts are bundled as Qt resources and licensed under the SIL Open Font License
1.1. Their original license texts are included alongside the fonts and installed in
`share/mininids/licenses/fonts`. No runtime font download or system-font installation
is required. `Theme.qml` provides the two font-family tokens for QML components.

SHA-256 of the unmodified font files:

```text
4989b125924991b90d05b2d16e0e388c48f7d5bb8b30539bbf9c755278d0ccaf  InterVariable.ttf
a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f  JetBrainsMono-Regular.ttf
```
