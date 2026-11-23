"""Fail closed on incomplete desktop builds, then prepare release hashes and notes."""
import hashlib
import json
import os
import subprocess
from pathlib import Path
from datetime import datetime, timezone


def expected_assets(label):
    return [f"Mini-NIDS-{label}-{platform}.{suffix}" for platform, suffixes in (
        ("windows-x64-Setup", ("exe",)), ("windows-x64-portable", ("zip",)),
        ("linux-x64", ("tar.gz", "deb", "rpm")),
        ("macos-x64", ("dmg", "zip")), ("macos-arm64", ("dmg", "zip")),
    ) for suffix in suffixes]


def validate_assets(directory, label):
    expected = expected_assets(label)
    if sorted(p.name for p in directory.iterdir()) != sorted(expected):
        raise ValueError("incomplete or unexpected release assets; required: " + ", ".join(expected))
    for name in expected:
        if (directory / name).stat().st_size == 0:
            raise ValueError("empty release asset: " + name)


if __name__ == "__main__":
    directory = Path("release-assets")
    label = os.environ["RELEASE_LABEL"]
    source_time = os.environ["RELEASE_SOURCE_TIME"]
    built_at = datetime.now(timezone.utc).isoformat()
    validate_assets(directory, label)
    source = directory / f"Mini-NIDS-{label}-source.zip"
    subprocess.run(["git", "archive", "--format=zip", "--output=" + str(source), "HEAD"], check=True)
    (directory / "BUILD-INFO.json").write_text(json.dumps({
        "version": label, "commit": os.environ["GITHUB_SHA"], "qt": "6.8.3",
        "source_commit_time": source_time, "packaged_at": built_at,
        "workflow_run": os.environ["GITHUB_RUN_ID"],
        "signing_required": os.environ.get("MININIDS_REQUIRE_SIGNING") == "true",
    }, indent=2) + "\n", encoding="utf-8")
    hashes = []
    for asset in sorted(directory.iterdir()):
        with asset.open("rb") as stream:
            checksum = hashlib.sha256()
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                checksum.update(chunk)
            digest = checksum.hexdigest()
        hashes.append(f"{digest}  {asset.name}\n")
    (directory / "SHA256SUMS.txt").write_text("".join(hashes), encoding="utf-8")
    Path("release-notes.md").write_text(f"""Mini-NIDS {label}

**Version:** `{label}`

**Source commit date (UTC):** {source_time}

**Packages assembled (UTC):** {built_at}

Built from commit `{os.environ['GITHUB_SHA']}`. All desktop build/test/package jobs passed before publication.

| System | Download and run/install |
|---|---|
| Windows 10/11 x64 | `windows-x64-Setup.exe` offline installer, or extract the entire portable ZIP and open `Mini-NIDS.exe` |
| Linux x64 (Ubuntu 22.04+ baseline) | `.deb` for Debian/Ubuntu, `.rpm` for compatible RPM desktops; portable `.tar.gz` also included |
| macOS 13+ Intel | `macos-x64.dmg`: drag Mini-NIDS into Applications |
| macOS 13+ Apple Silicon | `macos-arm64.dmg`: drag Mini-NIDS into Applications |

Qt, QML modules, embedded fonts and compiler/runtime dependencies accompany the application.
No Qt SDK, Python or capture driver is required by end users. Keep the complete portable folder/app bundle together.
Linux needs a compatible desktop OS, glibc and native graphics drivers; native installers declare those OS dependencies.

Reports and settings use the user profile, outside the installed program. Uninstalling does not delete captures/reports.
Windows Authenticode and Apple Developer ID/notarization are optional repository-secret integrations:
**when no certificates are configured, artifacts are unsigned/ad-hoc and OS trust warnings can appear.**
The workflow does not bypass SmartScreen or Gatekeeper. Android/iOS packages are not included.

Use `SHA256SUMS.txt` to verify downloads. Source, dependency notices and licenses are included.
""", encoding="utf-8")
