import os
import json

from meta.common import ensure_component_dir, launcher_path, upstream_path
from meta.common.modloadermp import MODLOADERMP_COMPONENT, VERSIONS_FILE
from meta.common.mojang import MINECRAFT_COMPONENT
from meta.common.risugami import RISUGAMI_COMPONENT
from meta.model import MetaPackage, MetaVersion, Library, MojangLibraryDownloads, MojangArtifact, Dependency

LAUNCHER_DIR = launcher_path()
UPSTREAM_DIR = upstream_path()

ensure_component_dir(MODLOADERMP_COMPONENT)


def main():
    src = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)
    if not os.path.exists(src):
        print(f"Missing upstream file: {src}")
        return

    with open(src, "r", encoding="utf-8") as f:
        entries = json.load(f)

    if not entries:
        print("No ModLoaderMP entries found, writing stub package")
        package = MetaPackage(
            uid=MODLOADERMP_COMPONENT,
            name="ModLoaderMP",
            description="ModLoaderMP - multiplayer companion to Risugami's ModLoader",
        )
        package.write(os.path.join(LAUNCHER_DIR, MODLOADERMP_COMPONENT, "package.json"))
        return

    all_versions = []

    for key, data in entries.items():
        mc_version = data.get("mc_version")
        label = data.get("label", f"ModLoaderMP {key}")
        requires_modloader = data.get("requires_modloader")

        v = MetaVersion(
            name=label,
            uid=MODLOADERMP_COMPONENT,
            version=key,
            type="release",
            order=11,
        )

        # Dependencies: Minecraft + Risugami ModLoader
        deps = []
        if mc_version:
            deps.append(Dependency(uid=MINECRAFT_COMPONENT, equals=mc_version))
        if requires_modloader:
            deps.append(Dependency(uid=RISUGAMI_COMPONENT, equals=requires_modloader))
        if deps:
            v.requires = deps

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

        v.write(os.path.join(LAUNCHER_DIR, MODLOADERMP_COMPONENT, f"{v.version}.json"))
        all_versions.append(v.version)

    package = MetaPackage(
        uid=MODLOADERMP_COMPONENT,
        name="ModLoaderMP",
        description="ModLoaderMP - multiplayer companion to Risugami's ModLoader",
        recommended=all_versions[:3],
    )
    package.write(os.path.join(LAUNCHER_DIR, MODLOADERMP_COMPONENT, "package.json"))

    print(f"Generated {len(all_versions)} ModLoaderMP versions")


if __name__ == "__main__":
    main()
