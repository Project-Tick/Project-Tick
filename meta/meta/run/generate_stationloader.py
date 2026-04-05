import os
import json
from datetime import datetime

from meta.common import ensure_component_dir, launcher_path, upstream_path
from meta.common.stationloader import STATIONLOADER_COMPONENT, VERSIONS_FILE
from meta.common.mojang import MINECRAFT_COMPONENT
from meta.model import MetaPackage, MetaVersion, Library, MojangLibraryDownloads, MojangArtifact, Dependency

LAUNCHER_DIR = launcher_path()
UPSTREAM_DIR = upstream_path()

ensure_component_dir(STATIONLOADER_COMPONENT)


def main():
    src = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)
    if not os.path.exists(src):
        print(f"Missing upstream file: {src}")
        return

    with open(src, "r", encoding="utf-8") as f:
        entries = json.load(f)

    if not entries:
        print("No StationLoader entries found, writing stub package")
        package = MetaPackage(
            uid=STATIONLOADER_COMPONENT,
            name="StationAPI",
            description="StationAPI mod loader for Minecraft b1.7.3",
        )
        package.write(os.path.join(LAUNCHER_DIR, STATIONLOADER_COMPONENT, "package.json"))
        return

    all_versions = []

    for key, data in entries.items():
        mc_version = data.get("mc_version", "b1.7.3")
        label = data.get("label", f"StationAPI {key}")
        prerelease = data.get("prerelease", False)

        v = MetaVersion(
            name=label,
            uid=STATIONLOADER_COMPONENT,
            version=key,
            type="snapshot" if prerelease else "release",
            order=10,
        )

        v.requires = [Dependency(uid=MINECRAFT_COMPONENT, equals=mc_version)]

        # Parse release date (ISO 8601 from GitHub)
        date = data.get("date")
        if date:
            try:
                v.release_time = datetime.fromisoformat(date.replace("Z", "+00:00"))
            except Exception:
                pass

        # Attach download artifact if available
        url = data.get("url")
        if url:
            artifact_kwargs = {"url": url}
            if data.get("size") is not None:
                artifact_kwargs["size"] = data["size"]
            if data.get("sha256") is not None:
                artifact_kwargs["sha256"] = data["sha256"]
            artifact = MojangArtifact(**artifact_kwargs)
            lib = Library(downloads=MojangLibraryDownloads(artifact=artifact))
            v.jar_mods = [lib]

        v.write(os.path.join(LAUNCHER_DIR, STATIONLOADER_COMPONENT, f"{v.version}.json"))
        all_versions.append((v.version, v.release_time, prerelease))

    # Recommended: latest non-prerelease versions
    releases = [(ver, rt) for ver, rt, pre in all_versions if not pre]
    releases.sort(key=lambda x: (x[1] or datetime.min), reverse=True)
    recommended = [r[0] for r in releases[:3]]

    package = MetaPackage(
        uid=STATIONLOADER_COMPONENT,
        name="StationAPI",
        description="StationAPI mod loader for Minecraft b1.7.3",
        project_url="https://github.com/modificationstation/StationAPI",
        recommended=recommended,
    )
    package.write(os.path.join(LAUNCHER_DIR, STATIONLOADER_COMPONENT, "package.json"))

    print(f"Generated {len(all_versions)} StationLoader versions ({len(recommended)} recommended)")


if __name__ == "__main__":
    main()
