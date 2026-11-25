# Reproducible academic hand-in

The authoritative assignment is [the original 22-page PDF](../reference/Mini-NIDS.pdf).
Read [PRESENTATION.md](../../PRESENTATION.md) for the complete Persian explanations,
source excerpts, actual findings and question answers; [final PDF](../report/Mini-NIDS-report-fa.pdf)
is the printable report. The original assignment and final report are different files.

## Inputs and provenance

Download the [user-supplied dataset](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1)
and prepare its seven captures under data/pcaps/. Raw captures/ZIP are intentionally excluded
from Git and the submission archive. Capture hashes and reviewed evidence are in
reviewed-observations.json. These are agent-reviewed independent TShark observations,
not official publisher labels or second-human-certified ground truth. Never infer labels
from Mini-NIDS alerts or claim perfect/general accuracy.

## Reproduction

Build the C++ Release CLI (README/RUNBOOK). Optional research-only tools are Python,
TShark 4.6.10, Matplotlib and a browser that shapes Persian for PDF. They are not app
runtime dependencies. For example `python -m pip install matplotlib arabic-reshaper python-bidi` in an isolated
research environment. Input hashes are checked before study execution.

```sh
# For each NAME (Slowloris independent folder is named slowloris):
python scripts/academic/inspect_capture.py --tshark /path/to/tshark --input data/pcaps/NAME.pcap --output tmp/independent/NAME
python scripts/academic/compile_references.py
python scripts/academic/evaluate.py --executable /path/to/mininids --output output/study-new
python scripts/academic/summarize.py --study output/study-new --independent tmp/independent
python scripts/academic/packet_evidence.py --tshark /path/to/tshark
python scripts/academic/verify_false_alerts.py --tshark /path/to/tshark
python scripts/academic/plot_results.py --study output/study-new
python scripts/academic/build_presentation.py
```

Use a NEW study directory; no reports are overwritten. On Windows the build kit's runtime
DLLs must be on PATH or use a deployed CLI. Four port thresholds 10/20/40/1000 keep other
settings unchanged. Full reports include configuration and completion status. The summarizer
joins immediate/final groups by group_id, records every TP/duplicate/FP, and checks counts.
Baseline uses up to 60 complete seconds strictly before earliest observed onset, filling
absent seconds with zero. It is descriptive, not an authenticated or auto-learned baseline.

The packet exporter stores small samples and aggregates rather than copies of captures.
Its scan outcome counts cover the entire selected source/target pair, distinct from the
curated campaign boundaries. TShark layer counts and the engine's exclusive outer-protocol
counts are different definitions. Largest packet count and largest file bytes differ.

Source excerpt markers in PRESENTATION.md are refreshed from actual source lines by
build_presentation.py. Its output tmp/report/PRESENTATION.html embeds fonts and figures
for offline viewing/printing. The submission HTML keeps portable relative source links;
chapter navigation stays in the same document without a base-URL override.
Print with Chrome/Edge headless `--no-pdf-header-footer --print-to-pdf=ABS_PATH FILE_URL`,
then render/review every page. The committed PDF is docs/report/Mini-NIDS-report-fa.pdf.

## Deliverables and limits

- results.json: all 28 runs, metrics, throughput and OS process peak memory.
- rule-file-matrix.csv / sensitivity.csv / detection-delays.csv / baselines.csv.
- alert-review.csv: all 418 default groups, including all 43 unmatched groups and reasons.
- performance.csv and figures/ PNG/SVG: single warm-cache measurements, no CI intervals.
- packet-evidence/: independently extracted packet fields with frame indices/time/seq/ack.
- environment.json: tools/build/method provenance without private machine paths.
- data/ground-truth/: 20 reviewed campaign intervals, with optional source-MAC constraints.

Default conditional metrics are TP20/FP43/FN0; 355 repeated true groups are not extra TP.
The negative controls are not a proof of all-benign traffic. Benign1 overlaps the SYN-file
prefix; no independent train/test generalization claim. Hyperparameter tuning on these same
files would be optimistic. Gateway history drift causes DNS false alerts; UDP diversity
without independent scan intent is counted FP under these references, not relabelled for score.

The implemented live path and isolated Linux replay workflow still need an actual driver
experiment on a usable host. Local ABI fixtures do not measure real packet loss/latency.
See ../live-capture.md. Android/iOS, IP reassembly, IPv6 network decoding, TLS decryption and
full HTTP/2 parsing are not claimed.

Create the hand-in with `python scripts/academic/package_submission.py --study output/study-new`.
It contains current source/README, presentation HTML/Markdown, original/final PDFs, figures,
reference evidence, all per-file reports and a SHA-256 manifest; no PCAP/SDK/build/.git/logs.
The output is output/Mini-NIDS-submission.zip. Raw study directories are disposable once
this archive has been validated. This tracked research evidence is an explicitly requested
academic deliverable, distinct from disposable build/test logs.
