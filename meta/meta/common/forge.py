from os.path import join, dirname

from ..model import GradleSpecifier, make_launcher_library

BASE_DIR = "forge"

JARS_DIR = join(BASE_DIR, "jars")
INSTALLER_INFO_DIR = join(BASE_DIR, "installer_info")
INSTALLER_MANIFEST_DIR = join(BASE_DIR, "installer_manifests")
VERSION_MANIFEST_DIR = join(BASE_DIR, "version_manifests")
FILE_MANIFEST_DIR = join(BASE_DIR, "files_manifests")
DERIVED_INDEX_FILE = join(BASE_DIR, "derived_index.json")
LEGACYINFO_FILE = join(BASE_DIR, "legacyinfo.json")

FORGE_COMPONENT = "net.minecraftforge"

FORGEWRAPPER_LIBRARY = make_launcher_library(
    GradleSpecifier("io.github.zekerzhayard", "ForgeWrapper", "projt-2026-04-04"),
    "6325aed622c501c30ca0a377a7c1ea2ebfb68a1a",
    29732,
)
BAD_VERSIONS = ["1.12.2-14.23.5.2851"]
