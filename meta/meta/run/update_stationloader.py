import json
import os

from meta.common import upstream_path, ensure_upstream_dir, default_session
from meta.common.stationloader import VERSIONS_FILE, BASE_DIR

UPSTREAM_DIR = upstream_path()
ensure_upstream_dir(BASE_DIR)

sess = default_session()

# StationAPI/StationLoader GitHub releases
GITHUB_API_URL = "https://api.github.com/repos/modificationstation/StationAPI/releases"


def main():
    out_path = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)

    # Load existing entries to preserve manually-added data
    existing = {}
    if os.path.exists(out_path):
        try:
            with open(out_path, "r") as f:
                existing = json.load(f)
        except Exception:
            pass

    # Fetch releases from GitHub
    print(f"Fetching StationAPI releases from {GITHUB_API_URL}")
    try:
        r = sess.get(GITHUB_API_URL, timeout=15)
        r.raise_for_status()
        releases = r.json()
    except Exception as e:
        print(f"Error fetching GitHub releases: {e}")
        # Write whatever we have
        with open(out_path, "w") as f:
            json.dump(existing, f, indent=4)
        return

    for release in releases:
        tag = release.get("tag_name", "")
        if not tag:
            continue

        key = tag.lstrip("v")
        prerelease = release.get("prerelease", False)
        published = release.get("published_at")

        # Find the main jar asset
        jar_url = None
        jar_size = None
        jar_name = None
        for asset in release.get("assets", []):
            name = asset.get("name", "")
            if name.endswith(".jar"):
                jar_url = asset.get("browser_download_url")
                jar_size = asset.get("size")
                jar_name = name
                break

        data = {
            "mc_version": "b1.7.3",
            "label": f"StationAPI {key}",
            "tag": tag,
            "prerelease": prerelease,
        }
        if published:
            data["date"] = published
        if jar_url:
            data["url"] = jar_url
        if jar_size is not None:
            data["size"] = jar_size
        if jar_name:
            data["filename"] = jar_name

        if key not in existing:
            existing[key] = data
        else:
            for field, value in data.items():
                existing[key].setdefault(field, value)

    with open(out_path, "w") as f:
        json.dump(existing, f, indent=4)

    print(f"Wrote {len(existing)} StationLoader entries to {out_path}")


if __name__ == "__main__":
    main()
