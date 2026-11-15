"""Import the three date-venue packs into /Game/<Pack> (the ParkourPack has its own script).

Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import sys

import unreal

sys.path.insert(0, r"C:\Users\emo\Desktop\asset pack")
import import_to_unreal  # the packs' own importer; importing it does not run it

total = sum(import_to_unreal.import_pack(p) for p in ("CafeAssetPack", "HarborCafePack", "MountainCafePack"))
unreal.log("Cafe packs imported: %d static meshes" % total)
