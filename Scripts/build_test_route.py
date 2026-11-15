"""Build /Game/Maps/L_TestRoute: a short feel-test course made only from the ParkourPack.

One of each mechanic, in order along +X:
  start -> 4 m gap -> slide gate -> wall run over a 12 m gap -> wall-kick shaft up one storey
  -> blast switch and gate -> slingshot swing from a crane -> slope slide jump over 8 m -> finish (Ari).

Headless (editor closed, or at least not holding this map):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
Re-running rebuilds the map from scratch, so hand edits to L_TestRoute are lost.
"""
import math

import unreal

MAP = "/Game/Maps/L_TestRoute"
PACK = "/Game/ParkourPack/"
CELL = 400.0          # building grid
STOREY = 400.0
DECK = 30.0           # roof tile on top of a block: walking height = floors * 400 + 30

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
# Headless runs have no live editor subsystems; the class default object does the same job there.
actors = (unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
          or unreal.get_default_object(unreal.EditorActorSubsystem))
count = 0


def walk_z(floors):
    return floors * STOREY + DECK


def spawn(cls, x, y, z, yaw=0.0, pitch=0.0, scale=(1.0, 1.0, 1.0), label=None, tags=(), folder=None):
    global count
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    if not actor:
        raise RuntimeError("could not spawn %s" % cls)
    if tuple(scale) != (1.0, 1.0, 1.0):
        actor.set_actor_scale3d(unreal.Vector(*scale))
    if label:
        actor.set_actor_label(label)
    if folder:
        actor.set_folder_path(folder)
    if tags:
        actor.tags = [unreal.Name(t) for t in tags]
    count += 1
    return actor


_meshes = {}


def mesh(name, x, y, z, yaw=0.0, scale=(1.0, 1.0, 1.0), tags=(), folder="Route", movable=False, collide=True):
    if name not in _meshes:
        path = name if name.startswith("/") else PACK + "SM_" + name
        _meshes[name] = unreal.EditorAssetLibrary.load_asset(path)
        if not _meshes[name]:
            raise RuntimeError("missing mesh " + path)
    actor = spawn(unreal.StaticMeshActor, x, y, z, yaw=yaw, scale=scale, tags=tags, folder=folder)
    comp = actor.static_mesh_component
    if movable:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(_meshes[name])
    if not collide:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_label("%s_%d" % (name.split("/")[-1], count))
    return actor


def mover(name, x, y, z, yaw=0.0, travel=(0.0, 0.0, 230.0), vanish=False, tags=(), folder="Route"):
    """A door or lift: moves by `travel` when a BlastSwitch sharing one of its tags is hit."""
    actor = spawn(unreal.RomanceMover, x, y, z, yaw=yaw, tags=tags, folder=folder, label="%s_Mover" % name)
    actor.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset(PACK + "SM_" + name))
    actor.set_editor_property("travel", unreal.Vector(*travel))
    actor.set_editor_property("vanish_when_open", vanish)
    return actor


def roof(ix0, ix1, iy0, iy1, floors, tags=(), folder="Route/Roofs"):
    """Flat roof: stacked building blocks with a roof tile on top, one per 4 m cell."""
    for ix in range(ix0, ix1 + 1):
        for iy in range(iy0, iy1 + 1):
            for f in range(floors):
                mesh("Building_Block_4x4", ix * CELL, iy * CELL, f * STOREY, tags=tags, folder=folder)
            mesh("Roof_Tile_4x4", ix * CELL, iy * CELL, walk_z(floors), folder=folder)


def point(kind, x, y, z, yaw=0.0, pitch=0.0, order=0, label="", hint="", radius=None, tags=()):
    actor = spawn(unreal.RomancePoint, x, y, z, yaw=yaw, pitch=pitch, tags=tags, folder="Course",
                  label="%s_%s" % (str(kind).split(".")[-1].title(), label.replace(" ", "") or order))
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("order", order)
    actor.set_editor_property("label", label)
    actor.set_editor_property("hint", hint)
    if radius:
        actor.set_editor_property("radius", radius)
    return actor


K = unreal.RomancePointKind
H1 = walk_z(1)   # 430
H2 = walk_z(2)   # 830

# ---------------------------------------------------------------- A: start, vault props, 4 m gap
roof(0, 3, -1, 1, 1)
point(K.START, 0, 0, H1, hint="SPACE jumps; hold it to keep hopping. Clear the gap ahead.")
mesh("Start_Arch", 300, 0, H1, yaw=90)
mesh("Vault_Box", 800, 0, H1, yaw=90)
mesh("AC_Unit", 1050, -320, H1)
mesh("Pallet_Stack", 1050, 330, H1)

