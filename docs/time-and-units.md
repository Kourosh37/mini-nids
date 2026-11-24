# Time, precision and settings

## Capture timestamps

Packets, Alerts and DNS show the recorded time as a UTC clock value and date.
Microsecond PCAP storage displays six fractional digits; nanosecond storage displays
nine. Hover for the ISO 8601 UTC value and exact decimal Unix seconds. Sorting and
time comparisons use the original integer nanoseconds, never a rounded display value.
The container's resolution does not establish the accuracy of its capture clock.

Inspect includes explicit _utc timestamps alongside existing Unix timestamp strings.
Flows include first/last UTC values and their duration; DNS includes matched response
delay when query context is available. A negative delay can occur with timestamp
regression and must be interpreted with the capture's health counters.

The search time field accepts any of these equivalent boundaries:

    time>=2024-11-23T07:07:58.123456789Z
    time>=2024-11-23T10:37:58.123456789+03:30
    time>=1732345678.123456789

ISO input requires an explicit timezone (Z or an offset); there is no implicit
computer-local conversion. Dates such as 2024-11-23 can also be searched as plain
text across every page.

## Durations and rates

Human-readable durations choose exact ns, µs or ms below one second, then seconds
and combinations of minutes, hours and days. Fractional values retain the stored
nanoseconds, e.g. 1.234 µs, 1.000001 ms, 1 min 2.5 s.

- **Capture duration** is the span between the earliest and latest packet timestamps.
- **Processing time** is measured by the engine's monotonic clock. It includes sink
  processing, but not application startup and final report serialization.
- **Timeline** labels are elapsed durations from the displayed UTC bucket origin;
  bucket width is labelled separately. The origin may precede the first packet
  because capture-time buckets are aligned.
- **Analysis rate** uses packets/s; report performance also includes bytes/s. These
  are rates, not durations.
- **IPv4 TTL** is a hop limit, not seconds. DNS record TTL is in seconds.
  Inspection labels these ttl_hops and ttl_s / ttl_display.

Existing CSV headers and decimal Unix-second fields remain compatible. New
report_units.json documents their units and epoch, JSON suffixes, TTLs and rates.
JSON summaries add UTC strings and exact/display duration fields; evaluation adds
delay_ns / delay_display alongside delay_s. No detector threshold is changed
by formatting. Empty captures do not acquire invented 1970 UTC endpoints.

## Settings

The normal Settings form contains detector switches, thresholds, SYN percentage,
time windows and export redaction. Each duration has an explicit ms/s/min/h selector;
switching its unit preserves the duration at nanosecond resolution. Values must be
between 1 ms and 24 h, as required by the engine. JSON configuration remains in
seconds (keys ending in _s), rounded to the nearest integer nanosecond on import.

There is no UI row maximum. The obsolete max_ui_rows key is accepted only for old
configuration compatibility and is not emitted or shown. All seven remaining resource
limits are used by the engine/reader/report writer, so they remain under **Advanced
resource limits**, with names distinguishing entries from bytes. The max_alerts
setting limits pending deduplication state, not all emitted alert rows.
Edits in both sections are saved together by **Apply settings**; hidden budgets
and unrelated settings are preserved. Engine JSON editing remains available via
configuration files and the CLI.

Windows Qt 6.8.3 checks cover exact UTC/offset comparisons, micro/nanosecond display,
duration boundaries, subsecond configuration rounding and QML modal/settings
interactions. Native Linux/macOS checks run in CI.
