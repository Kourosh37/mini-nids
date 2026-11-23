# Application icon

`org.mininids.MiniNIDS.svg` is the editable source for the shield and packet pulse.
The PNG, Windows ICO and macOS ICNS are exports of the same artwork, covered by
the project license. Keep these exports together when changing the icon.

- PNG: 1024px, embedded in the Qt client for window/taskbar icons.
- ICO: 16/20/24/32/40/48/64/128/256px, embedded in the Windows EXE and Inno Setup.
- ICNS: standard macOS resolutions through 1024px, installed in the app bundle.
- SVG: installed by Linux packages for desktop launchers.

Exports are checked in so building installers requires no image conversion tools.
