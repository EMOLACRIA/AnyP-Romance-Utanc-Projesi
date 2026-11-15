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
CITY = "CityPack"
FACADE_TINTS = 5      # MI_<Block>_0..4 per CityPack facade and ground floor (make_polish_assets.py)
CAR_TINTS = 6         # MI_Car_Parked_0..5, MI_Van_Parked_0..5
current_tint = None   # facade tint applied to building blocks placed from here on
current_facade = "Building_Block_4x4"
current_ground = "Building_Block_4x4"
_buildings = 0
keepouts = []         # world boxes (x0, y0, x1, y1) the background city must stay out of
lanes = []            # the route corridors among them: the city grows around these
cores = []            # where route buildings stand: nothing tall may grow there and poke through a roof
voids = []            # boxes with no city at all

# Route buildings only use blocks that stay inside the 4 x 4 x 4 m box and share Building_Block_4x4's
# box collision, so the routes' footprints, heights and wall-run faces are unchanged.
ROUTE_FACADES = ("Building_Block_4x4", "Block_Brick_Tall", "Block_Concrete_Strip", "Building_Block_4x4",
                 "Block_Office_Glazed", "Block_Party_Pipes", "Block_Brick_Tall")
ROUTE_GROUNDS = ("Ground_Entrance", "Ground_Garage_Door", "Building_Block_4x4")


def walk_z(floors):
    return floors * STOREY + DECK


def tint_material(i):
    return unreal.EditorAssetLibrary.load_asset("/Game/Romance/MI_Block_%d" % (i % TINTS))


def next_tint():
    """Each building gets the next facade colour, upper-floor block and ground floor."""
    global current_tint, current_facade, current_ground, _buildings
    n = _buildings // 3
    current_tint = _buildings % TINTS
    current_facade = ROUTE_FACADES[n % len(ROUTE_FACADES)]
    current_ground = ROUTE_GROUNDS[(n // 2) % len(ROUTE_GROUNDS)]
    _buildings += 3      # step by 3 so neighbours differ clearly


def block(frame, x, y, floor, scale=(1.0, 1.0, 1.0), tags=(), folder="Route/Roofs"):
    """One storey of a route building: the current building's ground floor or facade block, tinted."""
    name = current_ground if floor == 0 else current_facade
    if name == "Building_Block_4x4":
        return mesh(frame, name, x, y, floor * STOREY, scale=scale, tags=tags, folder=folder)
    actor = mesh(frame, name, x, y, floor * STOREY, scale=scale, tags=tags, folder=folder, pack=CITY)
    actor.static_mesh_component.set_material(0, city_material(name, (current_tint or 0) % FACADE_TINTS))
    return actor


def box(frame, x0, y0, x1, y1):
    """World-aligned box around a local rectangle (frames are turned in right angles)."""
    # Rounded: a frame turned 180 degrees puts 31000 at 30999.9999..., which would drop a whole row of cells.
    pts = [tuple(round(v, 2) for v in frame.at(x, y)) for x in (x0, x1) for y in (y0, y1)]
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts))


def keep_out(frame, x0, y0, x1, y1):
    keepouts.append(box(frame, x0, y0, x1, y1))


def _clear(b):
    return all(b[2] <= k[0] or b[0] >= k[2] or b[3] <= k[1] or b[1] >= k[3] for k in keepouts)


def lane(frame, x0, y0, x1, y1):
    """A route's corridor: no background buildings inside it, and the city grows around it."""
    keepouts.append(box(frame, x0, y0, x1, y1))
    lanes.append(keepouts[-1])
    core(frame, x0, -800, x1, 800)


def core(frame, x0, y0, x1, y1):
    """Ground under route buildings and venues: street furniture taller than a car stays out."""
    cores.append(box(frame, x0, y0, x1, y1))


def void(frame, x0, y0, x1, y1):
    """Somewhere that is not city at all (the sea, the mountain): no buildings and no streets."""
    keepouts.append(box(frame, x0, y0, x1, y1))
    voids.append(keepouts[-1])


seas = []             # world boxes of open water; the city meets them with a quay
QUAY_TOP, QUAY_BOTTOM = -12.0, -320.0


def sea(frame, x0, y0, x1, y1):
    """Open water: a void the city meets with a stone quay, a kerb, railings and lanterns.
    The water itself must lie below the street (z 0) and above QUAY_BOTTOM."""
    void(frame, x0, y0, x1, y1)
    seas.append(voids[-1])


