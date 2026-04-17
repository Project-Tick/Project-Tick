#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Project Tick
"""
Validate a components.json file against the Project Tick release manifest schema.

Usage:
    python3 validate-components-json.py <path-to-components.json>
    python3 validate-components-json.py --generate <release_tag> <meshmc_ver> [neozip_ver]
"""

import json
import re
import sys

SEMVER_RE = re.compile(r'^\d+\.\d+\.\d+(\.\d+)?$')
TAG_RE = re.compile(r'^v?\d{12}$|^v?\d+\.\d+\.\d+')


def validate(data: dict) -> list[str]:
    errors = []

    if data.get('schema_version') != 1:
        errors.append('schema_version must be 1')

    tag = data.get('release_tag', '')
    if not isinstance(tag, str) or not tag:
        errors.append('release_tag is required and must be a non-empty string')

    rd = data.get('release_date', '')
    if not re.match(r'^\d{4}-\d{2}-\d{2}$', str(rd)):
        errors.append(f'release_date must be YYYY-MM-DD, got: {rd}')

    components = data.get('components')
    if not isinstance(components, dict) or not components:
        errors.append('components must be a non-empty object')
        return errors

    for cid, cdata in components.items():
        if not isinstance(cdata, dict):
            errors.append(f'components.{cid} must be an object')
            continue
        ver = cdata.get('version', '')
        if not SEMVER_RE.match(str(ver)):
            errors.append(
                f'components.{cid}.version must match X.Y.Z, got: {ver}'
            )

    return errors


def generate(release_tag: str, meshmc_ver: str, neozip_ver: str | None = None) -> dict:
    from datetime import date  # noqa: C0415

    manifest = {
        'schema_version': 1,
        'release_tag': release_tag,
        'release_date': date.today().isoformat(),
        'components': {
            'meshmc': {'version': meshmc_ver},
        },
    }
    if neozip_ver:
        manifest['components']['neozip'] = {'version': neozip_ver}
    return manifest


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 1

    if sys.argv[1] == '--generate':
        if len(sys.argv) < 4:
            print('Usage: --generate <release_tag> <meshmc_ver> [neozip_ver]',
                  file=sys.stderr)
            return 1
        tag = sys.argv[2]
        meshmc = sys.argv[3]
        neozip = sys.argv[4] if len(sys.argv) > 4 else None
        manifest = generate(tag, meshmc, neozip)
        errs = validate(manifest)
        if errs:
            for e in errs:
                print(f'ERROR: {e}', file=sys.stderr)
            return 1
        print(json.dumps(manifest, indent=2))
        return 0

    path = sys.argv[1]
    try:
        with open(path) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 1

    errs = validate(data)
    if errs:
        for e in errs:
            print(f'FAIL: {e}', file=sys.stderr)
        return 1

    print(f'OK: {path} is valid (tag={data["release_tag"]}, '
          f'components={list(data["components"].keys())})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
