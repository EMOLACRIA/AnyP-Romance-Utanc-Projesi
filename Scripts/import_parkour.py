"""Import only the ParkourPack into /Game/ParkourPack and write each mesh's bounds to
Saved/parkour_bounds.json (used when laying out the test route).

Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import json
import os
import sys

import unreal

PACK_ROOT = r"C:\Users\emo\Desktop\asset pack"
sys.path.insert(0, PACK_ROOT)
import import_to_unreal  # the pack's own importer; importing it does not run it

import_to_unreal.import_pack("ParkourPack")

bounds = {}
for path in unreal.EditorAssetLibrary.list_assets("/Game/ParkourPack", recursive=True, include_folder=False):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        bounds[asset.get_name()] = {
            "min": [round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
            "max": [round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)],
        }

out = os.path.join(unreal.Paths.project_saved_dir(), "parkour_bounds.json")
with open(out, "w") as f:
    json.dump(bounds, f, indent=1, sort_keys=True)
unreal.log("ParkourPack: %d meshes, bounds written to %s" % (len(bounds), out))
