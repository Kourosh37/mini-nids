# Qt Quick interface

The primary workflow is select PCAP -> optionally load reference intervals -> analyze
on a worker -> inspect evidence -> open local reports. A dark slate sidebar, teal
actions and restrained amber evidence highlight distinguish navigation from threats.
`Theme.qml` centralizes colors, 16px base gaps, 10px radii and 44px controls. Light mode
uses dark text on white surfaces. Text accompanies every status color. The navigation
collapses to a drawer below 1000px; tables switch to wrapped cards below 760px.

`AppController` copies engine configuration into a QThread worker. Progress is batched
and throttled; model data transfers once when analysis finishes. QAbstractTableModel
sources and QSortFilterProxyModel provide numeric sorting and tokenized text filtering.
Histories retain the first configured number of rows, not all packets. Detail text is
capped at 16384 characters per row; raw bytes are read from the selected file on demand,
at most 4096 bytes. File size/modification checks reject stale inspection. No raw packet
payload is cached indefinitely. Technical data uses left-to-right detail views.

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
