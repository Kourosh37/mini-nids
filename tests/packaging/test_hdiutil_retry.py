"""Exercise the real DMG retry policy with a simulated hdiutil, on any host."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

WRAPPER = Path(__file__).resolve().parents[2] / "scripts/package/hdiutil-retry.sh"
HARNESS = r'''
source "$1"
shift
sync() { echo sync >> "$TEST_DIR/sync"; }
sleep() { echo "$1" >> "$TEST_DIR/sleep"; }
fake_hdiutil() {
  local count
  count=$(cat "$TEST_DIR/count")
  echo "$((count + 1))" > "$TEST_DIR/count"
  printf '<%s>' "$@" >> "$TEST_DIR/args"
  echo "${LC_ALL:-unset}" >> "$TEST_DIR/locale"
  if (( count < FAILURES )); then
    echo "$FAILURE_MESSAGE" >&2
    return "$FAILURE_CODE"
  fi
  echo 'created image'
}
mininids_hdiutil fake_hdiutil "$@"
'''


class HdiutilRetryTest(unittest.TestCase):
    def run_tool(self, failures=0, message="hdiutil: create failed - Resource busy",
                 code=1, operation="create"):
        with tempfile.TemporaryDirectory(prefix="DMG retry ") as directory:
            root = Path(directory)
            (root / "count").write_text("0")
            env = dict(os.environ, TEST_DIR=root.as_posix(), FAILURES=str(failures),
                       FAILURE_MESSAGE=message, FAILURE_CODE=str(code))
            args = [operation, "-ov", "-srcfolder", "/a path/Mini-NIDS.app", "/output path/temp.dmg"]
            result = subprocess.run([shutil.which("bash"), "-c", HARNESS, "test",
                                     WRAPPER.as_posix(), *args],
                                    env=env, text=True, capture_output=True, timeout=10)
            files = {path.name: path.read_text() for path in root.iterdir()}
            return result, files, "".join("<" + arg + ">" for arg in args)

    def test_success_needs_one_attempt_and_flushes_staged_files(self):
        result, files, args = self.run_tool()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(files["count"].strip(), "1")
        self.assertEqual(files["sync"], "sync\n")
        self.assertNotIn("sleep", files)
        self.assertEqual(files["args"], args)

    def test_busy_retries_and_preserves_arguments_and_diagnostics(self):
        result, files, args = self.run_tool(failures=2)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(files["count"].strip(), "3")
        self.assertEqual(files["sync"], "sync\n" * 3)
        self.assertEqual(files["sleep"], "2\n4\n")
        self.assertEqual(files["args"], args * 3)
        self.assertEqual(files["locale"], "C\n" * 3)
        self.assertEqual(result.stderr.count("hdiutil: create failed - Resource busy"), 2)
        self.assertIn("created image", result.stdout)

    def test_persistent_busy_stops_after_five_attempts(self):
        result, files, _ = self.run_tool(failures=99, code=16)
        self.assertEqual(result.returncode, 16)
        self.assertEqual(files["count"].strip(), "5")
        self.assertEqual(files["sleep"], "2\n4\n6\n8\n")
        self.assertEqual(result.stderr.count("hdiutil: create failed - Resource busy"), 5)

    def test_unrelated_failure_keeps_its_exit_status_without_retry(self):
        result, files, _ = self.run_tool(failures=99, message="hdiutil: create failed - No space left on device", code=28)
        self.assertEqual(result.returncode, 28)
        self.assertEqual(files["count"].strip(), "1")
        self.assertNotIn("sleep", files)

    def test_other_operations_are_not_retried_or_flushed(self):
        result, files, args = self.run_tool(failures=99, operation="attach")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(files["count"].strip(), "1")
        self.assertEqual(files["args"], args)
        self.assertNotIn("sync", files)
        self.assertNotIn("sleep", files)

    def test_other_operations_can_succeed(self):
        result, files, _ = self.run_tool(operation="verify")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(files["count"].strip(), "1")
        self.assertNotIn("sync", files)


if __name__ == "__main__":
    unittest.main()