# ---------------------------------------------------------------- B: slide gate
roof(5, 6, -1, 1, 1)
roof(7, 7, -1, 0, 1)
point(K.CHECKPOINT, 2000, 0, H1, order=0, label="Slide gate", hint="Hold CTRL while running to slide under the gate.")
mesh("Checkpoint_Ring", 2000, 0, H1, yaw=90)
mesh("Slide_Gate", 2400, 0, H1, yaw=90)
mesh("Fence_Chainlink_4m", 2400, 375, H1, yaw=90)
mesh("Fence_Chainlink_4m", 2400, -375, H1, yaw=90)
point(K.MARKER, 1960, 0, H1, label="VerifySlide", tags=("VerifySlide",))

# ---------------------------------------------------------------- C: wall run along a tall facade over a 12 m gap
# The facade (y = 200, facing the route) starts beside roof B and ends beside roof C.
roof(7, 11, 1, 1, 2, tags=("RideWall",), folder="Route/RideWalls")
point(K.MARKER, 2700, -100, H1, label="WallRunHint",
      hint="Jump beside the tall wall while holding W to wall-run. SPACE kicks off.")
mesh("Arrow_Sign", 2900, -150, H1, yaw=90)
point(K.MARKER, 2900, 120, H1 + 50, label="VerifyRide", tags=("VerifyRide",))
roof(11, 11, -1, 0, 1)
roof(12, 13, -1, 1, 1)
point(K.CHECKPOINT, 4800, 0, H1, order=1, label="Wall climb",
      hint="Climb between the two walls: SPACE against each wall in turn.")
mesh("Checkpoint_Ring", 4800, 0, H1, yaw=90)
# Jump pad: the other way up to the two-storey roof.
mesh("Jump_Pad", 5000, -400, H1)
pad = point(K.PAD, 5000, -400, H1, label="PadUp", radius=70)
pad.set_editor_property("pad_target", unreal.Vector(800, 0, H2 - H1))

# ---------------------------------------------------------------- D: wall-kick shaft up to the two-storey roof
roof(14, 14, -1, 1, 2, tags=("RideWall",), folder="Route/RideWalls")   # its -X face is one side of the shaft
roof(15, 16, -1, 1, 2)
# Free-standing wall 2 m in front of that face: Wall_Tall is 0.7 m thick, so its near face sits at x = 5200.
mesh("Wall_Tall_2x6", 5165, 0, H1, yaw=90, tags=("RideWall",), folder="Route/RideWalls")
point(K.CHECKPOINT, 5700, 0, H2, order=2, label="Blast gate",
      hint="Throw a charge (RMB or Q) at the red switch. At your feet it is a rocket jump.")
mesh("Checkpoint_Ring", 5700, 0, H2, yaw=90)

# ---------------------------------------------------------------- E: blast switch opens the gate
mesh("Gate_Frame_4x3", 6200, 0, H2, yaw=90)
mover("Gate_Door", 6200, 0, H2, yaw=90, travel=(0, 0, 230), vanish=True, tags=("GateA",))
# Test marker: stand facing the switch and throw a charge at it.
point(K.MARKER, 5800, -402, H2, label="VerifySwitch", tags=("VerifySwitch",))
mesh("Wall_Panel_4x3", 6200, 402, H2, yaw=90)
mesh("Wall_Panel_4x3", 6200, -402, H2, yaw=90)
mesh("Blast_Switch", 6155, -402, H2 + 160, yaw=90, tags=("BlastSwitch", "GateA"))
mesh("Launch_Marker", 5950, -402, H2 + 1)
mesh("Charge_Crate", 5850, 430, H2)

# ---------------------------------------------------------------- F: slingshot across a 20 m gap from a crane
roof(19, 19, 1, 1, 4, folder="Route/Crane")
crane_z = walk_z(4)
mesh("Crane_Arm", 19 * CELL, CELL, crane_z, yaw=180, folder="Route/Crane")   # arm reaches over the route (-Y)
point(K.MARKER, 6400, 0, H2, label="SlingHint",
      hint="Aim at the purple ring on the crane and hold LMB to swing. SPACE lets go.")
# The ring hangs 5 m out along the arm and 2.18 m up (pack README); the arm points to -Y here.
ring = (19 * CELL, CELL - 500.0, crane_z + 218.0)
point(K.ANCHOR, ring[0], ring[1], ring[2], label="CraneRing")
# Test marker: stand at the edge looking a little above the ring, so the aim assist has to find it.
eye = (6500.0, 0.0, H2 + 3 + 88 + 68)
mast = (ring[0], ring[1], ring[2] + 60.0)
d = [mast[i] - eye[i] for i in range(3)]
point(K.MARKER, eye[0], eye[1], H2, yaw=math.degrees(math.atan2(d[1], d[0])),
      pitch=math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))), label="VerifySling", tags=("VerifySling",))

