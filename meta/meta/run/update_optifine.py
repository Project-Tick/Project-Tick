import json
import os
import re
import hashlib
import concurrent.futures
from urllib.parse import urljoin, urlparse, parse_qs

from meta.common import upstream_path, ensure_upstream_dir, default_session
from meta.common.optifine import VERSIONS_FILE, BASE_DIR

UPSTREAM_DIR = upstream_path()
ensure_upstream_dir(BASE_DIR)

sess = default_session()

# Configurable via environment
TIMEOUT = float(os.environ.get("OPTIFINE_TIMEOUT", "10"))
HASH_TIMEOUT = float(os.environ.get("OPTIFINE_HASH_TIMEOUT", "120"))
CONCURRENCY = max(1, int(os.environ.get("OPTIFINE_CONCURRENCY", "8")))
COMPUTE_HASH = os.environ.get("OPTIFINE_COMPUTE_HASH", "1").lower() in ("1", "true", "yes")


def _resolve_href(href: str):
    """Return (filename, resolved_href) from an OptiFine download link."""
    parsed = urlparse(href)
    q = parse_qs(parsed.query)

    f = q.get("f")
    if f:
        return f[0], href

    inner = q.get("url")
    if inner:
        inner_parsed = urlparse(inner[0])
        inner_f = parse_qs(inner_parsed.query).get("f")
        if inner_f:
            return inner_f[0], inner[0]

    return os.path.basename(parsed.path), href


def _strip_ad_wrapper(filename: str) -> str:
    """Remove trailing ad/adload/adloadx fragments from a filename."""
    if not filename:
        return filename
    root, ext = os.path.splitext(filename)
    root = re.sub(r"[_-]ad[a-z0-9_-]*$", "", root, flags=re.IGNORECASE)
    return root + ext


def _clean_key(filename: str) -> str:
    """Normalize filename to version key: strip OptiFine_ prefix and .jar suffix."""
    key = re.sub(r"^OptiFine[_-]", "", filename, flags=re.IGNORECASE)
    key = re.sub(r"\.jar$", "", key, flags=re.IGNORECASE)
    key = re.sub(r"[_-]ad[a-z0-9_-]*$", "", key, flags=re.IGNORECASE)
    return key


def _score_entry(entry: dict) -> int:
    """Score an entry to pick the best when duplicates exist."""
    url = (entry.get("download_page") or "").lower()
    s = 0
    if any(k in url for k in ("optifine.net/adloadx", "optifine.net/adload", "optifine.net/download")):
        s += 10
    if url.endswith(".jar") or (entry.get("filename") or "").lower().endswith(".jar"):
        s += 5
    if "preview" in (entry.get("filename") or "").lower():
        s -= 2
    return s


def _scrape_downloads(html: str, base_url: str) -> dict:
    """Parse OptiFine downloads page and return {key: entry_dict}."""
    versions = {}

    try:
        from bs4 import BeautifulSoup
        soup = BeautifulSoup(html, "html.parser")

        for a in soup.find_all("a", href=True):
            href = a["href"]
            href_l = href.lower()
            if "?f=" not in href_l and not href_l.endswith(".jar"):
                continue

            filename, resolved = _resolve_href(href)
            filename = _strip_ad_wrapper(filename)

            ver_text = None
            changelog = None
            date = None

            tr = a.find_parent("tr")
            if tr:
                tds = tr.find_all("td")
                if tds:
                    ver_text = tds[0].get_text(strip=True)
                ch = tr.find("a", href=lambda h: h and "changelog" in h)
                if ch:
                    changelog = ch.get("href")
                date_td = tr.find("td", class_=lambda c: c and "colDate" in c)
                if date_td:
                    date = date_td.get_text(strip=True)

            if not ver_text:
                ver_text = (a.string or "").strip() or filename

            key = _clean_key(filename)
            data = {
                "filename": filename,
                "download_page": urljoin(base_url, resolved),
                "label": ver_text,
                "changelog": changelog,
                "date": date,
            }

            existing = versions.get(key)
            if existing is None or _score_entry(data) > _score_entry(existing):
                versions[key] = data
    except ImportError:
        # Fallback: regex parse
        print("BeautifulSoup not available, falling back to regex parse")
        for match in re.finditer(r'href="([^"]*\?f=[^"\s]+)"', html, flags=re.IGNORECASE):
            href = match.group(1)
            filename, resolved = _resolve_href(href)
            filename = _strip_ad_wrapper(filename)
            key = _clean_key(filename)
            data = {
                "filename": filename,
                "download_page": urljoin(base_url, resolved),
                "label": filename,
            }
            existing = versions.get(key)
            if existing is None or _score_entry(data) > _score_entry(existing):
                versions[key] = data

    return versions


def _make_download_url(filename: str) -> str:
    """Build a stable OptiFine download URL from a filename.

    Uses the permanent https://optifine.net/download?f=FILENAME format
    instead of the adloadx/downloadx token URLs which expire.
    """
    return f"https://optifine.net/download?f={filename}"


def _resolve_and_hash(key: str, data: dict) -> dict:
    """Build stable download URL and optionally compute SHA256 for a single entry."""
    filename = data.get("filename")
    if not filename:
        return data

    # Use stable download URL instead of expiring token-based downloadx URLs
    download_url = _make_download_url(filename)
    data["resolved_url"] = download_url

    # Compute hash if enabled
    if COMPUTE_HASH:
        try:
            h = hashlib.sha256()
            size = 0
            r = sess.get(download_url, stream=True, timeout=HASH_TIMEOUT)
            r.raise_for_status()
            for chunk in r.iter_content(8192):
                if chunk:
                    h.update(chunk)
                    size += len(chunk)
            data["sha256"] = h.hexdigest()
            data["size"] = size
        except Exception as e:
            print(f"  Warning: hash failed for {key}: {e}")

    return data


def main():
    url = "https://optifine.net/downloads"
    print(f"Fetching OptiFine downloads page: {url}")

    try:
        r = sess.get(url, timeout=TIMEOUT)
        r.raise_for_status()
        html = r.text
    except Exception as e:
        print(f"Error fetching downloads page: {e}")
        return

    versions = _scrape_downloads(html, url)
    print(f"Scraped {len(versions)} OptiFine entries")

    if not versions:
        print("No versions found, aborting")
        return

    out_dir = os.path.join(UPSTREAM_DIR, BASE_DIR)
    os.makedirs(out_dir, exist_ok=True)

    # Resolve URLs and compute hashes in parallel
    def _process(item):
        key, data = item
        print(f"  Resolving {key}...")
        data = _resolve_and_hash(key, data)
        return key, data

    items = list(versions.items())
    if CONCURRENCY == 1:
        results = [_process(item) for item in items]
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY) as ex:
            results = list(ex.map(_process, items))

    # Write per-version files and build combined index
    versions = {}
    for key, data in results:
        versions[key] = data
        per_path = os.path.join(out_dir, f"{key}.json")
        with open(per_path, "w") as f:
            json.dump(data, f, indent=4)

    # Write combined index
    combined_path = os.path.join(UPSTREAM_DIR, VERSIONS_FILE)
    os.makedirs(os.path.dirname(combined_path) or ".", exist_ok=True)
    with open(combined_path, "w") as f:
        json.dump(versions, f, indent=4)

    resolved = sum(1 for v in versions.values() if v.get("resolved_url"))
    hashed = sum(1 for v in versions.values() if v.get("sha256"))
    print(f"Wrote {len(versions)} entries to {combined_path}")
    print(f"Resolved: {resolved}, Hashed: {hashed}")


if __name__ == "__main__":
    main()
