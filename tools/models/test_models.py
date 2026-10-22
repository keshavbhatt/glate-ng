"""Tests for models.py. Run with: python3 -m unittest discover -s tools/models"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import models  # noqa: E402


def record(source: str, target: str, version: str, kind: str, name: str) -> dict:
    return {
        "fromLang": source,
        "toLang": target,
        "version": version,
        "fileType": kind,
        "name": name,
        "attachment": {"size": 10, "hash": "a" * 64, "location": f"loc/{name}"},
    }


def full_set(source: str, target: str, version: str, model: str = "model.bin") -> list[dict]:
    pair = f"{source}{target}"
    return [
        record(source, target, version, "model", f"{version}.{pair}.{model}"),
        record(source, target, version, "lex", f"lex.{pair}.bin"),
        record(source, target, version, "vocab", f"vocab.{pair}.spm"),
    ]


class SelectLatestTest(unittest.TestCase):
    def test_picks_the_newest_stable_version(self) -> None:
        records = full_set("en", "de", "1.0") + full_set("en", "de", "2.1") + full_set("en", "de", "3.0a1")
        [entry] = models.select_latest(records)
        self.assertEqual(entry["version"], "2.1")
        self.assertEqual([f["kind"] for f in entry["files"]], ["model", "lex", "vocab"])

    def test_skips_an_incomplete_newer_version(self) -> None:
        newer = [r for r in full_set("en", "de", "2.0") if r["fileType"] != "lex"]
        [entry] = models.select_latest(full_set("en", "de", "1.0") + newer)
        self.assertEqual(entry["version"], "1.0")

    def test_accepts_split_vocabularies(self) -> None:
        records = [
            record("zh-Hans", "en", "1.0", "model", "model.zhen.bin"),
            record("zh-Hans", "en", "1.0", "lex", "lex.zhen.bin"),
            record("zh-Hans", "en", "1.0", "srcvocab", "srcvocab.zhen.spm"),
            record("zh-Hans", "en", "1.0", "trgvocab", "trgvocab.zhen.spm"),
        ]
        [entry] = models.select_latest(records)
        self.assertEqual(entry["from"], "zh-Hans")

    def test_assets_are_unique_and_safe(self) -> None:
        records = full_set("en", "de", "2.1") + full_set("de", "en", "2.0") + full_set("en", "fr", "1.0")
        entries = models.select_latest(records)
        assets = [f["asset"] for e in entries for f in e["files"]]
        self.assertEqual(len(assets), len(set(assets)))
        for asset in assets:
            self.assertRegex(asset, r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
            self.assertNotIn("..", asset)

    def test_gemm_precision_follows_the_model_name(self) -> None:
        [plain] = models.select_latest(full_set("en", "de", "1.0", model="intgemm8.bin"))
        [alphas] = models.select_latest(full_set("en", "fr", "1.0", model="intgemm.alphas.bin"))
        self.assertEqual(plain["gemm_precision"], "int8shiftAll")
        self.assertEqual(alphas["gemm_precision"], "int8shiftAlphaAll")


class CatalogTest(unittest.TestCase):
    def test_catalog_drops_mozilla_locations(self) -> None:
        lock = {"schema_version": 1, "models": models.select_latest(full_set("en", "de", "2.1"))}
        catalog = models.catalog_from_lock(lock, "https://example.org/dl")
        self.assertEqual(catalog["schema_version"], 1)
        self.assertEqual(catalog["base_url"], "https://example.org/dl/")
        self.assertNotIn("location", catalog["models"][0]["files"][0])

    def test_catalog_needs_https(self) -> None:
        with self.assertRaises(SystemExit):
            models.catalog_from_lock({"models": []}, "http://example.org/")


if __name__ == "__main__":
    unittest.main()
