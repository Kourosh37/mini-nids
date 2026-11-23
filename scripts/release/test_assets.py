import tempfile
import unittest
from pathlib import Path
from prepare_assets import expected_assets, validate_assets


class ReleaseAssetsTest(unittest.TestCase):
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
