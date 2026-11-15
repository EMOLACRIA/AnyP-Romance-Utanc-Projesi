"""Import the CityPack (facade blocks, ground floors, roof edges, street tiles and furniture) into /Game/CityPack.

Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi

The blocks are drawn thousands of times in the background city, so Nanite is switched back on for them
(the packs' importer turns it off for every mesh; these have a single opaque material, so it is safe).
It also logs each block's collision shape count next to Building_Block_4x4's (one convex hull each).
"""
import sys

import unreal

sys.path.insert(0, r"C:\Users\emo\Desktop\asset pack")
import import_to_unreal  # the packs' own importer; importing it does not run it

EAL = unreal.EditorAssetLibrary
total = import_to_unreal.import_pack("CityPack")
unreal.log("CityPack imported: %d static meshes" % total)

tools = (unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
         or unreal.get_default_object(unreal.StaticMeshEditorSubsystem))


def collision(mesh):
    text = "simple=%d convex=%d" % (tools.get_simple_collision_count(mesh), tools.get_convex_collision_count(mesh))
    try:
        agg = mesh.get_editor_property("body_setup").get_editor_property("agg_geom")
        for b in agg.get_editor_property("box_elems"):
            c = b.get_editor_property("center")
            text += " box centre=(%.0f,%.0f,%.0f) size=(%.0f,%.0f,%.0f)" % (
                c.x, c.y, c.z, b.get_editor_property("x"), b.get_editor_property("y"), b.get_editor_property("z"))
    except Exception:               # the counts above still say whether a hull came through
        pass
    return text                     # convex hull points are not readable from Python; the movement tests cover them


unreal.log("COLLISION Building_Block_4x4 %s" % collision(EAL.load_asset("/Game/ParkourPack/SM_Building_Block_4x4")))
for path in sorted(EAL.list_assets("/Game/CityPack", recursive=True, include_folder=False)):
    mesh = EAL.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    name = mesh.get_name()
    if name.startswith(("SM_Block_", "SM_Ground_", "SM_Cornice_")):
        settings = tools.get_nanite_settings(mesh)
        settings.set_editor_property("enabled", True)
        tools.set_nanite_settings(mesh, settings, True)
    if name.startswith(("SM_Block_", "SM_Ground_")):
        unreal.log("COLLISION %s %s" % (name[3:], collision(mesh)))
EAL.save_directory("/Game/CityPack", only_if_is_dirty=False, recursive=True)
unreal.log("CityPack ready")
