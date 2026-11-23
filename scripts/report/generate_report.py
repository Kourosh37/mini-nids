"""Documentation helper only; packet analysis and application code are C++20."""
from pathlib import Path
import json
import subprocess
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.pagesizes import A4
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Image

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "output/pdf/mini-nids-engineering-report.pdf"
OUT.parent.mkdir(parents=True, exist_ok=True)
styles = getSampleStyleSheet()
styles.add(ParagraphStyle("BodyTextSmall", fontName="Helvetica", fontSize=9.5, leading=14, spaceAfter=9))
styles.add(ParagraphStyle("CodeSmall", fontName="Courier", fontSize=8, leading=11, spaceAfter=7))
styles.add(ParagraphStyle("CellSmall", fontName="Helvetica", fontSize=8.5, leading=12))
styles["Title"].textColor = colors.HexColor("#123c48")
styles["Heading1"].textColor = colors.HexColor("#123c48")
styles["Heading2"].textColor = colors.HexColor("#086f65")
story = []

def p(text, style="BodyTextSmall"):
    story.append(Paragraph(text, styles[style]))

def table(rows, widths):
    body = [[Paragraph(escape(str(cell)), styles["CellSmall"]) for cell in row] for row in rows]
    item = Table(body, colWidths=widths, repeatRows=1, hAlign="LEFT")
    item.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,0), colors.HexColor("#e4f1ef")),
        ("VALIGN", (0,0), (-1,-1), "TOP"),
        ("GRID", (0,0), (-1,-1), 0.35, colors.HexColor("#ccd5df")),
        ("LEFTPADDING", (0,0), (-1,-1), 8), ("RIGHTPADDING", (0,0), (-1,-1), 8),
        ("TOPPADDING", (0,0), (-1,-1), 8), ("BOTTOMPADDING", (0,0), (-1,-1), 8),
    ]))
    story.extend([item, Spacer(1,12)])

def page(title):
    if story: story.append(PageBreak())
    p(title, "Heading1")

commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
page("Mini-NIDS")
p("Engineering delivery and validation report", "Title")
p("23 November 2026 | C++20 / Qt 6.8.3 | Version 0.1.0, unreleased")
p("<b>Academic evaluation remains pending.</b> Working offline analysis, mandatory detector families, reports and a Qt Quick client are implemented. The four student PCAP files and independent ground truth were not supplied. Synthetic engineering fixtures are explicitly identified and do not establish real-world accuracy.")
table([
    ["Target", "Verified evidence"],
    ["Windows core and CLI", "GCC 15.2 Debug/Release; 37 automated checks."],
    ["Windows Qt client", "Qt 6.8.3 with matching MinGW 13.1; 39 checks including controller and offscreen QML navigation."],
    ["Linux core and CLI", "Ubuntu 24.04 under WSL; Clang 18, ASan and UBSan; 37 checks."],
    ["macOS / Android / iOS", "Unverified. Desktop CI configured; mobile toolchains and native document-provider integration pending."],
], [135, 370])
p("The original assignment's 22 pages were read, including rendered Persian pages because extracted text has font-encoding errors. Manual parsing, streaming, capture-time state, evidence and bounded resources were prioritized.")
p("Validated source revision: " + commit, "CodeSmall")
p("This report records engineering evidence and remaining work. It is not the final dataset-based academic submission.")

