import json
import os

from meta.common import upstream_path, ensure_upstream_dir
from meta.common.modloadermp import VERSIONS_FILE, BASE_DIR

UPSTREAM_DIR = upstream_path()
ensure_upstream_dir(BASE_DIR)

# ModLoaderMP is a legacy/archived project (companion to Risugami's ModLoader for SMP).
# No active upstream API exists; versions are curated from known historical releases.
KNOWN_VERSIONS = {
    "1.2.5": {"mc_version": "1.2.5", "label": "ModLoaderMP 1.2.5", "requires_modloader": "1.2.5"},
    "1.2.4": {"mc_version": "1.2.4", "label": "ModLoaderMP 1.2.4", "requires_modloader": "1.2.4"},
    "1.2.3": {"mc_version": "1.2.3", "label": "ModLoaderMP 1.2.3", "requires_modloader": "1.2.3"},
    "1.1": {"mc_version": "1.1", "label": "ModLoaderMP 1.1", "requires_modloader": "1.1"},
    "1.0": {"mc_version": "1.0", "label": "ModLoaderMP 1.0", "requires_modloader": "1.0"},
    "b1.8.1": {"mc_version": "b1.8.1", "label": "ModLoaderMP b1.8.1", "requires_modloader": "b1.8.1"},
    "b1.7.3": {"mc_version": "b1.7.3", "label": "ModLoaderMP b1.7.3", "requires_modloader": "b1.7.3"},
    "b1.6.6": {"mc_version": "b1.6.6", "label": "ModLoaderMP b1.6.6", "requires_modloader": "b1.6.6"},
}


def main():
    out_path = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)

    existing = {}
    if os.path.exists(out_path):
        try:
            with open(out_path, "r") as f:
                existing = json.load(f)
        except Exception:
            pass

    for key, data in KNOWN_VERSIONS.items():
        if key not in existing:
            existing[key] = data
        else:
            for field, value in data.items():
                existing[key].setdefault(field, value)

    with open(out_path, "w") as f:
        json.dump(existing, f, indent=4)

    print(f"Wrote {len(existing)} ModLoaderMP entries to {out_path}")


if __name__ == "__main__":
    main()
