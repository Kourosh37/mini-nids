"""Compute safe, deterministic GitHub release names from the checkout and event."""
import os
import re
from pathlib import Path


def metadata(version, ref_type, ref_name, sha):
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
        label = f"{version}-dev.{sha[:12]}"
        preview = True
        tag = f"build-{sha[:12]}"
    return {"label": label, "tag": tag, "prerelease": str(preview).lower(),
            "title": f"Mini-NIDS {label}"}


if __name__ == "__main__":
    match = re.search(r"project\(MiniNIDS VERSION ([0-9.]+)", Path("CMakeLists.txt").read_text())
    if not match:
        raise SystemExit("CMake project version not found")
    values = metadata(match[1], os.environ["GITHUB_REF_TYPE"], os.environ["GITHUB_REF_NAME"],
                      os.environ["GITHUB_SHA"])
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write("".join(f"{key}={value}\n" for key, value in values.items()))
