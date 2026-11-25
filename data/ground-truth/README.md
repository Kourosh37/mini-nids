# Reviewed capture-time reference intervals

These JSON arrays are compiled from
[reviewed TShark observations](../../docs/academic/reviewed-observations.json), not
from Mini-NIDS alerts. The observations record capture hashes, packet indices,
endpoints, decimal timestamps and the reasoning behind each label. They were reviewed
in this implementation session; they are not official CIC labels or independently
certified by another human. Capture visibility and attribution remain limited.

Regenerate with `python scripts/academic/compile_references.py`. Decimal timestamps
become exact integer Unix nanoseconds without floating-point conversion. Pass the
matching file to `mininids analyze --ground-truth data/ground-truth/NAME.json`.

The unit is an observed attack campaign/poison interval. The TCP and later UDP scan
phases are separate events. Repeated correct alert groups within one interval are
duplicates, not additional true-positive events. SYN references constrain destination
because the detector reports source concentration as an aggregate. ARP `src` is the
claimed gateway IP, not proof of the poisoner's actual IP. DNS includes observed ARP
co-attacks and five victims with visible forged-reply evidence. Empty benign reference
arrays are negative controls under this review, not a proof of universal benignness.

Metrics are conditional on these reviewed intervals, with zero matching tolerance.
They must not be advertised as externally validated accuracy or per-packet detection
quality. The annotation method, independent samples and uncertainties are explained
in the Persian presentation/report.

Optional `src_mac` constrains the triggering packet's observed Ethernet source, normalized
case-insensitively. ARP/DNS reference events use it to avoid crediting a legitimate gateway
response that merely occurs inside a poison interval. It is an observed identity constraint,
not proof of authenticated origin. Other categories need not specify it.
