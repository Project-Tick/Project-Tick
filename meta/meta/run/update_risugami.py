import json
import os

from meta.common import upstream_path, ensure_upstream_dir
from meta.common.risugami import VERSIONS_FILE, BASE_DIR

UPSTREAM_DIR = upstream_path()
ensure_upstream_dir(BASE_DIR)

# Risugami's ModLoader is a legacy/archived project (last updated around MC 1.6.2).
# No active upstream API exists; versions are curated from known historical releases.
# To add a version, add an entry to this dict with the Minecraft version it targets.
KNOWN_VERSIONS = {
    "1.6.2": {"mc_version": "1.6.2", "label": "ModLoader 1.6.2"},
    "1.6.1": {"mc_version": "1.6.1", "label": "ModLoader 1.6.1"},
    "1.5.2": {"mc_version": "1.5.2", "label": "ModLoader 1.5.2"},
    "1.5.1": {"mc_version": "1.5.1", "label": "ModLoader 1.5.1"},
    "1.4.7": {"mc_version": "1.4.7", "label": "ModLoader 1.4.7"},
    "1.4.6": {"mc_version": "1.4.6", "label": "ModLoader 1.4.6"},
    "1.4.5": {"mc_version": "1.4.5", "label": "ModLoader 1.4.5"},
    "1.4.4": {"mc_version": "1.4.4", "label": "ModLoader 1.4.4"},
    "1.4.2": {"mc_version": "1.4.2", "label": "ModLoader 1.4.2"},
    "1.3.2": {"mc_version": "1.3.2", "label": "ModLoader 1.3.2"},
    "1.3.1": {"mc_version": "1.3.1", "label": "ModLoader 1.3.1"},
    "1.2.5": {"mc_version": "1.2.5", "label": "ModLoader 1.2.5"},
    "1.2.4": {"mc_version": "1.2.4", "label": "ModLoader 1.2.4"},
    "1.2.3": {"mc_version": "1.2.3", "label": "ModLoader 1.2.3"},
    "1.1": {"mc_version": "1.1", "label": "ModLoader 1.1"},
    "1.0": {"mc_version": "1.0", "label": "ModLoader 1.0"},
    "b1.8.1": {"mc_version": "b1.8.1", "label": "ModLoader b1.8.1"},
    "b1.7.3": {"mc_version": "b1.7.3", "label": "ModLoader b1.7.3"},
    "b1.6.6": {"mc_version": "b1.6.6", "label": "ModLoader b1.6.6"},
    "b1.5_01": {"mc_version": "b1.5_01", "label": "ModLoader b1.5_01"},
    "b1.4_01": {"mc_version": "b1.4_01", "label": "ModLoader b1.4_01"},
    "b1.3_01": {"mc_version": "b1.3_01", "label": "ModLoader b1.3_01"},
}


def main():
    # Merge with any existing entries to preserve manually-added data (sha256, urls, etc.)
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
            # Preserve existing fields, add any missing ones from known data
            for field, value in data.items():
                existing[key].setdefault(field, value)

    with open(out_path, "w") as f:
        json.dump(existing, f, indent=4)

    print(f"Wrote {len(existing)} Risugami ModLoader entries to {out_path}")


if __name__ == "__main__":
    main()
