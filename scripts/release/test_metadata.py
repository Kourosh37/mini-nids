import unittest
from metadata import metadata


class ReleaseMetadataTest(unittest.TestCase):
    def test_pushes_and_tags(self):
        sha = "a" * 40
        preview = metadata("0.1.0", "branch", 'feature/"unsafe-name', sha)
        self.assertEqual(preview["tag"], "build-" + sha[:12])
        self.assertEqual(preview["prerelease"], "true")
        self.assertNotIn("unsafe", str(preview))
        self.assertEqual(metadata("0.1.0", "tag", "v0.1.0", sha)["prerelease"], "false")
        self.assertEqual(metadata("0.1.0", "tag", "v0.1.0-rc.1", sha)["prerelease"], "true")
        with self.assertRaises(ValueError):
            metadata("0.1.0", "tag", "v0.2.0", sha)
        with self.assertRaises(ValueError):
            metadata("0.1.0", "branch", "main", "invalid")


if __name__ == "__main__":
    unittest.main()
