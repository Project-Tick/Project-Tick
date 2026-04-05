import os
import json
import re
from datetime import datetime

from meta.common import ensure_component_dir, launcher_path, upstream_path
from meta.common.optifine import OPTIFINE_COMPONENT, VERSIONS_FILE, BASE_DIR
from meta.common.mojang import MINECRAFT_COMPONENT
from meta.model import MetaPackage, MetaVersion, Library, MojangLibraryDownloads, MojangArtifact, Dependency

LAUNCHER_DIR = launcher_path()
UPSTREAM_DIR = upstream_path()

ensure_component_dir(OPTIFINE_COMPONENT)


def _parse_date(d: str):
    """Parse dates like DD.MM.YYYY from OptiFine site."""
    try:
        return datetime.strptime(d, "%d.%m.%Y")
    except Exception:
        return None


def _extract_mc_version(key: str) -> str:
    """Extract Minecraft version from an OptiFine version key.

    Examples:
        '1.21.4_HD_U_J3' -> '1.21.4'
        'preview_OptiFine_1.21.8_HD_U_J6_pre16' -> '1.21.8'
        '1.8.9_HD_U_M5' -> '1.8.9'
    """
    # Strip preview prefix
    clean = re.sub(r"^preview_(?:OptiFine_)?", "", key, flags=re.IGNORECASE)
    # Match the Minecraft version at the start (e.g. 1.21.4, 1.8.9, 1.7.10)
    m = re.match(r"(\d+\.\d+(?:\.\d+)?)", clean)
    return m.group(1) if m else None


def _is_preview(key: str, data: dict) -> bool:
    """Check if a version is a preview/pre-release."""
    filename = data.get("filename", "")
    return "preview" in key.lower() or "preview" in filename.lower() or "pre" in key.lower()


def _load_entries() -> dict:
    """Load upstream OptiFine version entries from per-version files or combined index."""
    comp_dir = os.path.join(UPSTREAM_DIR, BASE_DIR)
    entries = {}

    if os.path.isdir(comp_dir):
        for fn in os.listdir(comp_dir):
            if not fn.endswith(".json") or fn == "versions.json":
                continue
            # Skip the nested optifine/ subdir if it exists
            path = os.path.join(comp_dir, fn)
            if not os.path.isfile(path):
                continue
            try:
                with open(path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                key = os.path.splitext(fn)[0]
                entries[key] = data
            except Exception:
                print(f"Warning: failed to read {path}")

    # Fallback to combined index
    if not entries:
        src = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)
        if not os.path.exists(src):
            print(f"Missing upstream file: {src}")
            return {}
        with open(src, "r", encoding="utf-8") as f:
            entries = json.load(f)

    return entries


def main():
    entries = _load_entries()
    if not entries:
        print("No OptiFine entries found")
        return

    parsed_versions = []

    for key, data in entries.items():
        mc_version = _extract_mc_version(key)
        is_preview = _is_preview(key, data)

        v = MetaVersion(
            name="OptiFine",
            uid=OPTIFINE_COMPONENT,
            version=key,
            type="snapshot" if is_preview else "release",
            order=10,
        )

        # Add Minecraft version dependency if we could extract it
        if mc_version:
            v.requires = [Dependency(uid=MINECRAFT_COMPONENT, equals=mc_version)]

        # Use label as display name if available
        label = data.get("label")
        if label:
            v.name = label

        # Parse release date, fallback to epoch so index.py doesn't reject None
        date = data.get("date")
        if date:
            dt = _parse_date(date)
            v.release_time = dt if dt else datetime(1970, 1, 1)
        else:
            v.release_time = datetime(1970, 1, 1)

        # Build jar mod artifact — use stable download?f= URL
        filename = data.get("filename")
        if filename:
            resolved = f"https://optifine.net/download?f={filename}"
        else:
            resolved = data.get("resolved_url") or data.get("download_page")
        if resolved:
            artifact_kwargs = {"url": resolved}
            if data.get("size") is not None:
                artifact_kwargs["size"] = data["size"]
            if data.get("sha256") is not None:
                artifact_kwargs["sha256"] = data["sha256"]

            artifact = MojangArtifact(**artifact_kwargs)
            lib = Library(downloads=MojangLibraryDownloads(artifact=artifact))
            v.jar_mods = [lib]

        v.write(os.path.join(LAUNCHER_DIR, OPTIFINE_COMPONENT, f"{v.version}.json"))
        parsed_versions.append((v.version, v.release_time, is_preview))

    # Recommended: latest non-preview releases by date
    releases = [(ver, rt) for ver, rt, preview in parsed_versions if not preview]
    releases.sort(key=lambda x: (x[1] or datetime.min), reverse=True)
    recommended = [r[0] for r in releases[:3]]

    package = MetaPackage(
        uid=OPTIFINE_COMPONENT,
        name="OptiFine",
        description="OptiFine - Minecraft performance and graphics mod",
        project_url="https://optifine.net",
        recommended=recommended,
    )
    package.write(os.path.join(LAUNCHER_DIR, OPTIFINE_COMPONENT, "package.json"))

    print(f"Generated {len(parsed_versions)} OptiFine versions ({len(recommended)} recommended)")


if __name__ == "__main__":
    main()
