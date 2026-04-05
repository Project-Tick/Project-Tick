import os
import json
from datetime import datetime, timezone

from meta.common import ensure_component_dir, launcher_path, upstream_path
from meta.common.risugami import RISUGAMI_COMPONENT, VERSIONS_FILE
from meta.common.mojang import MINECRAFT_COMPONENT
from meta.model import MetaPackage, MetaVersion, Library, MojangLibraryDownloads, MojangArtifact, Dependency

LAUNCHER_DIR = launcher_path()
UPSTREAM_DIR = upstream_path()

ensure_component_dir(RISUGAMI_COMPONENT)


def main():
    src = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)
    if not os.path.exists(src):
        print(f"Missing upstream file: {src}")
        return

    with open(src, "r", encoding="utf-8") as f:
        entries = json.load(f)

    if not entries:
        print("No Risugami ModLoader entries found, writing stub package")
        package = MetaPackage(
            uid=RISUGAMI_COMPONENT,
            name="Risugami ModLoader",
            description="Risugami's ModLoader for classic/legacy Minecraft",
        )
        package.write(os.path.join(LAUNCHER_DIR, RISUGAMI_COMPONENT, "package.json"))
        return

    all_versions = []

    for key, data in entries.items():
        mc_version = data.get("mc_version")
        label = data.get("label", f"ModLoader {key}")

        v = MetaVersion(
            name=label,
            uid=RISUGAMI_COMPONENT,
            version=key,
            type="release",
            order=10,
        )

        if mc_version:
            v.requires = [Dependency(uid=MINECRAFT_COMPONENT, equals=mc_version)]

        # Parse release date, fallback to epoch so index.py doesn't reject None
        date = data.get("date")
        if date:
            try:
                v.release_time = datetime.fromisoformat(date.replace("Z", "+00:00"))
            except Exception:
                v.release_time = datetime(1970, 1, 1, tzinfo=timezone.utc)
        else:
            v.release_time = datetime(1970, 1, 1, tzinfo=timezone.utc)

        # Attach download artifact if available
        url = data.get("url") or data.get("download_url")
        if url:
            artifact_kwargs = {"url": url}
            if data.get("size") is not None:
                artifact_kwargs["size"] = data["size"]
            if data.get("sha256") is not None:
                artifact_kwargs["sha256"] = data["sha256"]
            artifact = MojangArtifact(**artifact_kwargs)
            lib = Library(downloads=MojangLibraryDownloads(artifact=artifact))
            v.jar_mods = [lib]

        v.write(os.path.join(LAUNCHER_DIR, RISUGAMI_COMPONENT, f"{v.version}.json"))
        all_versions.append(v.version)

    package = MetaPackage(
        uid=RISUGAMI_COMPONENT,
        name="Risugami ModLoader",
        description="Risugami's ModLoader for classic/legacy Minecraft",
        recommended=all_versions[:3],
    )
    package.write(os.path.join(LAUNCHER_DIR, RISUGAMI_COMPONENT, "package.json"))

    print(f"Generated {len(all_versions)} Risugami ModLoader versions")


if __name__ == "__main__":
    main()
