import copy
import pathlib
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import release_provenance as provenance


class ReleaseProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = pathlib.Path(self.temp.name)
        for name in provenance.ASSETS:
            (self.directory / name).write_bytes(name.encode())
        self.manifest = {"schema": 1, "source": "source", "headsetcontrol": "dependency",
                         "run_id": "42", "version": "2.2.3", "assets": {
                             name: {"size": (self.directory / name).stat().st_size,
                                    "sha256": provenance.sha256(self.directory / name)}
                             for name in provenance.ASSETS}}

    def verify(self, manifest=None):
        return provenance.verify(self.manifest if manifest is None else manifest, self.directory,
                                 "source", "42", "2.2.3", "dependency")

    def test_matching_release(self):
        self.assertEqual(self.verify(), "2.2.3")

    def test_wrong_source_run_version_or_dependency(self):
        for key in ("schema", "source", "run_id", "version", "headsetcontrol"):
            with self.subTest(key=key):
                manifest = copy.deepcopy(self.manifest)
                manifest[key] = "wrong"
                with self.assertRaises(ValueError):
                    self.verify(manifest)

    def test_tampered_asset(self):
        name = next(iter(provenance.ASSETS))
        (self.directory / name).write_bytes(b"tampered")
        with self.assertRaises(ValueError):
            self.verify()

    def test_digest_mismatch_with_correct_size(self):
        name = next(iter(provenance.ASSETS))
        (self.directory / name).write_bytes(b"x" * self.manifest["assets"][name]["size"])
        with self.assertRaises(ValueError):
            self.verify()

    def test_missing_or_unexpected_asset(self):
        for name in ("../../installer.exe", "unexpected.exe"):
            manifest = copy.deepcopy(self.manifest)
            manifest["assets"][name] = {"size": 0, "sha256": ""}
            with self.assertRaises(ValueError):
                self.verify(manifest)
        (self.directory / next(iter(provenance.ASSETS))).unlink()
        with self.assertRaises(ValueError):
            self.verify()


if __name__ == "__main__":
    unittest.main()
