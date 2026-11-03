"""Opt-in integration checks for an installed MO build with a configured instance."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


class InspectTests(unittest.TestCase):
    def run_inspect(self, *args, cwd=None):
        return subprocess.run(
            [
                str(self.options.mo2),
                "-i",
                self.options.instance,
                "-p",
                self.options.profile,
                *args,
            ],
            capture_output=True,
            timeout=120,
            check=False,
            cwd=cwd,
        )

    def snapshot(self):
        result = self.run_inspect("inspect", "--json")
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
        # Parse the entire stream, rather than extracting a JSON-looking substring:
        # startup/shutdown messages on stdout would break callers.
        snapshot = json.loads(result.stdout.decode("utf-8"))
        self.assertEqual(snapshot["schemaVersion"], 1)
        self.assertEqual(snapshot["profile"]["name"], self.options.profile)
        self.assertTrue(snapshot["instance"]["path"])
        self.assertTrue(snapshot["game"]["path"])
        for field in ("mods", "plugins", "executables"):
            self.assertIsInstance(snapshot[field], list)
        for field in ("mods", "plugins"):
            priorities = [item["priority"] for item in snapshot[field]]
            self.assertEqual(priorities, sorted(priorities))
            for item in snapshot[field]:
                self.assertIsInstance(item["enabled"], bool)
        if self.options.expected_plugin:
            self.assertIn(
                self.options.expected_plugin,
                [plugin["name"] for plugin in snapshot["plugins"]],
            )
        return snapshot

    def test_stdout_contains_complete_snapshot(self):
        self.snapshot()

    def test_inspection_preserves_loaded_state(self):
        self.assertEqual(self.snapshot(), self.snapshot())

    def test_unicode_output_path_matches_stdout(self):
        expected = self.snapshot()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "状态 snapshot.json"
            output.write_text("previous contents", encoding="utf-8")
            result = self.run_inspect("inspect", "--json", "--output", str(output))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, b"")
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), expected)

    def test_missing_output_directory_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "missing" / "snapshot.json"
            result = self.run_inspect("inspect", "--json", "--output", str(output))
            self.assertEqual(result.returncode, 1)
            self.assertFalse(output.exists())

    def test_output_path_is_relative_to_caller(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_inspect(
                "inspect", "--json", "--output", "relative.json", cwd=directory
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            snapshot = json.loads(
                (Path(directory) / "relative.json").read_text(encoding="utf-8")
            )
            self.assertEqual(snapshot["profile"]["name"], self.options.profile)

    def test_directory_output_is_not_replaced(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_inspect("inspect", "--json", "--output", directory)
            self.assertEqual(result.returncode, 1)
            self.assertTrue(Path(directory).is_dir())

    def test_logs_cannot_contaminate_json_stdout(self):
        result = self.run_inspect("--logs", "inspect", "--json")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b"")
        self.assertIn(b"--output", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mo2", required=True, type=Path)
    parser.add_argument("--instance", required=True)
    parser.add_argument("--profile", default="Default")
    parser.add_argument("--expected-plugin")
    InspectTests.options, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
