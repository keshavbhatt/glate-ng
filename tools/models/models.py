#!/usr/bin/env python3
"""Mirror Mozilla's Firefox translation models for glate (DOCS/DECISIONS.md ADR-004).

Mozilla publishes the models through Firefox Remote Settings. glate never reads
that endpoint at run time: this tool pins a set of models in models.lock.json,
and the models workflow copies exactly those files into the rolling `models`
release with a catalog.json the app reads.

    models.py lock                      refresh models.lock.json from Remote Settings
    models.py mirror OUT --base-url U   download the locked files into OUT, write OUT/catalog.json
    models.py install DEST ID [ID...]   install directions (en-de ...) into a glate model store,
                                        for tests and development
    models.py catalog OUT --base-url U  write only catalog.json from the lock (no downloads)

Standard library only, so it runs on a bare CI runner.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
import urllib.request
from pathlib import Path
from typing import Any, Iterable

HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "models.lock.json"
REMOTE_SETTINGS = "https://firefox.settings.services.mozilla.com/v1"
RECORDS_URL = f"{REMOTE_SETTINGS}/buckets/main/collections/translations-models/records"
CATALOG_SCHEMA_VERSION = 1
LOCK_SCHEMA_VERSION = 1
FILE_KINDS = ("model", "lex", "vocab", "srcvocab", "trgvocab")
USER_AGENT = "glate-models-mirror"
CHUNK = 1 << 20


def fetch_json(url: str) -> Any:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def parse_version(text: str) -> tuple[int, ...] | None:
    """'2.1' -> (2, 1). Pre-releases such as '1.0a1' are skipped (None)."""
    if not re.fullmatch(r"\d+(\.\d+)*", text):
        return None
    return tuple(int(part) for part in text.split("."))


def gemm_precision(model_name: str) -> str:
    # Same rule as Firefox: plain int8 models end in intgemm8.bin, the rest carry alphas.
    return "int8shiftAll" if model_name.endswith("intgemm8.bin") else "int8shiftAlphaAll"


def select_latest(records: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    """Newest stable version of every direction that has a complete file set."""
    by_direction: dict[tuple[str, str], dict[tuple[int, ...], list[dict[str, Any]]]] = {}
    for record in records:
        version = parse_version(str(record.get("version", "")))
        if version is None or record.get("fileType") not in FILE_KINDS:
            continue
        key = (record["fromLang"], record["toLang"])
        by_direction.setdefault(key, {}).setdefault(version, []).append(record)

    entries = []
    for (source, target), versions in sorted(by_direction.items()):
        for version in sorted(versions, reverse=True):
            files = {r["fileType"]: r for r in versions[version]}
            if len(files) != len(versions[version]):
                continue  # the same kind twice: not a set we understand
            if not _complete(files):
                continue
            entries.append(_lock_entry(source, target, version, files))
            break
    return entries


def _complete(files: dict[str, Any]) -> bool:
    vocab = "vocab" in files or ("srcvocab" in files and "trgvocab" in files)
    return "model" in files and "lex" in files and vocab


def _lock_entry(source: str, target: str, version: tuple[int, ...], files: dict[str, Any]) -> dict[str, Any]:
    version_text = ".".join(str(part) for part in version)
    locked = []
    for kind in FILE_KINDS:
        if kind not in files:
            continue
        record = files[kind]
        attachment = record["attachment"]
        locked.append(
            {
                "kind": kind,
                "name": record["name"],
                # Unique across the release: direction, version, original name.
                "asset": f"{source}-{target}-{version_text}-{record['name']}",
                "size": attachment["size"],
                "sha256": attachment["hash"],
                "location": attachment["location"],
            }
        )
    return {
        "from": source,
        "to": target,
        "version": version_text,
        "gemm_precision": gemm_precision(files["model"]["name"]),
        "files": locked,
    }


def read_lock() -> dict[str, Any]:
    lock = json.loads(LOCK_PATH.read_text())
    if lock.get("schema_version") != LOCK_SCHEMA_VERSION:
        raise SystemExit(f"{LOCK_PATH.name}: unsupported schema_version")
    return lock


def catalog_from_lock(lock: dict[str, Any], base_url: str) -> dict[str, Any]:
    if not base_url.startswith("https://"):
        raise SystemExit("--base-url must be https")
    models = []
    for entry in lock["models"]:
        files = [{k: f[k] for k in ("kind", "name", "asset", "size", "sha256")} for f in entry["files"]]
        models.append({k: entry[k] for k in ("from", "to", "version", "gemm_precision")} | {"files": files})
    return {
        "schema_version": CATALOG_SCHEMA_VERSION,
        "base_url": base_url if base_url.endswith("/") else base_url + "/",
        "source": "Mozilla Firefox translation models (MPL-2.0), mirrored unchanged",
        "models": models,
    }


def download(url: str, dest: Path, size: int, sha256: str) -> None:
    """Streams url to dest, refusing anything that does not match size and hash."""
    if dest.is_file() and dest.stat().st_size == size and _sha256(dest) == sha256:
        return
    digest = hashlib.sha256()
    received = 0
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with tempfile.NamedTemporaryFile(dir=dest.parent, delete=False) as tmp:
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                while chunk := response.read(CHUNK):
                    received += len(chunk)
                    if received > size:
                        raise ValueError(f"{dest.name}: larger than {size} bytes")
                    digest.update(chunk)
                    tmp.write(chunk)
            if received != size or digest.hexdigest() != sha256:
                raise ValueError(f"{dest.name}: size or sha256 mismatch")
        except BaseException:
            os.unlink(tmp.name)
            raise
    os.replace(tmp.name, dest)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(CHUNK):
            digest.update(chunk)
    return digest.hexdigest()


def attachments_base() -> str:
    return str(fetch_json(f"{REMOTE_SETTINGS}/")["capabilities"]["attachments"]["base_url"])


def cmd_lock(_: argparse.Namespace) -> None:
    records = fetch_json(RECORDS_URL)["data"]
    models = select_latest(records)
    lock = {"schema_version": LOCK_SCHEMA_VERSION, "source": RECORDS_URL, "models": models}
    LOCK_PATH.write_text(json.dumps(lock, indent=2, sort_keys=False) + "\n")
    total = sum(f["size"] for m in models for f in m["files"])
    print(f"{len(models)} directions, {total / 1e9:.2f} GB -> {LOCK_PATH.name}")


def cmd_mirror(args: argparse.Namespace) -> None:
    lock = read_lock()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    base = attachments_base()
    for entry in lock["models"]:
        for file in entry["files"]:
            download(base + file["location"], out / file["asset"], file["size"], file["sha256"])
        print(f"{entry['from']}-{entry['to']} {entry['version']}")
    catalog = catalog_from_lock(lock, args.base_url)
    (out / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n")


def cmd_catalog(args: argparse.Namespace) -> None:
    catalog = catalog_from_lock(read_lock(), args.base_url)
    Path(args.out).write_text(json.dumps(catalog, indent=2) + "\n")


def cmd_install(args: argparse.Namespace) -> None:
    lock = read_lock()
    wanted = set(args.ids)
    entries = [e for e in lock["models"] if f"{e['from']}-{e['to']}" in wanted]
    missing = wanted - {f"{e['from']}-{e['to']}" for e in entries}
    if missing:
        raise SystemExit(f"not in {LOCK_PATH.name}: {', '.join(sorted(missing))}")
    base = attachments_base()
    for entry in entries:
        directory = Path(args.dest) / f"{entry['from']}-{entry['to']}"
        directory.mkdir(parents=True, exist_ok=True)
        for file in entry["files"]:
            download(base + file["location"], directory / file["name"], file["size"], file["sha256"])
        # The same model.json the app writes (ModelStore::commit).
        manifest = {k: entry[k] for k in ("from", "to", "version", "gemm_precision")}
        manifest["files"] = [{k: f[k] for k in ("kind", "name", "size", "sha256")} for f in entry["files"]]
        (directory / "model.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"installed {directory}")


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("lock", help="refresh models.lock.json").set_defaults(func=cmd_lock)
    mirror = sub.add_parser("mirror", help="download the locked files and write catalog.json")
    mirror.add_argument("out")
    mirror.add_argument("--base-url", required=True, help="release download URL the assets will live under")
    mirror.set_defaults(func=cmd_mirror)
    catalog = sub.add_parser("catalog", help="write catalog.json from the lock without downloading")
    catalog.add_argument("out")
    catalog.add_argument("--base-url", required=True)
    catalog.set_defaults(func=cmd_catalog)
    install = sub.add_parser("install", help="install directions into a glate model store")
    install.add_argument("dest")
    install.add_argument("ids", nargs="+")
    install.set_defaults(func=cmd_install)
    args = parser.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    sys.exit(main())
