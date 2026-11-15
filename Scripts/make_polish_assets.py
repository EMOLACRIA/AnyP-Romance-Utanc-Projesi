"""Create the project's own look-and-feel assets:

  /Game/Characters   Ari, Bea and Jules (the dressed models from the earlier Unreal project's Art folder)
  /Game/Romance      M_PaintTint + MI_Block_0..N (tinted facades), M_Burst, M_Rope, M_Street

Headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import os

import unreal

CHAR_SRC = r"C:\Program Files\Epic Games\UE_5.8\Projects\AnyRomance\Art\SelectedCharacters"
CHARS = "/Game/Characters"
LOOK = "/Game/Romance"

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty

# Facade tints, multiplied over the building block's painted texture. Index 0 leaves it as painted.
TINTS = [(1.00, 1.00, 1.00), (1.06, 0.84, 0.80), (0.80, 1.00, 0.92), (0.82, 0.92, 1.08),
         (1.08, 0.96, 0.72), (0.86, 0.86, 0.92), (1.02, 0.78, 0.66), (0.92, 0.88, 1.06)]


def fresh(name, folder, cls, factory):
    """Create the asset, or reuse it emptied if it exists (maps already point at it by path)."""
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        asset = EAL.load_asset(path)
        if isinstance(asset, unreal.Material):
            MEL.delete_all_material_expressions(asset)
        return asset
    return AT.create_asset(name, folder, cls, factory)


def node(material, cls, x, y, **props):
    n = MEL.create_material_expression(material, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def const(material, value, x, y):
    return node(material, unreal.MaterialExpressionConstant, x, y, r=value)


def colour(material, rgb, x, y):
    return node(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))


def painted(name, folder, texture, tinted=False):
    """Base colour straight from a hand-painted texture, matte, optionally multiplied by a Tint parameter."""
    m = fresh(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    # The background city draws its buildings as instances.
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0, parameter_name="Paint", texture=texture)
    if tinted:
        tint = node(m, unreal.MaterialExpressionVectorParameter, -700, 300, parameter_name="Tint",
                    default_value=unreal.LinearColor(1, 1, 1, 1))
        mul = node(m, unreal.MaterialExpressionMultiply, -350, 100)
        MEL.connect_material_expressions(tex, "RGB", mul, "A")
        MEL.connect_material_expressions(tint, "", mul, "B")
        MEL.connect_material_property(mul, "", MP.MP_BASE_COLOR)
    else:
        MEL.connect_material_property(tex, "RGB", MP.MP_BASE_COLOR)
    MEL.connect_material_property(const(m, 0.9, -350, 400), "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.1, -350, 500), "", MP.MP_SPECULAR)
    MEL.recompile_material(m)
    return m


def flat(name, rgb, roughness=1.0):
    m = fresh(name, LOOK, unreal.Material, unreal.MaterialFactoryNew())
    MEL.connect_material_property(colour(m, rgb, -400, 0), "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(const(m, roughness, -400, 200), "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 0.05, -400, 300), "", MP.MP_SPECULAR)
    MEL.recompile_material(m)
    return m


def burst():
    """Unlit glowing shell, brightest at its rim; the game drives Fade from 1 to 0."""
    m = fresh("M_Burst", LOOK, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    col = node(m, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Color",
               default_value=unreal.LinearColor(1.0, 0.45, 0.12, 1))
    glow = node(m, unreal.MaterialExpressionMultiply, -550, 0)
    MEL.connect_material_expressions(col, "", glow, "A")
    MEL.connect_material_expressions(const(m, 9.0, -900, 200), "", glow, "B")
    MEL.connect_material_property(glow, "", MP.MP_EMISSIVE_COLOR)
    fade = node(m, unreal.MaterialExpressionScalarParameter, -900, 400, parameter_name="Fade", default_value=1.0)
    rim = node(m, unreal.MaterialExpressionFresnel, -900, 550)
    body = node(m, unreal.MaterialExpressionAdd, -650, 500)
    MEL.connect_material_expressions(rim, "", body, "A")
    MEL.connect_material_expressions(const(m, 0.12, -900, 700), "", body, "B")
    alpha = node(m, unreal.MaterialExpressionMultiply, -400, 450)
    MEL.connect_material_expressions(fade, "", alpha, "A")
    MEL.connect_material_expressions(body, "", alpha, "B")
    MEL.connect_material_property(alpha, "", MP.MP_OPACITY)
    MEL.recompile_material(m)
    return m


def import_file(path, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    AT.import_asset_tasks([t])


# ---------------------------------------------------------------- look
block_paint = EAL.load_asset("/Game/ParkourPack/T_Building_Block_4x4")
master = painted("M_PaintTint", LOOK, block_paint, tinted=True)
for i, rgb in enumerate(TINTS):
    mi = fresh("MI_Block_%d" % i, LOOK, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1))
burst()
flat("M_Rope", (0.86, 0.78, 0.62), 0.8)
flat("M_Street", (0.055, 0.062, 0.075))
flat("M_Sea", (0.03, 0.22, 0.30), 0.25)
EAL.save_directory(LOOK, only_if_is_dirty=False, recursive=True)

# ---------------------------------------------------------------- characters
for name in ("Ari", "Bea", "Jules"):
    import_file(os.path.join(CHAR_SRC, "%s_BaseColor.png" % name), CHARS)
    import_file(os.path.join(CHAR_SRC, "SM_%s.fbx" % name), CHARS)
    tex = EAL.load_asset("%s/%s_BaseColor" % (CHARS, name))
    body = EAL.load_asset("%s/SM_%s" % (CHARS, name))
    mat = painted("M_%s" % name, CHARS, tex)
    for slot in range(len(body.get_editor_property("static_materials"))):
        body.set_material(slot, mat)
    box = body.get_bounding_box()
    unreal.log("CHARACTER %s min=(%.1f, %.1f, %.1f) max=(%.1f, %.1f, %.1f) slots=%d" % (
        name, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z,
        len(body.get_editor_property("static_materials"))))
EAL.save_directory(CHARS, only_if_is_dirty=False, recursive=True)
unreal.log("Polish assets done")