page("Architecture and safety")
p("PCAP source -> manual decoder -> flow/protocol state -> independent rules -> bounded alert aggregation -> CSV/JSON reports. The CLI and worker-backed Qt client share the same pure C++ core.")
p("The source owns one reusable packet buffer. Parser spans borrow bytes only until the next read. Flows, ARP/DNS contexts and alerts own copied metadata. State expiration maintains one timer per entry, avoiding a growing stale-timer heap or a complete state scan for every packet.")
table([
    ["Default bound", "Behavior"],
    ["262144-byte packet", "Validate PCAP signature, byte order, snaplen, record lengths and timestamp fractions before reading."],
    ["50000 flows / 10000 DNS / 4096 ARP", "Idle/active expiration; capacity exhaustion rejects new state and records loss of visibility."],
    ["100000 attempts / 10000 alert groups", "Incremental time windows and fixed-window evidence aggregation."],
    ["2000 rows per GUI model", "Batch snapshot transfer; 16384-character details; at most 4096 packet bytes read on demand."],
    ["256 MiB report budget", "New session directory required; checked writes/flushes; incomplete marker retained on failure."],
], [160,345])
p("DNS pointers must refer backward; names, hops and record counts are bounded. Snaplen truncation preserves intact headers. Non-initial IPv4 fragments are not transport-decoded; fragmented DNS and TCP streams are not reconstructed. Checksums remain diagnostic fields because capture offload can make them appear invalid.")
p("No payload logging, telemetry, network upload or packet-generated shell execution exists. Export redaction hides identity-bearing evidence. CSV escaping also neutralizes leading spreadsheet formula tokens. Filesystem parents must be trusted; size/mtime checks are not cryptographic integrity checks.")

page("Detectors and interpretation")
table([
    ["Rule", "Observed trigger", "Principal false-positive caveat"],
    ["ARP-01/02", "Repeated unsolicited mapping conflicts; Ethernet/ARP sender mismatch.", "DHCP, failover, replacement and proxy behavior. Gratuitous and solicited replies do not advance the conflict counter."],
    ["SCAN-01/02", "Source/target distinct destination ports; TCP completion/reset and UDP quoted ICMP evidence.", "NAT, monitoring, discovery and legitimate multi-service clients. Missing responses do not prove an attack."],
    ["SYN-01", "Destination SYN concentration plus incomplete count and incomplete ratio.", "Bursts, loss, asymmetric capture and SYN cookies. Retransmissions do not create new attempts."],
    ["DNS-01", "Different normalized answers in one observed endpoint/ID/question context.", "CDNs and load balancing; duplicate/reordered records and TTL-only differences are ignored."],
    ["DNS-02", "Unmatched/unexpected reply or observed ARP/Ethernet disagreement.", "Partial captures and resolver changes; low-severity heuristic."],
    ["DNS-03", "Response MAC correlated with recent suspicious ARP claims.", "Legitimate failover can affect both layers; inspect related evidence."],
], [70,220,215])
p("No network identity is hardcoded in production. Reserved documentation addresses occur only in explicitly synthetic tests. ARP first observation is an anchor, not trusted authentication. A local Ethernet MAC can identify a next hop rather than a remote sender; NAT can hide original addresses.")
p("Plain DNS over UDP and its transaction ID do not authenticate answer origin. DNS over TLS/HTTPS hides application evidence. TCP/ARP/scan evidence may remain available independently. Slowloris, learned baselines, horizontal scan classification and reassembly are not implemented.")
p("Primary alerts.csv has exactly timestamp, rule_id, attack, src, dst, evidence. Aggregation preserves the first trigger time, last trigger and count. Rules generate indicators for analyst review, not definitive attribution.")

page("Validation and performance")
p("Unit, regression, malformed-input and integration checks run through GoogleTest/CTest. The pipeline check verifies repeatable CSV/JSON, actual report files, bounded state, cancellation, redaction, output-budget failure and reference-interval evaluation. Qt tests cover worker analysis, cancellation, filtering, details and settings; QML smoke coverage visits all nine sections.")
p("Five libFuzzer entry points cover PCAP, DNS, IPv4, TCP and the dispatcher. Each completed 20000 seeded runs under ASan/UBSan without a reported failure. This is a bounded smoke campaign, not proof that no bugs remain. Logs are retained under docs/validation.")
bench = json.loads((ROOT / "docs/validation/windows-benchmarks.json").read_text(encoding="utf-8-sig"))
rows = [["Benchmark", "Median items / second"]]
for item in bench["benchmarks"]:
    if item.get("aggregate_name") == "median":
        rows.append([item["name"].removesuffix("_median"), f'{item.get("items_per_second", 0):,.0f}'])
