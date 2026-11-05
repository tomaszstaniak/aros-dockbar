#!/usr/bin/env python3
"""Generate the arospkg application-folder description from packaging metadata."""
import argparse
import json
import tomllib
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("target", choices=("x86_64-v1", "x86_64-abiv11", "aarch64-raspi-v1"))
p.add_argument("version")
p.add_argument("output", type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parent.parent
m = tomllib.loads((root / "packaging" / ("application-" + a.target + ".toml")).read_text())
d = {"schema": 1, "id": m["id"], "version": a.version, "revision": m["revision"],
     "targets": [{"os": "aros", "arch": m["arch"], "abi": m["abi"]}],
     "requires_system": m["requires_system"],
     "launch": {**m["launch"], "working_directory": "."}}
a.output.write_text(json.dumps(d, indent=2) + "\n", encoding="utf-8")
