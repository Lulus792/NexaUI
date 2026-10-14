"""Verify campaign isolation and failure propagation without a fuzz toolchain."""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "run_fuzz_campaign", Path(__file__).resolve().parents[1] / "tools" / "run_fuzz_campaign.py")
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)


class CampaignTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.corpus = self.root / "seeds"
        (self.corpus / "text").mkdir(parents=True)
        (self.corpus / "text" / "seed.txt").write_bytes(b"abc")
        suffix = ".exe" if os.name == "nt" else ""
        (self.build / f"nexaui_text_fuzz{suffix}").write_bytes(b"mock executable")
        self.output = self.root / "output"

    def run_campaign(self):
        return RUNNER.run_campaign(self.build, self.output, ["text"],
                                   100, 1800, 30, self.corpus)

    def test_copies_seeds_and_records_failure(self):
        def fail(command, **options):
            Path(command[1], "new-discovery").write_bytes(b"mutated")
            options["stdout"].write(b"ERROR: AddressSanitizer: test finding\n")
            return subprocess.CompletedProcess(command, 77)
        with patch.object(RUNNER.subprocess, "run", side_effect=fail):
            self.assertEqual(self.run_campaign(), 1)
        self.assertEqual(list((self.corpus / "text").iterdir()),
                         [self.corpus / "text" / "seed.txt"])
        self.assertEqual((self.corpus / "text" / "seed.txt").read_bytes(), b"abc")
        summary = json.loads((self.output / "summary.json").read_text())
        self.assertEqual(summary["targets"][0]["exit_code"], 77)
        self.assertEqual(summary["sanitizer_environment"]["UBSAN_OPTIONS"], "halt_on_error=1")

    def test_timeout_is_failure_with_report(self):
        with patch.object(RUNNER.subprocess, "run", side_effect=subprocess.TimeoutExpired([], 30)):
            self.assertEqual(self.run_campaign(), 1)
        summary = json.loads((self.output / "summary.json").read_text())
        self.assertEqual(summary["targets"][0]["exit_code"], 124)

    def test_success_and_coverage(self):
        def success(command, **options):
            options["stdout"].write(b"#100 DONE cov: 123 ft: 456\n")
            return subprocess.CompletedProcess(command, 0)
        with patch.object(RUNNER.subprocess, "run", side_effect=success):
            self.assertEqual(self.run_campaign(), 0)
        summary = json.loads((self.output / "summary.json").read_text())
        self.assertEqual(summary["targets"][0]["final_coverage_counters"], 123)

    def test_rejects_output_reuse(self):
        self.output.mkdir()
        with self.assertRaises(FileExistsError):
            self.run_campaign()

    def test_rejects_source_corpus_output(self):
        self.output = self.corpus / "generated"
        with self.assertRaises(ValueError):
            self.run_campaign()
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