roof(22, 23, -1, 1, 2)
point(K.CHECKPOINT, 8900, 0, H2, order=3, label="Slope",
      hint="Slide down the slope and jump on the flat to clear the gap.")
mesh("Checkpoint_Ring", 8900, 0, H2, yaw=90)

# ---------------------------------------------------------------- G: slope slide jump over 8 m
# Roof_Slope drops 2 m toward its +Y edge; yaw -90 turns that edge to +X. Two in a row drop one storey.
for iy in (-1, 0, 1):
    mesh("Building_Block_4x4", 24 * CELL, iy * CELL, 0, scale=(1, 1, 1.5), folder="Route/Roofs")
    mesh("Roof_Slope_4x4", 24 * CELL, iy * CELL, H1 + 200, yaw=-90, folder="Route/Roofs")
    mesh("Building_Block_4x4", 25 * CELL, iy * CELL, 0, folder="Route/Roofs")
    mesh("Roof_Slope_4x4", 25 * CELL, iy * CELL, H1, yaw=-90, folder="Route/Roofs")
roof(26, 26, -1, 1, 1)

# ---------------------------------------------------------------- H: finish
roof(29, 32, -1, 1, 1)
mesh("Finish_Arch", 12000, 0, H1, yaw=90)
mesh("Crash_Mat", 11600, 0, H1, yaw=90)
point(K.DATE, 12500, 0, H1, yaw=180, order=0, label="Test finish", radius=350,
      hint="Ari is waiting past the arch. Walk up and press E.")
mesh("Collectible_Star", 12500, 0, H1 + 170, yaw=90, collide=False)
# Lift demo beside the finish: a charge on the floor plate raises the platform 3 m.
mesh("Blast_Plate", 11700, 400, H1, tags=("BlastSwitch", "LiftA"))
mover("Lift_Platform_2x2", 12100, 400, H1 + 2, travel=(0, 0, 300), tags=("LiftA",))
mesh("Collectible_Star", 12100, 400, H1 + 470, yaw=90, collide=False)

# ---------------------------------------------------------------- sky, light, street
street = mesh("/Engine/BasicShapes/Plane", 6400, 0, 0, scale=(400, 400, 1), folder="Sky", collide=False)

sun = spawn(unreal.DirectionalLight, 0, 0, 2000, yaw=35, pitch=-42, label="Sun", folder="Sky")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_intensity(4.4)
sc.set_light_color(unreal.LinearColor(1.0, 0.92, 0.80))
sc.set_editor_property("atmosphere_sun_light", True)

sky = spawn(unreal.SkyLight, 0, 0, 2200, label="SkyLight", folder="Sky")
kc = sky.get_component_by_class(unreal.SkyLightComponent)
kc.set_mobility(unreal.ComponentMobility.MOVABLE)
kc.set_editor_property("real_time_capture", True)
kc.set_intensity(1.5)

spawn(unreal.SkyAtmosphere, 0, 0, 0, label="SkyAtmosphere", folder="Sky")

fog = spawn(unreal.ExponentialHeightFog, 0, 0, 200, label="Haze", folder="Sky")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_fog_density(0.016)
fc.set_fog_height_falloff(0.105)
fc.set_fog_inscattering_color(unreal.LinearColor(0.42, 0.61, 0.72))
fc.set_start_distance(5000.0)

# Same fixed exposure the runner's camera uses, so the editor view, the title camera and play all match.
ppv = spawn(unreal.PostProcessVolume, 0, 0, 0, label="Exposure", folder="Sky")
ppv.set_editor_property("unbound", True)
pp = ppv.get_editor_property("settings")
pp.set_editor_property("override_auto_exposure_method", True)
pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
pp.set_editor_property("override_auto_exposure_bias", True)
pp.set_editor_property("auto_exposure_bias", -0.15)
ppv.set_editor_property("settings", pp)

# Title screen view: over the start, looking down the route.
cam_at = (-900.0, -1500.0, 1400.0)
look = (2600.0, 100.0, 500.0)
d = [look[i] - cam_at[i] for i in range(3)]
spawn(unreal.CameraActor, cam_at[0], cam_at[1], cam_at[2], yaw=math.degrees(math.atan2(d[1], d[0])),
      pitch=math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))), label="TitleCamera", tags=("TitleCamera",),
      folder="Course")

ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.log("L_TestRoute: %d actors, saved=%s" % (count, ok))