def city_material(name, tint=0):
    path = "/Game/Romance/City/MI_%s_%d" % (name, tint)
    if path not in _meshes:
        _meshes[path] = unreal.EditorAssetLibrary.load_asset(path)
        if not _meshes[path]:
            raise RuntimeError("missing material %s (run make_polish_assets.py)" % path)
    return _meshes[path]


def face(dx, dy):
    """Yaw that turns a piece's front (+Y in Unreal, -Y in the packs' Blender files) toward (dx, dy)."""
    return math.degrees(math.atan2(dy, dx)) - 90.0


CORNER_YAW = {(1, 1): 0.0, (-1, 1): 90.0, (-1, -1): 180.0, (1, -1): 270.0}


class Scenery:
    """Instanced, collision-free scenery in world space. Pieces are collected per mesh and tint, then handed
    to one ARomanceCityBlock, which draws each kind with a single instanced component."""

    def __init__(self, label, folder="City"):
        self.label, self.folder, self.items, self.total = label, folder, {}, 0

    def add(self, name, x, y, z=0.0, yaw=0.0, tint=0, pack=CITY, shadow=True, cull=0.0):
        t = unreal.Transform(location=unreal.Vector(x, y, z), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw),
                             scale=unreal.Vector(1.0, 1.0, 1.0))
        self.items.setdefault((pack, name, tint, shadow, cull), []).append(t)
        self.total += 1

    def commit(self):
        actor = spawn(unreal.RomanceCityBlock, Frame(), 0, 0, 0, label=self.label, folder=self.folder)
        for (pack, name, tint, shadow, cull), transforms in sorted(self.items.items(), key=lambda kv: kv[0][:3]):
            actor.add_pieces(load_mesh(name, pack), city_material(name, tint), transforms, shadow, cull)
        return actor


# Background building styles: upper-floor block, blocks mixed into it, ground floors, roof edge.
# Tall buildings draw from the last three.
STYLES = (
    ("Block_Brick_Tall", ("Block_Party_Pipes", "Block_Bay_Window"), ("Ground_Shop_Awning", "Ground_Entrance", "Ground_Cafe_Front"), "Classic"),
    ("Block_Plaster_Balcony", ("Block_Bay_Window",), ("Ground_Cafe_Front", "Ground_Shop_Awning", "Ground_Entrance"), "Eave"),
    ("Block_Bay_Window", ("Block_Plaster_Balcony",), ("Ground_Shop_Awning", "Ground_Cafe_Front", "Ground_Entrance"), "Classic"),
    ("Block_Party_Pipes", ("Block_Brick_Tall",), ("Ground_Garage_Door", "Ground_Entrance"), "Parapet"),
    ("Block_Brick_Tall", ("Block_Plaster_Balcony",), ("Ground_Entrance", "Ground_Garage_Door", "Ground_Shop_Awning"), "Parapet"),
    ("Block_Concrete_Strip", ("Block_Office_Glazed",), ("Ground_Garage_Door", "Ground_Entrance", "Ground_Shop_Awning"), "Parapet"),
    ("Block_Office_Glazed", ("Block_Concrete_Strip",), ("Ground_Entrance", "Ground_Garage_Door"), "Parapet"),
    ("Block_Concrete_Strip", (), ("Ground_Shop_Awning", "Ground_Cafe_Front", "Ground_Garage_Door"), "Classic"),
)
CITY_ROOF_PROPS = ("Water_Tank", "AC_Unit_Large", "Chimney_Brick", "Roof_Access", "Solar_Panel", "Satellite_Dish",
                   "AC_Unit", "Vent_Mushroom")


def _shell(s, rng, x, y, nx, ny, f0, f1, block, mixed, corner, grounds, tint, cornice):
    """Floors f0..f1-1 of a hollow nx x ny shell whose first cell centre is (x, y), then its roof and roof edge."""
    ring = [(i, j) for i in range(nx) for j in range(ny) if i in (0, nx - 1) or j in (0, ny - 1)]
    shift = rng.randrange(8)
    for n, (i, j) in enumerate(ring):
        at_corner = i in (0, nx - 1) and j in (0, ny - 1)
        column = corner if (corner and at_corner) else (mixed if mixed and (n + shift) % 3 == 0 else block)
        for fl in range(f0, f1):
            if fl == 0:
                s.add(grounds[(n + shift) % len(grounds)], x + i * CELL, y + j * CELL, 0.0, tint=0)
            else:
                s.add(column, x + i * CELL, y + j * CELL, fl * STOREY, tint=tint)
    top = f1 * STOREY
    for i in range(nx):
        for j in range(ny):
            s.add("Roof_Tile_4x4", x + i * CELL, y + j * CELL, top + DECK, pack=PARKOUR)
    if cornice:
        for i in range(nx):
            for j, d in ((0, -1), (ny - 1, 1)):
                s.add("Cornice_%s_4m" % cornice, x + i * CELL, y + j * CELL + d * 200, top, yaw=face(0, d))
        for j in range(ny):
            for i, d in ((0, -1), (nx - 1, 1)):
                s.add("Cornice_%s_4m" % cornice, x + i * CELL + d * 200, y + j * CELL, top, yaw=face(d, 0))
        for sx in (-1, 1):
            for sy in (-1, 1):
                s.add("Cornice_%s_Corner" % cornice, x + (nx - 1) * CELL * (sx > 0) + sx * 200,
                      y + (ny - 1) * CELL * (sy > 0) + sy * 200, top, yaw=CORNER_YAW[(sx, sy)])


