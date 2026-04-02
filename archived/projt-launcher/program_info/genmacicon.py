#!/usr/bin/env python3

import os
import sys
import json
import xml.etree.ElementTree as ET
from copy import deepcopy

INPUT = sys.argv[1] if len(sys.argv) > 1 else "org.projecttick.ProjTLauncher.svg"
OUT_DIR = ".icon/assets"

os.makedirs(OUT_DIR, exist_ok=True)

tree = ET.parse(INPUT)
root = tree.getroot()

# SVG namespace fix (çünkü XML eğlenceli)
ns = {"svg": "http://www.w3.org/2000/svg"}

def strip_ns(tag):
    return tag.split("}")[-1]

layers = []

# 1. group-based extraction
groups = [el for el in root if strip_ns(el.tag) == "g"]

if not groups:
    # fallback: top-level shapes
    groups = [el for el in root if strip_ns(el.tag) != "defs"]

for idx, g in enumerate(groups):
    name = g.attrib.get("id", f"layer{idx}")

    new_svg = ET.Element(root.tag, root.attrib)
    new_svg.append(deepcopy(g))

    out_path = os.path.join(OUT_DIR, f"{name}.svg")
    ET.ElementTree(new_svg).write(out_path)

    layers.append({
        "image-name": f"assets/{name}.svg",
        "name": name,
        "position": {
            "scale": 1.0,
            "translation-in-points": [0, 0]
        }
    })

# JSON oluştur
icon_json = {
    "color-space-for-untagged-svg-colors": "display-p3",
    "groups": [
        {
            "layers": layers,
            "shadow": {
                "kind": "neutral",
                "opacity": 0.5
            },
            "translucency": {
                "enabled": False,
                "value": 0.5
            }
        }
    ],
    "supported-platforms": {
        "squares": ["macOS"]
    }
}

with open(".icon/icon.json", "w") as f:
    json.dump(icon_json, f, indent=2)

print(f"{len(layers)} layer extracted -> .icon/assets/")
