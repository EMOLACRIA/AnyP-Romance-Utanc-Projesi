"""Shared level-building helpers for the main level: frames, mesh and point spawning, and a Course
that lays rooftop parkour segments one after another.

Units are cm. A Frame is a position plus a yaw (use multiples of 90 so the 4 m grid stays square);
everything is placed in the frame's local space, with local +X as the running direction.
"""
import math

import unreal

CELL = 400.0          # building grid
STOREY = 400.0
DECK = 30.0           # roof tile on a block: walking height = floors * 400 + 30
PARKOUR = "ParkourPack"

K = unreal.RomancePointKind

_actors = (unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
           or unreal.get_default_object(unreal.EditorActorSubsystem))
_meshes = {}
count = 0
_checkpoints = 0
_gates = 0


TINTS = 8             # MI_Block_0..7 from make_polish_assets.py
current_tint = None   # facade tint applied to building blocks placed from here on
_buildings = 0
keepouts = []         # world boxes (x0, y0, x1, y1) the background city must stay out of


def walk_z(floors):
    return floors * STOREY + DECK


def tint_material(i):
    return unreal.EditorAssetLibrary.load_asset("/Game/Romance/MI_Block_%d" % (i % TINTS))


def next_tint():
    """Each building gets the next facade colour."""
    global current_tint, _buildings
    current_tint = _buildings % TINTS
    _buildings += 3      # step by 3 so neighbours differ clearly


def box(frame, x0, y0, x1, y1):
    """World-aligned box around a local rectangle (frames are turned in right angles)."""
    pts = [frame.at(x, y) for x in (x0, x1) for y in (y0, y1)]
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts))


def keep_out(frame, x0, y0, x1, y1):
    keepouts.append(box(frame, x0, y0, x1, y1))


def _clear(b):
    return all(b[2] <= k[0] or b[0] >= k[2] or b[3] <= k[1] or b[1] >= k[3] for k in keepouts)


def city_block(frame, x, y, cells_x, cells_y, floors, tint):
    """One background building: an instanced shell with no collision (ARomanceCityBlock)."""
    actor = spawn(unreal.RomanceCityBlock, frame, x, y, 0, label="City", folder="City")
    actor.set_editor_property("block_mesh", load_mesh("Building_Block_4x4"))
    actor.set_editor_property("roof_mesh", load_mesh("Roof_Tile_4x4"))
    actor.set_editor_property("block_material", tint_material(tint))
    actor.set_editor_property("cells_x", cells_x)
    actor.set_editor_property("cells_y", cells_y)
    actor.set_editor_property("floors", floors)
    actor.rebuild()
    return actor


def instanced_grid(frame, name, pack, x, y, z, count_x, count_y, spacing, scale, folder="City"):
    """One mesh repeated on a flat grid, no collision (the open sea)."""
    actor = spawn(unreal.RomanceCityBlock, frame, x, y, z, label=name + "_Grid", folder=folder)
    actor.set_editor_property("block_mesh", load_mesh(name, pack))
    actor.set_editor_property("flat_grid", True)
    actor.set_editor_property("cells_x", count_x)
    actor.set_editor_property("cells_y", count_y)
    actor.set_editor_property("cell", spacing)
    actor.set_editor_property("instance_scale", unreal.Vector(*scale))
    actor.rebuild()
    return actor


def skyline(frame, ix0, ix1, rng, rows=3):
    """Fill both sides of a lane (cells ix0..ix1 along the frame's +X) with background buildings of mixed
    footprint, height and colour, in rows with streets between them, skipping every keep-out box."""
    heights = (2, 3, 3, 4, 4, 4, 5, 5, 6, 7, 9, 12)
    placed = 0
    for side in (1, -1):
        for row in range(rows):
            ix = ix0 - 6
            while ix < ix1 + 6:
                cx, cy = rng.randint(2, 4), rng.randint(2, 4)
                start = 5 + row * 6 + rng.randint(0, 1)
                iy = start if side > 0 else -(start + cy - 1)
                b = box(frame, ix * CELL - 200, iy * CELL - 200, (ix + cx) * CELL - 200, (iy + cy) * CELL - 200)
                if _clear(b):
                    city_block(frame, ix * CELL, iy * CELL, cx, cy, rng.choice(heights), rng.randrange(TINTS))
                    placed += 1
                ix += cx + rng.randint(1, 2)
    return placed


