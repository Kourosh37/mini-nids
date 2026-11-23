"""Check real otool -L record shapes without requiring a macOS host."""
import shutil
import subprocess
import unittest
from pathlib import Path

PARSER = Path(__file__).resolve().parents[2] / "scripts/package/macho-dependencies.awk"


class MachoDependenciesTest(unittest.TestCase):
    def parse(self, text):
        result = subprocess.run([shutil.which("bash"), "-c", 'awk -f "$1"', "test", PARSER.as_posix()],
                                input=text, text=True, capture_output=True, check=True)
        return result.stdout.splitlines()

    def test_universal_architecture_headers_are_not_dependencies(self):
        path = "/Users/runner/work/mini-nids/build/package/Mini-NIDS.app/Contents/PlugIns/libworkerscriptplugin.dylib"
        records = ("\t@rpath/QtQml.framework/Versions/A/QtQml (compatibility version 6.0.0, current version 6.8.3)\n"
                   "\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1345.120.2)\n")
        output = path + " (architecture x86_64):\n" + records + path + " (architecture arm64):\n" + records
        self.assertEqual(self.parse(output), ["@rpath/QtQml.framework/Versions/A/QtQml",
                                             "/usr/lib/libSystem.B.dylib"] * 2)

    def test_real_sdk_dependency_is_preserved_for_rejection(self):
        dependency = "/Users/runner/Qt SDK/6.8.3/macos/lib/QtCore.framework/Versions/A/QtCore"
        output = "/Users/runner/app/plugin.dylib:\n\t" + dependency + " (compatibility version 6.0.0, current version 6.8.3)\n"
        self.assertEqual(self.parse(output), [dependency])


if __name__ == "__main__":
    unittest.main()
