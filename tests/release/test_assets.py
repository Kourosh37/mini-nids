import sys
import os
import json
import runpy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts/release"))
from prepare_assets import expected_assets, validate_assets


class ReleaseAssetsTest(unittest.TestCase):
    def test_notes_and_build_info_include_version_and_explicit_dates(self):
        script = Path(__file__).resolve().parents[2] / "scripts/release/prepare_assets.py"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "release-assets"
            directory.mkdir()
            label = "0.1.0-dev.20261123185109.gaaaaaaaaaaaa"
            for name in expected_assets(label):
                (directory / name).write_bytes(b"package")
            def archive(arguments, check):
                self.assertEqual(arguments[:2], ["git", "archive"])
                Path(arguments[3].removeprefix("--output=")).write_bytes(b"source")
            environment = {"RELEASE_LABEL": label, "RELEASE_SOURCE_TIME": "2026-11-23T18:51:09+00:00",
                           "GITHUB_SHA": "a" * 40, "GITHUB_RUN_ID": "123"}
            previous = Path.cwd()
            try:
                os.chdir(root)
                with patch.dict(os.environ, environment), patch("subprocess.run", side_effect=archive):
                    runpy.run_path(str(script), run_name="__main__")
            finally:
                os.chdir(previous)
            info = json.loads((directory / "BUILD-INFO.json").read_text())
            notes = (root / "release-notes.md").read_text(encoding="utf-8")
            self.assertEqual(info["version"], label)
            self.assertEqual(info["source_commit_time"], environment["RELEASE_SOURCE_TIME"])
            self.assertTrue(info["packaged_at"].endswith("+00:00"))
            self.assertIn(label, notes)
            self.assertIn(environment["RELEASE_SOURCE_TIME"], notes)
            self.assertIn("Packages assembled (UTC)", notes)

    def test_partial_and_empty_matrix_results_cannot_publish(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaises(ValueError):
                validate_assets(root, "0.1.0")
            for name in expected_assets("0.1.0"):
                (root / name).write_bytes(b"package")
            validate_assets(root, "0.1.0")
            (root / expected_assets("0.1.0")[0]).write_bytes(b"")
            with self.assertRaises(ValueError):
                validate_assets(root, "0.1.0")


if __name__ == "__main__":
    unittest.main()
