#!/usr/bin/env python3
"""Apply built macOS PKG metadata to catalog webDownloads without touching Hub ZIPs."""

from __future__ import annotations

import argparse
import json
from datetime import datetime
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    return parser.parse_args()


def web_download(package: dict[str, Any]) -> dict[str, Any]:
    return {
        "kind": "installer",
        "format": package["format"],
        "architecture": package["architecture"],
        "version": package["version"],
        "url": package["url"],
        "sha256": package["sha256"],
        "sizeBytes": package["sizeBytes"],
    }


def main() -> None:
    args = parse_args()
    with args.catalog.open(encoding="utf-8") as handle:
        catalog = json.load(handle)
    with args.manifest.open(encoding="utf-8") as handle:
        manifest = json.load(handle)

    packages = {
        (package["pluginId"], package["formatKey"]): package
        for package in manifest["packages"]
    }
    applied = 0
    for plugin in catalog.get("plugins", []):
        plugin_id = plugin["id"]
        vst3 = packages.get((plugin_id, "vst3"))
        au = packages.get((plugin_id, "au"))
        if not vst3 and not au:
            continue
        downloads = plugin.setdefault("webDownloads", {})
        if vst3:
            downloads["macosVst3"] = web_download(vst3)
            applied += 1
        if au:
            downloads["macosAu"] = web_download(au)
            applied += 1

    if applied != manifest["packageCount"]:
        raise RuntimeError(
            f"Applied {applied} package records, expected {manifest['packageCount']}"
        )

    catalog["updatedAt"] = datetime.now().astimezone().isoformat(timespec="seconds")
    args.catalog.write_text(
        json.dumps(catalog, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Applied {applied} macOS web package records")


if __name__ == "__main__":
    main()
