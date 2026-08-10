#!/usr/bin/env python3
"""Build per-format macOS PKG installers from production catalog ZIP assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import plistlib
import shutil
import subprocess
import tempfile
import urllib.parse
import urllib.request
from datetime import datetime
from pathlib import Path
from typing import Any


def run(*args: str, capture: bool = False) -> str:
    result = subprocess.run(
        args,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )
    return result.stdout if capture else ""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_cached_asset(cache: Path | None, name: str, expected_sha: str) -> Path | None:
    if cache is None:
        return None
    for candidate in sorted(cache.rglob(name)):
        if candidate.is_file() and sha256(candidate) == expected_sha:
            return candidate
    return None


def download(url: str, destination: Path) -> None:
    request = urllib.request.Request(url, headers={"User-Agent": "TB-Hub-PKG-Builder/1.0"})
    with urllib.request.urlopen(request) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output)


def validate_source_archive(path: Path, asset: dict[str, Any]) -> None:
    expected_size = int(asset["sizeBytes"])
    actual_size = path.stat().st_size
    if actual_size != expected_size:
        raise RuntimeError(f"{path.name}: expected {expected_size} bytes, got {actual_size}")
    expected_sha = str(asset["sha256"]).lower()
    actual_sha = sha256(path)
    if actual_sha != expected_sha:
        raise RuntimeError(f"{path.name}: expected SHA-256 {expected_sha}, got {actual_sha}")


def locate_bundle(extract_root: Path, bundle_name: str) -> Path:
    matches = [path for path in extract_root.rglob(bundle_name) if path.is_dir()]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one {bundle_name} in the source archive, found {len(matches)}"
        )
    return matches[0]


def validate_bundle(bundle: Path) -> dict[str, str]:
    info_path = bundle / "Contents" / "Info.plist"
    if not info_path.is_file():
        raise RuntimeError(f"Missing Info.plist: {bundle}")
    with info_path.open("rb") as handle:
        info = plistlib.load(handle)
    executable_name = info.get("CFBundleExecutable")
    if not executable_name:
        raise RuntimeError(f"Missing CFBundleExecutable: {bundle}")
    executable = bundle / "Contents" / "MacOS" / executable_name
    if not executable.is_file():
        raise RuntimeError(f"Missing bundle executable: {executable}")

    run("/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", str(bundle))
    architectures = run("/usr/bin/lipo", "-archs", str(executable), capture=True).strip()
    if "arm64" not in architectures.split():
        raise RuntimeError(f"{bundle.name} does not contain arm64 code: {architectures}")
    return {
        "bundleIdentifier": str(info.get("CFBundleIdentifier", "")),
        "bundleVersion": str(info.get("CFBundleShortVersionString", "")),
        "architectures": architectures,
    }


def component_plist(payload: Path, destination: Path) -> None:
    run("/usr/bin/pkgbuild", "--analyze", "--root", str(payload), str(destination))
    with destination.open("rb") as handle:
        components = plistlib.load(handle)
    if not components:
        raise RuntimeError(f"pkgbuild did not identify a bundle under {payload}")
    for component in components:
        component["BundleIsRelocatable"] = False
        component["BundleIsVersionChecked"] = False
        component["BundleHasStrictIdentifier"] = True
        component["BundleOverwriteAction"] = "upgrade"
    with destination.open("wb") as handle:
        plistlib.dump(components, handle, sort_keys=False)


def package_identifier(plugin_id: str, format_key: str) -> str:
    stable_id = "".join(char if char.isalnum() else "-" for char in plugin_id.lower())
    return f"com.teamimpulseimpact.plugins.{stable_id}.{format_key}"


def package_filename(asset: dict[str, Any]) -> str:
    source_name = Path(urllib.parse.urlparse(asset["url"]).path).name
    if not source_name.lower().endswith(".zip"):
        raise RuntimeError(f"Expected a ZIP source URL, got {asset['url']}")
    return source_name[:-4] + ".pkg"


def build_package(
    *,
    plugin: dict[str, Any],
    format_key: str,
    asset: dict[str, Any],
    cache: Path | None,
    output: Path,
    work_root: Path,
    release_tag: str,
    repository: str,
    application_sign_identity: str | None,
    sign_identity: str | None,
    notary_profile: str | None,
) -> dict[str, Any]:
    bundle_name = str(asset["bundleName"])
    source_name = Path(urllib.parse.urlparse(asset["url"]).path).name
    expected_extension = ".vst3" if format_key == "vst3" else ".component"
    if not bundle_name.endswith(expected_extension):
        raise RuntimeError(f"{plugin['id']} {format_key}: invalid bundleName {bundle_name}")

    package_work = work_root / f"{plugin['id']}-{format_key}"
    package_work.mkdir()
    archive = find_cached_asset(cache, source_name, str(asset["sha256"]).lower())
    if archive is None:
        archive = package_work / source_name
        download(str(asset["url"]), archive)
    validate_source_archive(archive, asset)

    extract_root = package_work / "source"
    extract_root.mkdir()
    run("/usr/bin/ditto", "-x", "-k", str(archive), str(extract_root))
    source_bundle = locate_bundle(extract_root, bundle_name)
    source_details = validate_bundle(source_bundle)

    payload = package_work / "payload"
    install_subpath = (
        Path("Library/Audio/Plug-Ins/VST3")
        if format_key == "vst3"
        else Path("Library/Audio/Plug-Ins/Components")
    )
    destination_bundle = payload / install_subpath / bundle_name
    destination_bundle.parent.mkdir(parents=True)
    run("/usr/bin/ditto", str(source_bundle), str(destination_bundle))
    run("/usr/bin/xattr", "-cr", str(destination_bundle))
    if application_sign_identity:
        run(
            "/usr/bin/codesign",
            "--force",
            "--deep",
            "--options",
            "runtime",
            "--timestamp",
            "--sign",
            application_sign_identity,
            str(destination_bundle),
        )
    validate_bundle(destination_bundle)

    components = package_work / "components.plist"
    component_plist(payload, components)
    filename = package_filename(asset)
    package_path = output / filename
    identifier = package_identifier(str(plugin["id"]), format_key)
    command = [
        "/usr/bin/pkgbuild",
        "--root",
        str(payload),
        "--component-plist",
        str(components),
        "--identifier",
        identifier,
        "--version",
        str(asset["version"]),
        "--install-location",
        "/",
        "--ownership",
        "recommended",
    ]
    if sign_identity:
        command.extend(["--sign", sign_identity])
    command.append(str(package_path))
    run(*command)

    if notary_profile:
        run(
            "/usr/bin/xcrun",
            "notarytool",
            "submit",
            str(package_path),
            "--keychain-profile",
            notary_profile,
            "--wait",
        )
        run("/usr/bin/xcrun", "stapler", "staple", str(package_path))
        run("/usr/bin/xcrun", "stapler", "validate", str(package_path))

    payload_listing = run("/usr/sbin/pkgutil", "--payload-files", str(package_path), capture=True)
    expected_payload_path = f"{install_subpath}/{bundle_name}"
    if expected_payload_path not in payload_listing:
        raise RuntimeError(f"{filename}: expected payload path not found: {expected_payload_path}")

    expanded = package_work / "expanded"
    run("/usr/sbin/pkgutil", "--expand-full", str(package_path), str(expanded))
    expanded_bundles = [path for path in expanded.rglob(bundle_name) if path.is_dir()]
    if len(expanded_bundles) != 1:
        raise RuntimeError(f"{filename}: expanded payload contains {len(expanded_bundles)} bundles")
    validate_bundle(expanded_bundles[0])

    if sign_identity:
        run("/usr/sbin/pkgutil", "--check-signature", str(package_path))
        run("/usr/sbin/spctl", "-a", "-vv", "-t", "install", str(package_path))

    return {
        "pluginId": plugin["id"],
        "pluginName": plugin["name"],
        "displayVersion": plugin.get("version"),
        "format": "VST3" if format_key == "vst3" else "AU",
        "formatKey": format_key,
        "version": asset["version"],
        "bundleName": bundle_name,
        "bundleIdentifier": source_details["bundleIdentifier"],
        "bundleVersion": source_details["bundleVersion"],
        "architectures": source_details["architectures"],
        "identifier": identifier,
        "kind": "installer",
        "architecture": "arm64",
        "filename": filename,
        "url": f"https://github.com/{repository}/releases/download/{release_tag}/{filename}",
        "sha256": sha256(package_path),
        "sizeBytes": package_path.stat().st_size,
        "source": {
            "url": asset["url"],
            "sha256": asset["sha256"],
            "sizeBytes": asset["sizeBytes"],
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--archive-cache", type=Path)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--repository", default="TaebeumKim/TB_Audio-Plugins")
    parser.add_argument("--plugin", action="append", dest="plugin_ids")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        raise RuntimeError(f"Output directory must be empty: {args.output}")
    args.output.mkdir(parents=True, exist_ok=True)
    with args.catalog.open(encoding="utf-8") as handle:
        catalog = json.load(handle)

    application_sign_identity = os.environ.get("APPLE_APPLICATION_SIGN_IDENTITY") or None
    sign_identity = os.environ.get("APPLE_INSTALLER_SIGN_IDENTITY") or None
    notary_profile = os.environ.get("APPLE_NOTARY_PROFILE") or None
    if notary_profile and (not sign_identity or not application_sign_identity):
        raise RuntimeError(
            "APPLE_NOTARY_PROFILE requires APPLE_APPLICATION_SIGN_IDENTITY and "
            "APPLE_INSTALLER_SIGN_IDENTITY"
        )

    packages: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="tbhub-macos-pkg-") as temporary:
        work_root = Path(temporary)
        for plugin in catalog.get("plugins", []):
            if args.plugin_ids and plugin["id"] not in args.plugin_ids:
                continue
            formats = plugin.get("macos", {}).get("formats", {})
            for format_key in ("vst3", "au"):
                asset = formats.get(format_key)
                if not asset:
                    continue
                print(f"Building {plugin['name']} {format_key.upper()}…", flush=True)
                packages.append(
                    build_package(
                        plugin=plugin,
                        format_key=format_key,
                        asset=asset,
                        cache=args.archive_cache,
                        output=args.output,
                        work_root=work_root,
                        release_tag=args.release_tag,
                        repository=args.repository,
                        application_sign_identity=application_sign_identity,
                        sign_identity=sign_identity,
                        notary_profile=notary_profile,
                    )
                )

    manifest = {
        "schemaVersion": 1,
        "releaseTag": args.release_tag,
        "repository": args.repository,
        "generatedAt": datetime.now().astimezone().isoformat(timespec="seconds"),
        "sourceCatalogUpdatedAt": catalog.get("updatedAt"),
        "payloadSigned": bool(application_sign_identity),
        "installerSigned": bool(sign_identity),
        "signed": bool(application_sign_identity and sign_identity),
        "notarized": bool(notary_profile),
        "packageCount": len(packages),
        "packages": packages,
    }
    manifest_path = args.output / "macos-installers-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    checksums = [f"{item['sha256']}  {item['filename']}" for item in packages]
    checksums.append(f"{sha256(manifest_path)}  {manifest_path.name}")
    (args.output / "SHA256SUMS.txt").write_text("\n".join(checksums) + "\n", encoding="utf-8")

    signing_note = (
        "Packages are signed and notarized for normal Gatekeeper installation."
        if notary_profile
        else "Packages are unsigned because no Developer ID Installer identity was available."
    )
    notes = (
        "# TB Audio macOS plug-in installers\n\n"
        "Per-format Apple Silicon PKG installers generated from the exact production ZIP assets "
        "referenced by `catalog.json`. Existing TB Hub ZIP assets remain unchanged.\n\n"
        f"- Packages: {len(packages)}\n"
        "- Architecture: arm64\n"
        f"- Signing: {signing_note}\n"
    )
    (args.output / "RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
    print(f"Built {len(packages)} packages in {args.output}")


if __name__ == "__main__":
    main()
