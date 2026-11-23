# Complete session tables and live search

Packets, Flows, Alerts and DNS messages include **every record emitted by the analysis**.
There is no 2,000-row display cap. Tables use a private SQLite session database in the
OS temporary directory and hold only the current page in memory. SQLite/FTS5 is linked
into the GUI; users install no database server, driver or additional runtime.
Builds with `MININIDS_FETCH_DEPS=OFF` require an installed SQLite >=3.34 with FTS5
and its trigram tokenizer; the vcpkg GUI feature requests FTS5 explicitly.
Normal exit or replacing a session removes its temporary database. Captures and saved
CSV/JSON reports remain separate. A storage error fails visibly rather than silently
discarding later rows. A cancelled analysis contains only its processed portion.

Use Previous/Next, First/Last, or type a page number. Page sizes are 100, 250 and 500;
compact windows retain Previous/Next and the editable page number. Counts distinguish
the current range, all search matches and all session records. Sorting a column orders
the **entire matching result**, including numeric counts and capture timestamps.

Search starts automatically 150 ms after typing stops, across all committed session
records, including pages never opened. New analysis records become visible in batches.
Search/count/sort run on worker threads; changing the query cancels obsolete work.
While a query runs, table inspection and paging wait for its consistent result.
Invalid syntax displays an error and no misleading unfiltered results.

## Search syntax

- Plain words and `"quoted phrases"` search visible values; non-packet tables also
  include their stored evidence/details. Spaces and `AND` combine conditions.
- `OR` joins alternatives. AND binds more tightly than OR. Parentheses are not supported.
- `-term` excludes a word, phrase or field predicate.
- `field:value` finds contained text; `field=value` matches text exactly.
- Numeric fields accept `:`, `=`, `!=`, `>`, `>=`, `<`, `<=`.
- `time` is capture time in Unix seconds, optionally with decimals.
- `port` matches either transport endpoint; `src_port` / `dst_port` select one.

| Table | Supported fields |
|---|---|
| Packets | `id`, `time`, `src`, `dst`, `protocol` (`proto`), `bytes`, `src_port`, `dst_port`, `port` |
| Flows | `src`, `dst`, `protocol` (`proto`), `packets`, `bytes`, `state`, `src_port`, `dst_port`, `port` |
| Alerts | `time`, `rule`, `severity`, `src`, `dst`, `count` |
| DNS messages | `time`, `id`, `direction`, `name`, `src`, `rcode` |

Examples:

```text
protocol:TCP bytes>=1000 -port:443
id>2000 dst_port>=20 dst_port<=1024
src:192.0.2 OR dst:192.0.2
rule:SCAN-01 severity=medium
direction:Response name:example.org
```

Substring search of three or more characters uses the FTS5 trigram index; shorter
terms and some field predicates require background scans. Search latency depends
on file size, storage and query selectivity; the GUI does not promise instantaneous
completion of every query. Packet payloads are not text-indexed. Inspect decodes the
selected packet on demand and explicitly labels its first 4,096 captured bytes as a
hex preview. Packet, flow, alert and DNS row availability has no prefix limit.

Engine state budgets are independent of pagination. Inspect `state_drops`, malformed
and truncated counts before interpreting detections. See [rules](detection-rules.md)
and the [real dataset audit](evaluation.md#local-dataset-audit-2026-11-24).