class Frame:
    def __init__(self, x=0.0, y=0.0, z=0.0, yaw=0.0):
        self.x, self.y, self.z, self.yaw = x, y, z, yaw

    def at(self, x, y, z=0.0):
        c, s = math.cos(math.radians(self.yaw)), math.sin(math.radians(self.yaw))
        return (self.x + x * c - y * s, self.y + x * s + y * c, self.z + z)

    def child(self, x, y, z=0.0, yaw=0.0):
        wx, wy, wz = self.at(x, y, z)
        return Frame(wx, wy, wz, self.yaw + yaw)


def spawn(cls, frame, x, y, z, yaw=0.0, pitch=0.0, scale=(1.0, 1.0, 1.0), label=None, tags=(), folder=None):
    global count
    wx, wy, wz = frame.at(x, y, z)
    actor = _actors.spawn_actor_from_class(cls, unreal.Vector(wx, wy, wz),
                                           unreal.Rotator(roll=0.0, pitch=pitch, yaw=frame.yaw + yaw))
    if not actor:
        raise RuntimeError("could not spawn %s" % cls)
    if tuple(scale) != (1.0, 1.0, 1.0):
        actor.set_actor_scale3d(unreal.Vector(*scale))
    count += 1
    if label:
        actor.set_actor_label("%s_%d" % (label, count))
    if folder:
        actor.set_folder_path(folder)
    if tags:
        actor.tags = [unreal.Name(t) for t in tags]
    return actor


def load_mesh(name, pack=PARKOUR):
    path = name if name.startswith("/") else "/Game/%s/SM_%s" % (pack, name)
    if path not in _meshes:
        _meshes[path] = unreal.EditorAssetLibrary.load_asset(path)
        if not _meshes[path]:
            raise RuntimeError("missing mesh " + path)
    return _meshes[path]


def mesh(frame, name, x, y, z, yaw=0.0, scale=(1.0, 1.0, 1.0), tags=(), folder="Route", pack=PARKOUR, collide=True):
    actor = spawn(unreal.StaticMeshActor, frame, x, y, z, yaw=yaw, scale=scale, tags=tags, folder=folder,
                  label=name.split("/")[-1])
    comp = actor.static_mesh_component
    comp.set_static_mesh(load_mesh(name, pack))
    if name == "Building_Block_4x4" and current_tint is not None:
        comp.set_material(0, tint_material(current_tint))
    if not collide:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def mover(frame, name, x, y, z, yaw=0.0, travel=(0.0, 0.0, 230.0), vanish=False, tags=(), folder="Route"):
    """A door or lift: moves by `travel` (world space) when a BlastSwitch sharing one of its tags is hit."""
    actor = spawn(unreal.RomanceMover, frame, x, y, z, yaw=yaw, tags=tags, folder=folder, label=name + "_Mover")
    actor.static_mesh_component.set_static_mesh(load_mesh(name))
    actor.set_editor_property("travel", unreal.Vector(*travel))
    actor.set_editor_property("vanish_when_open", vanish)
    return actor


def point(frame, kind, x, y, z, yaw=0.0, pitch=0.0, order=0, label="", hint="", radius=None, tags=(), name=None):
    actor = spawn(unreal.RomancePoint, frame, x, y, z, yaw=yaw, pitch=pitch, tags=tags, folder="Course",
                  label=name or (str(kind).split(".")[-1].title() + "_" + (label.replace(" ", "") or str(order))))
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("order", order)
    actor.set_editor_property("label", label)
    actor.set_editor_property("hint", hint)
    if radius:
        actor.set_editor_property("radius", radius)
    return actor


def checkpoint(frame, x, y, z, label, hint="", ring=True, yaw=0.0):
    global _checkpoints
    point(frame, K.CHECKPOINT, x, y, z, yaw=yaw, order=_checkpoints, label=label, hint=hint)
    _checkpoints += 1
    if ring:
        mesh(frame, "Checkpoint_Ring", x, y, z, yaw=yaw + 90)


def aim(src, dst):
    """Yaw and pitch (degrees) that look from src to dst, both local to the same frame."""
    d = [dst[i] - src[i] for i in range(3)]
    return math.degrees(math.atan2(d[1], d[0])), math.degrees(math.atan2(d[2], math.hypot(d[0], d[1])))


def put(frame, pack, name, bx, by, bz=0.0, rz=0.0, s=1.0, folder="Venue", collide=True):
    """Place a pack asset from the pack's own Blender demo layout: metres, Blender axes.
    The FBX import mirrors Y, so position Y and the rotation both change sign."""
    scale = (s, s, s) if isinstance(s, (int, float)) else tuple(s)
    return mesh(frame, name, bx * 100.0, -by * 100.0, bz * 100.0, yaw=-rz, scale=scale, folder=folder, pack=pack,
                collide=collide)