table(rows, [290,215])
load = json.loads((ROOT / "docs/validation/windows-load-measurement.json").read_text(encoding="utf-8-sig"))
p("Measured machine: " + escape(load["hardware"]) + ". GCC 15.2 Release (-O3 -DNDEBUG), three benchmark repetitions. Parser microbenchmarks use hot synthetic headers; the full-pipeline benchmark uses 10000 frames and excludes CSV/GUI overhead.")
p(f'The separate 100000-frame load run measured {load["packets_per_second"]:,.0f} packets/s, {load["peak_working_set_bytes"]/1048576:.1f} MiB OS peak working set and {load["sampled_mean_working_set_bytes"]/1048576:.1f} MiB mean of {load["sample_count"]} sampled working sets. Sampling is approximate. These numbers are workload-specific baselines, not a sustained live-capture claim.')

page("Qt Quick client")
p("The nine sections provide dashboard, capture controls, packets, flows, alerts, DNS, evaluation, reports and settings. QAbstractTableModel/QSortFilterProxyModel expose real engine snapshots; no fixture data is embedded in the application.")
picture = ROOT / "docs/screenshots/dashboard-dark.png"
if picture.exists():
    story.append(Image(str(picture), width=505, height=505*860/1320))
    story.append(Spacer(1,12))
p("Actual Qt rendering with explicitly synthetic engineering traffic. Desktop, light and compact renders were inspected. Narrow layouts use drawer navigation and card rows. The interface is English-only and left-to-right, with bundled Inter and JetBrains Mono fonts.")
p("Choose Capture, select PCAP and destination, optionally import independent truth, and start. After completion filter model rows and inspect evidence. Reports opens the new session folder. Settings accepts strictly validated engine JSON. Analysis stays responsive through worker execution and batched updates.")
p("Remaining UI work includes Qt Graphs integration, field-linked hex highlighting, a richer header tree, threshold-comparison views and screen-reader runtime validation.")

page("Academic evaluation and handoff")
p("An event reference is an interval with category, optional entities, start_ns and end_ns. Each interval contributes at most one TP. Unmatched aggregated alerts are FP, intervals without a correct alert FN, and repeated correct alerts are duplicates. Delay uses the earliest correct trigger time relative to interval start. Undefined denominators and absent ground truth yield null metrics.")
p("The constructed 155-packet demo exercised all four categories and emitted five aggregate rule groups. Its known construction intervals are useful for integration checks only. They are not independent annotations or a substitute for student datasets.")
table([
    ["Needed input / work", "Next action"],
    ["DoS-SYN-stu.pcap, DNSspoofing-stu.pcap, ARPspoofing-stu.pcap, portScan-stu.pcap", "Keep local and outside Git. Inspect benign and attack intervals independently using Wireshark/tshark."],
    ["Independent reference intervals", "Load JSON truth with integer nanosecond capture timestamps. Merge overlapping same-entity/category intervals."],
    ["Per-file detector matrix and sensitivity", "Run scripts/test/evaluate-captures.ps1 for thresholds 10/20/40 and explain every off-diagonal result and false positive."],
    ["Final academic PDF", "Add real benign connection walkthrough, attack onset/identity evidence, measured accuracy, delay and false-positive analysis."],
    ["Additional platforms / optional features", "Verify macOS and Qt mobile toolchains; implement native file providers before claiming mobile support. Live capture and Slowloris remain deferred."],
], [215,290])
p("Detailed source documentation: README.md, RUNBOOK.md, docs/architecture.md, docs/protocol-parsing.md, docs/detection-rules.md, docs/evaluation.md, docs/security-model.md and docs/assignment-compliance.md. Git uses focused Conventional Commits; no remote push was performed.")

def decorate(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#69b8ac"))
    canvas.line(45, A4[1]-36, A4[0]-45, A4[1]-36)
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#4b617a"))
    canvas.drawString(45, 26, "MINI-NIDS | Engineering evidence | Academic dataset evaluation pending")
    canvas.drawRightString(A4[0]-45, 26, str(doc.page))
    canvas.restoreState()

doc = SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=45, rightMargin=45,
                        topMargin=52, bottomMargin=47, title="Mini-NIDS Engineering Delivery",
                        author="Mini-NIDS contributors")
doc.build(story, onFirstPage=decorate, onLaterPages=decorate)
print(OUT)