def city_block(s, rng, x, y, nx, ny, floors):
    """One background building into Scenery `s`: its first cell centre is (x, y), the footprint nx x ny cells.
    It picks a style (facade, a second facade mixed in, ground floors, roof edge), a tint, and for big tall
    ones a narrower tower on top, then dresses the roof."""
    style = rng.choice(STYLES[4:] if floors >= 7 else STYLES)
    block, mixes, grounds, cornice = style
    mixed = rng.choice(mixes) if mixes and rng.random() < 0.55 else None
    corner = "Block_Bay_Window" if block in ("Block_Brick_Tall", "Block_Plaster_Balcony") and rng.random() < 0.4 else None
    grounds = list(grounds)
    rng.shuffle(grounds)
    grounds = grounds[:rng.randint(1, len(grounds))]
    tint = rng.randrange(FACADE_TINTS)
    base = floors
    if floors >= 6 and nx >= 3 and ny >= 3 and rng.random() < 0.6:
        base = max(2, int(floors * rng.uniform(0.35, 0.65)))
    _shell(s, rng, x, y, nx, ny, 0, base, block, mixed, corner, grounds, tint, cornice)
    rx, ry, rnx, rny = x, y, nx, ny
    if base < floors:
        rx, ry, rnx, rny = x + CELL, y + CELL, nx - 2, ny - 2
        _shell(s, rng, rx, ry, rnx, rny, base, floors, block, mixed, None, grounds, tint, cornice)
    top = floors * STOREY + DECK
    for _ in range(rng.randint(0, 2 + (rnx * rny) // 4)):
        s.add(rng.choice(CITY_ROOF_PROPS), rx + rng.uniform(-80, (rnx - 1) * CELL + 80),
              ry + rng.uniform(-80, (rny - 1) * CELL + 80), top, yaw=90.0 * rng.randrange(4), pack=PARKOUR, cull=30000.0)
    if floors >= 5 and rng.random() < 0.3:
        s.add("Billboard_6x3", rx + (rnx - 1) * CELL / 2.0, ry + (rny - 1) * CELL / 2.0, top, yaw=90.0 * rng.randrange(4),
              pack=PARKOUR)


def _split(rng, total):
    """Cut a run of cells into plots 2 to 4 cells long."""
    parts = []
    while total > 0:
        n = rng.randint(2, 4)
        if total - n == 1:
            n += 1 if n < 4 else -1
        n = min(n, total)
        parts.append(n)
        total -= n
    return parts


def city(rng, reach=22):
    """The city around the routes, on the world's 4 m grid: a street grid (two lanes of road, with crossings
    at the junctions) cutting it into lots; each lot is a ring of pavement with kerbs, lamps, trees, benches,
    bus stops and traffic lights around plots that hold buildings; cars park along the kerbs.
    It grows `reach` cells out from every lane(), builds nothing inside a keep-out and leaves void()s empty."""
    G = CELL
    HEIGHTS = (2, 3, 3, 4, 4, 4, 5, 5, 6, 7, 9, 12)
    i0 = int(math.floor(min(b[0] for b in lanes) / G)) - reach
    i1 = int(math.ceil(max(b[2] for b in lanes) / G)) + reach
    j0 = int(math.floor(min(b[1] for b in lanes) / G)) - reach
    j1 = int(math.ceil(max(b[3] for b in lanes) / G)) + reach
    pad = reach * G

    def inside(i, j):
        x, y = i * G, j * G
        if not any(b[0] - pad <= x <= b[2] + pad and b[1] - pad <= y <= b[3] + pad for b in lanes):
            return False
        return all(x + 200 <= v[0] or x - 200 >= v[2] or y + 200 <= v[1] or y - 200 >= v[3] for v in voids)

    def cut(a, b):
        roads, lots, k = set(), [], a
        while k < b:
            n = rng.choice((5, 6, 6, 7, 8))
            lots.append((k, n))
            roads.update((k + n, k + n + 1))
            k += n + 2
        return roads, lots

    road_x, lots_x = cut(i0, i1)
    road_y, lots_y = cut(j0, j1)
    ground, fronts, props = Scenery("CityGround"), Scenery("CityBuildings"), Scenery("CityStreet")
    cells = {(i, j) for i in range(i0, i1 + 1) for j in range(j0, j1 + 1) if inside(i, j)}
    built = set()
    tall = ("Tree_Planter", "Street_Lamp", "Traffic_Light", "Bus_Stop", "Van_Parked")

    def stand(name, x, y, z, **kw):
        """Street furniture; anything taller than a car stays off the ground the route buildings stand on."""
        if name in tall and any(c[0] - 250 <= x <= c[2] + 250 and c[1] - 250 <= y <= c[3] + 250 for c in cores):
            return
        props.add(name, x, y, z, **kw)

    buildings = 0

    # ---- lots: buildings on the plots, open plots become small squares or car parks
    for lx, lw in lots_x:
        for ly, lh in lots_y:
            iw, ih = lw - 2, lh - 2
            rows = [(ly + 1, ih)] if ih <= 4 else [(ly + 1, ih // 2), (ly + 1 + ih // 2, ih - ih // 2)]
            for py, ny in rows:
                px = lx + 1
                for nx in _split(rng, iw):
                    plot = [(i, j) for i in range(px, px + nx) for j in range(py, py + ny)]
                    world = (px * G - 200, py * G - 200, (px + nx) * G - 200, (py + ny) * G - 200)
                    if all(c in cells for c in plot) and _clear(world) and rng.random() < 0.9:
                        city_block(fronts, rng, px * G, py * G, nx, ny, rng.choice(HEIGHTS))
                        built.update(plot)
                        buildings += 1
                    elif all(c in cells for c in plot):
                        parking = rng.random() < 0.5
                        for (i, j) in plot:
                            r = rng.random()
                            if parking and r < 0.6:
                                stand("Van_Parked" if rng.random() < 0.2 else "Car_Parked", i * G + rng.uniform(-40, 40),
                                          j * G, 15.0, yaw=rng.choice((0.0, 180.0)) + rng.uniform(-4, 4),
                                          tint=rng.randrange(CAR_TINTS), cull=26000.0)
                            elif not parking and r < 0.4:
                                stand("Tree_Planter", i * G + rng.uniform(-60, 60), j * G + rng.uniform(-60, 60), 15.0,
                                          yaw=rng.uniform(0, 360), cull=30000.0)
                            elif not parking and r < 0.6:
                                stand("Bench_Street", i * G + rng.uniform(-80, 80), j * G + rng.uniform(-80, 80), 15.0,
                                          yaw=90.0 * rng.randrange(4), cull=14000.0)
                    px += nx

    # ---- ground: roads, crossings, pavements, kerbs, and what stands on them
    for (i, j) in sorted(cells):
        x, y = i * G, j * G
        rx, ry = i in road_x, j in road_y
        if rx and ry:
            ground.add("Road_Plain_4x4", x, y, yaw=90.0 * ((i + j) % 4), shadow=False)
            continue
        if rx or ry:
            if ry:      # street along X; the kerb is on -Y for the lower row, +Y for the upper
                d = (0, -1) if (j - 1) not in road_y else (0, 1)
                crossing = (i - 1) in road_x or (i + 1) in road_x
            else:
                d = (-1, 0) if (i - 1) not in road_x else (1, 0)
                crossing = (j - 1) in road_y or (j + 1) in road_y
            yaw = face(*d)
            ground.add("Road_Crossing_4x4" if crossing else "Road_Lane_4x4", x, y, yaw=yaw, shadow=False)
            if not crossing and rng.random() < 0.2:
                van = rng.random() < 0.22
                stand("Van_Parked" if van else "Car_Parked", x + d[0] * (62 if van else 75), y + d[1] * (62 if van else 75),
                          0.0, yaw=yaw - 90.0 + (180.0 if rng.random() < 0.15 else 0.0), tint=rng.randrange(CAR_TINTS),
                          cull=26000.0)
            continue
        if (i, j) in built:
            continue
        ground.add("Pavement_4x4", x, y, yaw=90.0 * ((i * 7 + j * 3) % 4), shadow=False)
        sides = [d for d in ((1, 0), (-1, 0), (0, 1), (0, -1))
                 if (d[0] and (i + d[0]) in road_x) or (d[1] and (j + d[1]) in road_y)]
        for d in sides:
            ground.add("Kerb_4m", x + d[0] * 200, y + d[1] * 200, yaw=face(*d), shadow=False)
        if len(sides) == 2:
            sx, sy = sides[0][0] + sides[1][0], sides[0][1] + sides[1][1]
            ground.add("Kerb_Corner", x + sx * 200, y + sy * 200, yaw=CORNER_YAW[(sx, sy)], shadow=False)
            if rng.random() < 0.6:
                stand("Traffic_Light", x + sx * 140, y + sy * 140, 15.0, yaw=face(*sides[rng.randrange(2)]) + 180.0,
                          cull=22000.0)
        elif len(sides) == 1:
            d = sides[0]
            n = i if d[1] else j
            r = rng.random()
            if n % 4 == 0:
                stand("Street_Lamp", x + d[0] * 150, y + d[1] * 150, 15.0, yaw=face(*d), cull=30000.0)
            elif n % 4 == 2 and r < 0.8:
                stand("Tree_Planter", x + d[0] * 105, y + d[1] * 105, 15.0, yaw=rng.uniform(0, 360), cull=30000.0)
            elif r < 0.10:
                stand("Bus_Stop", x + d[0] * 30, y + d[1] * 30, 15.0, yaw=face(*d), cull=26000.0)
            elif r < 0.28:
                stand("Bench_Street", x - d[0] * 40, y - d[1] * 40, 15.0, yaw=face(*d), cull=14000.0)
            elif r < 0.40:
                stand("Trash_Bin", x + d[0] * 120, y + d[1] * 120 , 15.0, cull=12000.0)
            elif r < 0.48:
                stand("Fire_Hydrant", x + d[0] * 150, y + d[1] * 150, 15.0, yaw=face(*d), cull=12000.0)
    # ---- the waterfront: wherever a land cell meets the sea, a quay wall drops into the water, with a kerb
    # on its edge; open pavement and road ends get a railing, and a lantern every third cell.
    quay, quays = Scenery("CityQuay"), []
    for (i, j) in sorted(cells):
        x, y = i * G, j * G
        for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + d[0] * G, y + d[1] * G
            if (i + d[0], j + d[1]) in cells or not any(s[0] <= nx <= s[2] and s[1] <= ny <= s[3] for s in seas):
                continue
            ex, ey = x + d[0] * 200, y + d[1] * 200
            along_x = d[1] != 0
            quays.append(unreal.Transform(
                location=unreal.Vector(ex - d[0] * 40, ey - d[1] * 40, (QUAY_TOP + QUAY_BOTTOM) / 2),
                rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0 if along_x else 90.0),
                scale=unreal.Vector(4.04, 0.8, (QUAY_TOP - QUAY_BOTTOM) / 100.0)))
            ground.add("Kerb_4m", ex, ey, yaw=face(*d), shadow=False)
            if (i, j) in built or any(b[0] <= x <= b[2] and b[1] <= y <= b[3] for b in lanes):
                continue
            for k in (-1, 1):
                ox, oy = (k * 100, 0) if along_x else (0, k * 100)
                quay.add("Dock_Railing_2m", ex - d[0] * 22 + ox, ey - d[1] * 22 + oy, 15.0,
                         yaw=0.0 if along_x else 90.0, pack="HarborCafePack", cull=20000.0)
            if (i + j) % 3 == 0:
                quay.add("Dock_Lantern", ex - d[0] * 70, ey - d[1] * 70, 15.0, yaw=face(*d), pack="HarborCafePack",
                         cull=26000.0)
    for s in (ground, fronts, props, quay):
        s.commit()
    if quays:
        walls = spawn(unreal.RomanceCityBlock, Frame(), 0, 0, 0, label="CityQuayWall", folder="City")
        walls.add_pieces(load_mesh("/Engine/BasicShapes/Cube"), city_material("Quay"), quays, True, 0.0)
    unreal.log("Waterfront: %d quay segments" % len(quays))
    unreal.log("City: %d buildings, %d ground tiles, %d facade pieces, %d street pieces" % (
        buildings, ground.total, fronts.total, props.total))
    return buildings


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


def skyline(rng, reach=22):
    """The background city around every lane(): see city()."""
    return city(rng, reach)


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
                    block(self.f, ix * CELL, iy * CELL, fl, tags=tags, folder=folder)
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
                    block(self.f, x, iy * CELL, fl, folder="Route/Roofs")
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
