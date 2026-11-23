"""Exercise publication failure handling using a fake gh; never contacts GitHub."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().with_name("publish.sh")
FAKE_GH = r'''#!/usr/bin/env bash
printf '%s\n' "$*" >> "$FAKE_GH_LOG"
if [[ $1 == api ]]; then
  [[ $* != *'-X POST'* ]] || exit 0
  [[ $FAKE_STATE != absent ]] || exit 1
  printf '%s\n' "${FAKE_COMMIT:-$GITHUB_SHA}"; exit 0
fi
if [[ $2 == view ]]; then
  [[ $FAKE_STATE != absent ]] || exit 1
  if [[ $* == *isDraft* ]]; then
    [[ $FAKE_STATE != published ]] && echo true || echo false
  else echo package.zip; fi
fi
if [[ $2 == upload && $FAKE_STATE == upload-fails ]]; then exit 1; fi
'''


class PublishTest(unittest.TestCase):
    def run_publisher(self, state, wrong_commit=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / "fake-bin"
            binary.mkdir()
            fake = binary / "gh"
            fake.write_bytes(FAKE_GH.encode())
            fake.chmod(0o755)
            (root / "release-assets").mkdir()
            (root / "release-assets/package.zip").write_bytes(b"package")
            log = root / "calls.txt"
            environment = dict(os.environ, PATH=binary.as_posix() + os.pathsep + os.environ["PATH"],
                GH_TOKEN="fake", GH_REPO="test/repo", RELEASE_TAG="build-test", RELEASE_TITLE="Test",
                GITHUB_SHA="a"*40, PRERELEASE="true", FAKE_STATE=state, FAKE_GH_LOG=log.as_posix())
            if wrong_commit:
                environment["FAKE_COMMIT"] = "b" * 40
            result = subprocess.run([shutil.which("bash"), SCRIPT.as_posix()], cwd=root, env=environment,
                text=True, capture_output=True)
            return result, log.read_text() if log.exists() else ""

    def test_upload_failure_leaves_a_draft(self):
        result, calls = self.run_publisher("upload-fails")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("release edit", calls)

    def test_new_release_publishes_after_upload(self):
        result, calls = self.run_publisher("absent")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--draft --verify-tag --target", calls)
        self.assertIn("ref=refs/tags/build-test", calls)
        self.assertLess(calls.index("release upload"), calls.index("release edit"))
        self.assertIn("--prerelease=true --latest=false", calls)

    def test_published_release_is_preserved(self):
        result, calls = self.run_publisher("published")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("release upload", calls)

    def test_existing_draft_can_resume(self):
        result, calls = self.run_publisher("draft")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("release upload", calls)
        self.assertNotIn("release create", calls)

    def test_tag_cannot_move(self):
        result, calls = self.run_publisher("draft", wrong_commit=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("release upload", calls)


if __name__ == "__main__":
    unittest.main()
