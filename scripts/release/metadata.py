"""Compute safe, deterministic GitHub release names from the checkout and event."""
import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def metadata(version, ref_type, ref_name, sha, commit_time=None):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("invalid CMake project version")
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("invalid commit SHA")
    if ref_type == "tag":
        if not re.fullmatch(rf"v{re.escape(version)}(?:-[A-Za-z0-9.-]+)?", ref_name):
            raise ValueError("release tag must match the CMake project version, e.g. v" + version)
        label = ref_name[1:]
        preview = "-" in label
        tag = ref_name
    else:
        if type(commit_time) is not int or commit_time <= 0:
            raise ValueError("branch releases require the source commit timestamp")
        stamp = datetime.fromtimestamp(commit_time, timezone.utc).strftime("%Y%m%d%H%M%S")
        label = f"{version}-dev.{stamp}.g{sha[:12]}"
        preview = True
        tag = f"v{label}"
    return {"label": label, "tag": tag, "prerelease": str(preview).lower(),
            "title": f"Mini-NIDS {label}"}


if __name__ == "__main__":
    match = re.search(r"project\(MiniNIDS VERSION ([0-9.]+)", Path("CMakeLists.txt").read_text())
    if not match:
        raise SystemExit("CMake project version not found")
    commit_time = int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], text=True))
    values = metadata(match[1], os.environ["GITHUB_REF_TYPE"], os.environ["GITHUB_REF_NAME"],
                      os.environ["GITHUB_SHA"], commit_time)
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write("".join(f"{key}={value}\n" for key, value in values.items()))
