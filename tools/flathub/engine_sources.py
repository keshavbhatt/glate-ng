#!/usr/bin/env python3
"""Print the Flathub manifest sources for Mozilla's translation engine.

    python3 tools/flathub/engine_sources.py > engine-sources.yaml

The Flathub build has no network, so the engine that cmake/FetchTranslationsEngine.cmake
fetches during a normal build comes in as source archives there (DOCS/DECISIONS.md ADR-001).
This reads GLATE_ENGINE_COMMIT and the submodule list from that file, looks up each
submodule's pinned commit through the GitHub API, downloads the archives to hash them, and
prints the `sources:` entries to paste after the glate-ng git source in
flathub/com.ktechpit.glate. Standard library only.
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CMAKE = ROOT / "cmake/FetchTranslationsEngine.cmake"
ENGINE_REPO = "mozilla/translations"
DEST = "third_party/translations"
USER_AGENT = "glate-flathub-sources"


def get(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response:
        return response.read()


def pinned() -> tuple[str, list[str]]:
    text = CMAKE.read_text()
    commit = re.search(r'set\(GLATE_ENGINE_COMMIT "([0-9a-f]{40})"', text)
    block = re.search(r"set\(_glate_engine_submodules\n(.*?)\)", text, re.S)
    if not commit or not block:
        raise SystemExit(f"cannot read the pin from {CMAKE}")
    return commit.group(1), block.group(1).split()


def submodule(repo: str, commit: str, path: str) -> tuple[str, str]:
    """(owner/name, commit) of the submodule at `path` in `repo` at `commit`."""
    info = json.loads(get(f"https://api.github.com/repos/{repo}/contents/{path}?ref={commit}"))
    if info.get("type") != "submodule":
        raise SystemExit(f"{repo}:{path} is not a submodule at {commit}")
    url = info["submodule_git_url"]
    name = re.sub(r"(\.git)?/?$", "", re.sub(r"^.*github\.com[/:]", "", url))
    return name, info["sha"]


def archive(repo: str, commit: str, dest: str) -> str:
    url = f"https://github.com/{repo}/archive/{commit}.tar.gz"
    digest = hashlib.sha256(get(url)).hexdigest()
    return f"      - type: archive\n        url: {url}\n        sha256: {digest}\n        dest: {dest}\n"


def main() -> None:
    commit, paths = pinned()
    out = [archive(ENGINE_REPO, commit, DEST)]
    for path in paths:
        repo, sha = submodule(ENGINE_REPO, commit, path)
        out.append(archive(repo, sha, f"{DEST}/{path}"))
        if path.endswith("/ruy"):
            # ruy builds its bundled cpuinfo, a submodule of its own.
            sub_repo, sub_sha = submodule(repo, sha, "third_party/cpuinfo")
            out.append(archive(sub_repo, sub_sha, f"{DEST}/{path}/third_party/cpuinfo"))
    sys.stdout.write(f"      # Mozilla's inference engine at {commit[:12]} with the submodules it uses.\n")
    sys.stdout.write("".join(out))


if __name__ == "__main__":
    main()
