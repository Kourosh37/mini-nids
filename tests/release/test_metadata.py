import sys
import unittest
from pathlib import Path
from datetime import datetime, timezone

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts/release"))
from metadata import metadata


class ReleaseMetadataTest(unittest.TestCase):
    def test_pushes_and_tags(self):
        sha = "a" * 40
        timestamp = int(datetime(2026, 11, 23, 18, 51, 9, tzinfo=timezone.utc).timestamp())
        preview = metadata("0.1.0", "branch", 'feature/"unsafe-name', sha, timestamp)
        self.assertEqual(preview["tag"], "v0.1.0-dev.20261123185109.g" + sha[:12])
        self.assertEqual(preview["prerelease"], "true")
        self.assertIn("2026-11-23 18:51:09 UTC", preview["title"])
        self.assertIn(preview["label"], preview["title"])
        self.assertEqual(preview["source_time"], "2026-11-23T18:51:09+00:00")
        self.assertNotIn("unsafe", str(preview))
        self.assertEqual(metadata("0.1.0", "tag", "v0.1.0", sha)["prerelease"], "false")
        self.assertEqual(metadata("0.1.0", "tag", "v0.1.0-rc.1", sha)["prerelease"], "true")
        with self.assertRaises(ValueError):
            metadata("0.1.0", "tag", "v0.2.0", sha)
        with self.assertRaises(ValueError):
            metadata("0.1.0", "branch", "main", "invalid")

    def test_same_day_order_uses_commit_time_instead_of_hash(self):
        timestamp = int(datetime(2026, 11, 23, 18, 0, tzinfo=timezone.utc).timestamp())
        older = metadata("0.1.0", "branch", "main", "f" * 40, timestamp)
        newer = metadata("0.1.0", "branch", "main", "0" * 40, timestamp + 1)
        self.assertGreater(newer["tag"], older["tag"])
        self.assertGreater(newer["label"], older["label"])
        self.assertEqual(newer, metadata("0.1.0", "branch", "main", "0" * 40, timestamp + 1))

    def test_missing_commit_time_is_rejected(self):
        for timestamp in [None, -1, 0, "20261123", True]:
            with self.assertRaises(ValueError):
                metadata("0.1.0", "branch", "main", "a" * 40, timestamp)


if __name__ == "__main__":
    unittest.main()
