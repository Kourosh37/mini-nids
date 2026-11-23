# Security

Treat PCAPs, JSON and output as untrusted. Bounds checks, packet/state/window/history
limits, DNS pointer/count limits and report byte budgets are mandatory. Offline
analysis requires no privileges, sends no telemetry and executes no commands from
capture data. Use authorized local captures. Reports contain network identifiers;
enable export redaction before sharing them. Hex inspection deliberately displays
packet bytes only on analyst request.

Report vulnerabilities privately to the project maintainer through the repository
owner's private security reporting channel when configured. Include a minimized
synthetic reproducer and toolchain, without secrets or private captures. No security
contact address or disclosure program is asserted until the repository owner sets one.
See docs/security-model.md for residual risks and validation evidence.