def warm_light(frame, bx, by, bz, intensity=3.0, radius=600.0):
    """The packs paint fire and lamp glow into the textures; these add the matching warm light."""
    actor = spawn(unreal.PointLight, frame, bx * 100.0, -by * 100.0, bz * 100.0, label="WarmLight", folder="Venue/Lights")
    c = actor.get_component_by_class(unreal.PointLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("use_inverse_squared_falloff", False)
    c.set_intensity(intensity)
    c.set_attenuation_radius(radius)
    c.set_light_color(unreal.LinearColor(1.0, 0.72, 0.42))
    c.set_cast_shadows(False)
    return actor


ROOF_PROPS = ("AC_Unit", "Vent_Mushroom", "Solar_Panel", "Chimney_Brick", "Satellite_Dish", "AC_Unit_Large",
              "Skylight", "Pallet_Stack", "Oil_Barrel", "Cable_Spool")


class Course:
    """Lays rooftop segments along a frame's +X. `ix` is the next free 4 m cell, `floors` the current roof height.
    The lane is three cells wide (iy -1..1). Each mechanic segment also drops the marker that
    -RomanceVerifyMovement uses to play that mechanic against the built geometry."""

    def __init__(self, frame, ix, floors, name):
        self.f, self.ix, self.floors, self.name = frame, ix, floors, name
        self.start_ix = ix

    # -- basics
    @property
    def z(self):
        return walk_z(self.floors)

    def cells(self):
        return self.ix - self.start_ix

    def roof(self, ix0, ix1, lane=(-1, 0, 1), floors=None, tags=(), folder="Route/Roofs"):
        floors = self.floors if floors is None else floors
        next_tint()
        for ix in range(ix0, ix1 + 1):
            for iy in lane:
                for fl in range(floors):
                    mesh(self.f, "Building_Block_4x4", ix * CELL, iy * CELL, fl * STOREY, tags=tags, folder=folder)
                mesh(self.f, "Roof_Tile_4x4", ix * CELL, iy * CELL, walk_z(floors), folder=folder)

    def run(self, n, props=True):
        self.roof(self.ix, self.ix + n - 1)
        if props:
            for ix in range(self.ix, self.ix + n):
                if ix % 2 == 0:
                    side = 1 if (ix // 2) % 2 == 0 else -1
                    name = ROOF_PROPS[(ix // 2 + len(self.name)) % len(ROOF_PROPS)]
                    mesh(self.f, name, ix * CELL, side * 440, self.z, yaw=90 * ((ix // 2) % 4), folder="Route/Props")
        self.ix += n
        return self

    def gap(self, n=1):
        self.ix += n
        return self

    def cp(self, label, hint=""):
        # A little before the cell centre, clear of the test markers the next segment may drop there.
        checkpoint(self.f, (self.ix - 1) * CELL - 150, 0, self.z, label, hint)
        return self

    def hint(self, text, dx=0.0):
        point(self.f, K.MARKER, (self.ix - 1) * CELL + dx, 0, self.z, hint=text, name="Hint")
        return self

    def vault(self):
        """Low obstacles across the last cell: jump them."""
        x = (self.ix - 1) * CELL
        mesh(self.f, "Vault_Box", x, 0, self.z, yaw=90)
        mesh(self.f, "Barrier_Hazard", x, 300, self.z, yaw=90)
        mesh(self.f, "Barrier_Hazard", x, -300, self.z, yaw=90)
        return self

    # -- mechanics
    def slide_gate(self):
        x = self.ix * CELL
        self.roof(self.ix, self.ix)
        mesh(self.f, "Slide_Gate", x, 0, self.z, yaw=90)
        mesh(self.f, "Fence_Chainlink_4m", x, 375, self.z, yaw=90)
        mesh(self.f, "Fence_Chainlink_4m", x, -375, self.z, yaw=90)
        point(self.f, K.MARKER, x - 440, 0, self.z, tags=("VerifySlide",), name="VerifySlide")
        self.ix += 1
        return self

    def wallrun(self, n=3, side=1):
        """A facade one storey taller runs along one side of an n-cell gap; ride it across."""
        lane = tuple(iy for iy in (-1, 0, 1) if iy != side)
        self.roof(self.ix, self.ix, lane=lane)
        self.roof(self.ix, self.ix + n + 1, lane=(side,), floors=self.floors + 1, tags=("RideWall",),
                  folder="Route/RideWalls")
        self.roof(self.ix + n + 1, self.ix + n + 1, lane=lane)
        point(self.f, K.MARKER, self.ix * CELL + 100, side * 120, self.z + 50, tags=("VerifyRide",), name="VerifyRide")
        mesh(self.f, "Arrow_Sign", self.ix * CELL + 100, -side * 150, self.z, yaw=90)
        self.ix += n + 2
        return self

    def shaft_up(self):
        """Wall-kick shaft: a free-standing wall 2 m in front of the next, one-storey-taller building."""
        face = self.ix * CELL - 200
        mesh(self.f, "Wall_Tall_2x6", face - 235, 0, self.z, yaw=90, tags=("RideWall",), folder="Route/RideWalls")
        self.floors += 1
        self.roof(self.ix, self.ix, tags=("RideWall",), folder="Route/RideWalls")
        self.ix += 1
        return self

    def pad_up(self):
        """Jump pad on the last cell launching onto the next roof, one storey up."""
        x = (self.ix - 1) * CELL
        mesh(self.f, "Jump_Pad", x, 0, self.z)
        pad = point(self.f, K.PAD, x, 0, self.z, radius=70, name="Pad")
        pad.set_editor_property("pad_target", unreal.Vector(640, 0, STOREY))
        self.floors += 1
        return self

    def rocket_up(self):
        """A bare 4 m wall: only a rocket jump gets up."""
        x = (self.ix - 1) * CELL
        mesh(self.f, "Launch_Marker", x + 40, 0, self.z + 1)
        mesh(self.f, "Charge_Crate", x, 330, self.z)
        self.floors += 1
        return self

    def gate(self):
        """Blast switch and sliding door across the lane."""
        global _gates
        _gates += 1
        tag = "Gate%d" % _gates
        x = self.ix * CELL
        self.roof(self.ix, self.ix)
        mesh(self.f, "Gate_Frame_4x3", x, 0, self.z, yaw=90)
        mover(self.f, "Gate_Door", x, 0, self.z, yaw=90, travel=(0, 0, 230), vanish=True, tags=(tag,))
        mesh(self.f, "Wall_Panel_4x3", x, 402, self.z, yaw=90)
        mesh(self.f, "Wall_Panel_4x3", x, -402, self.z, yaw=90)
        mesh(self.f, "Blast_Switch", x - 45, -402, self.z + 160, yaw=90, tags=("BlastSwitch", tag))
        # Test marker: stand clear of the roof props and look at the switch.
        eye = (x - 400.0, -250.0, self.z + 3 + 88 + 68)
        yaw, pitch = aim(eye, (x - 45.0, -402.0, self.z + 160.0))
        point(self.f, K.MARKER, eye[0], eye[1], self.z, yaw=yaw, pitch=pitch, tags=("VerifySwitch",), name="VerifySwitch")
        self.ix += 1
        return self

    def sling(self, n=5, side=1):
        """A crane on a tower beside an n-cell gap; its ring hangs over the lane."""
        up = 2 if n <= 5 else 3
        tx = (self.ix + n // 2) * CELL
        self.roof(self.ix + n // 2, self.ix + n // 2, lane=(side,), floors=self.floors + up, folder="Route/Crane")
        top = walk_z(self.floors + up)
        mesh(self.f, "Crane_Arm", tx, side * CELL, top, yaw=180 if side > 0 else 0, folder="Route/Crane")
        ring = (tx, side * CELL - side * 500.0, top + 218.0)
        point(self.f, K.ANCHOR, ring[0], ring[1], ring[2], name="CraneRing")
        eye = (self.ix * CELL - 300.0, 0.0, self.z + 3 + 88 + 68)
        yaw, pitch = aim(eye, (ring[0], ring[1], ring[2] + 60.0))
        point(self.f, K.MARKER, eye[0], eye[1], self.z, yaw=yaw, pitch=pitch, tags=("VerifySling",), name="VerifySling")
        self.ix += n
        return self

    def slope_down(self):
        """Two sloped roof cells dropping one storey; slide down them for speed."""
        low = self.floors - 1
        next_tint()
        for iy in (-1, 0, 1):
            for k, (zslope, half) in enumerate(((walk_z(low) + 200, True), (walk_z(low), False))):
                x = (self.ix + k) * CELL
                for fl in range(low):
                    mesh(self.f, "Building_Block_4x4", x, iy * CELL, fl * STOREY, folder="Route/Roofs")
                if half:
                    mesh(self.f, "Building_Block_4x4", x, iy * CELL, low * STOREY, scale=(1, 1, 0.5), folder="Route/Roofs")
                mesh(self.f, "Roof_Slope_4x4", x, iy * CELL, zslope, yaw=-90, folder="Route/Roofs")
        self.floors = low
        self.ix += 2
        return self

    def bridge(self, n=2):
        """Planks over an n-cell gap."""
        for k in range(n):
            mesh(self.f, "Plank_Bridge_4m", (self.ix + k) * CELL, 0, self.z)
        self.ix += n
        return self
