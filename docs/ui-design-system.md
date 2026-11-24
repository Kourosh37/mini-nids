# Qt Quick interface

The primary workflow is select PCAP -> optionally load reference intervals -> analyze
on a worker -> inspect evidence -> open local reports. A dark slate sidebar, teal
actions and restrained amber evidence highlight distinguish navigation from threats.
`Theme.qml` centralizes the dark-only palette, 16px base gaps, 10px radii and 44–48px
controls. Light colors, theme switches and the persisted theme property are removed.
Text accompanies every status color. The navigation
collapses to a drawer below 1000px; tables switch to wrapped cards below 760px.

`AppController` copies engine configuration into a QThread worker. Every table record
is written to a private temporary SQLite database and committed in batches. Paged
QAbstractTableModel instances load 100/250/500 rows, with debounced asynchronous search
over the full session and cancellable global sorting. Counts show range, matches and
total; a fixed pager supports direct page entry. Compact cards bind to the real model
so page changes replace their records too. There is no first-N-row display cap or
16,384-character detail truncation. Packet metadata is decoded on demand; the hex view
labels its 4,096-byte preview. File size/modification checks reject stale inspection.
No raw packet payload is cached indefinitely. Technical details are left-to-right.
See [search syntax and fields](table-search.md).
Search help is a modal beside the input, with table-specific fields and selectable
examples; Escape restores focus to its trigger. Timestamps display UTC with the
capture's six/nine-digit resolution, and the dashboard distinguishes capture span
from processing time. Settings uses named controls and unit selectors, with active
resource budgets in an advanced dialog. [Time and units](time-and-units.md) describes
the exact stored/displayed values and compatibility behavior.

The application is English-only and always left-to-right. The former language toggle,
translation catalog, translator and direction-switching code have been removed.
Inter Variable 4.1 is used for navigation, headings and controls; JetBrains Mono 2.304
Regular is used for packet tables, network addresses, counters, JSON and hexadecimal
details. Both fonts are compiled into Qt resources, with family tokens in Theme.qml,
and their SIL Open Font License texts are distributed with the application. The
monospaced technical font keeps address octets and byte columns aligned and makes
letters/digits easier to distinguish.

Nine sections are connected to real analysis state. Canvas and native progress bars
provide bounded timeline/protocol visuals; Qt Graphs integration, field-linked hex
highlighting, rich header trees, dedicated threshold comparisons and mobile document
provider integration are pending. A screen-reader runtime audit remains pending.

The Capture page accepts PCAP/ZIP input. A bounded scrollable archive list shows full
plain-text paths, sizes, actual container format and compatibility reasons.
Start analysis stays disabled until full preparation succeeds. A
selected capture uses the accent border and exposes metadata warnings in text. Import
progress/cancellation use the existing worker controls; nested archive extraction is absent.

`CapturePage.qml` has a fixed source toolbar and task footer, with no page-wide scroll
container. Only the ZIP list scrolls; paths, Options and Prepare/Start/Cancel remain
in place. The single primary action follows the selected row and its prepared state,
so selecting a different file cannot accidentally analyze the previous one. Recent
captures use a menu. Report destination and optional labels live in a modal Options
dialog, with its own scroll area for short displays.

`ArchivePicker.qml` filters filenames without losing their original archive indices.
Its local list model updates metadata rows in place. `archiveEntriesChanged` is a
dedicated controller notification; general progress/status changes never reset the list.
Selection supports pointer, arrow keys and Enter. Full filenames appear in dark tooltips
when truncated; tooltips stay hidden during scrolling. Changing input clears the filter.

`ActionButton.qml` provides bold labels, 48px targets, keyboard focus, hover color,
110ms press compression and a 240ms click flash. Native Button activation is retained
for pointer and keyboard input. `InputField.qml` adds a visible focus border and uses
the bundled mono font for paths and filters. Table headers/rows expose hover and
selection; selecting evidence opens the inspector. The sidebar omits technical stack
labels. Protocol bars explicitly draw a white fraction over a dark track.

The main window starts hidden, chooses the screen under the pointer (primary as a
fallback), fits the available geometry and positions itself at its center before
showing. Native smoke checks visit all nine sections at normal and narrow widths.
Qt 6.8.3 QML previews with synthetic controller/models checked all nine pages at
1320/760/420px, capture gating, mouse hover/press/click, keyboard activation, evidence
inspection and bar geometry with no QML warnings. These previews do not replace a
compiled C++/installer check; native CI must verify the resulting application.

`tests/ui/check_capture.py` checks actual wheel events, fixed panel geometry, retained
scroll position, 250-row filtering/index mapping, keyboard preparation, Options and
task actions at 1080/760/420/360px (including 360x600). It uses Qt 6.8.3 with synthetic
metadata, writes only to a temporary directory and cleans up on success or failure.
Build and release CI run it in an isolated PySide6-Essentials 6.8.3 test environment;
Python/PySide are not application dependencies or distributed with installers.
To run locally, create an ignored environment with `python -m venv tmp/ui-check`,
install `PySide6-Essentials==6.8.3` using that environment's Python, then run
`tests/ui/check_capture.py` with the same Python. Use a Python version supported by
that PySide release. The separate C++ controller test checks notifier isolation.
