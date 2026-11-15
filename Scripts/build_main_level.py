"""Build /Game/Maps/L_AnyRomance: the full run.

  Route 1 (east):  city rooftops        -> Date 1, Ari,   city cafe on a rooftop     (CafeAssetPack)
  Route 2 (north): rooftops down        -> Date 2, Bea,   harbor cafe at the water   (HarborCafePack)
  Route 3 (west):  rooftops climbing    -> Date 3, Jules, chalet cafe on a snowy terrace (MountainCafePack)

The three venues are the packs' own demo layouts (scripts/*_scene.py in each pack), placed as they are.
Each route is a list of segments from route_kit.Course; edit the lists below to reshape a route.

Headless (editor closed):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
Re-running rebuilds the map from scratch, so hand edits to L_AnyRomance are lost.
"""
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_kit as kit
from route_kit import CELL, K, Course, Frame, checkpoint, mesh, point, put, walk_z, warm_light

MAP = "/Game/Maps/L_AnyRomance"
CAFE, HARBOR, MOUNTAIN = "CafeAssetPack", "HarborCafePack", "MountainCafePack"

# Deadlines are set from route length: a clean run is taken as RUN_SPEED along the course, plus MARGIN.
# REPLY_BUDGET is the time assumed spent answering at the previous date (four replies of 15-45 s each).
RUN_SPEED = 400.0     # cm/s averaged over a route, obstacles included (an estimate until the routes are timed)
MARGIN = 1.3
REPLY_BUDGET = 150.0

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)


def date_character(frame, index, name, x, y, z, yaw):
    """The date, standing at the spot. Tagged DateActorN so the game hides them once they give up waiting.
    The character meshes face +Y; `yaw` is the way they should look, in the venue's frame."""
    actor = kit.mesh(frame, "/Game/Characters/SM_%s" % name, x, y, z, yaw=yaw - 90, scale=(0.92, 0.92, 0.92),
                     folder="Course", tags=("DateActor%d" % index,), collide=False)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.static_mesh_component.set_collision_profile_name("NoCollision")
    return actor


def date_spot(x, y, yaw, ahead=110.0):
    """Where the runner meets a date standing at (x, y) and looking along yaw: a step in front of them."""
    return x + ahead * math.cos(math.radians(yaw)), y + ahead * math.sin(math.radians(yaw))


# ================================================================ venues
def venue_city_cafe(f):
    """Demo_Cafe's 6 x 4 m room, closed into a real cafe on the rooftop: four walls with the door facing the
    arriving runner (-X), a ceiling and roof, and a terrace with tables and a parapet around the roof.
    Blender coordinates in metres (put() turns them into Unreal ones)."""
    def p(name, x, y, z=0.0, rz=0.0, s=1.0, collide=True):
        return put(f, CAFE, name, x, y, z, rz, s, folder="Venue/CityCafe", collide=collide)

    for x in (-2, 0, 2):
        for y in (-1, 1):
            p("Floor_Tile_2x2" if x == 2 else "Floor_Wood_2x2", x, y)
            p("Floor_Wood_2x2", x, y, 2.83, s=(1, 1, -1))        # ceiling: the same boards, face down
    # Walls: windows on the back and on the terrace front, the door wall facing the arrival, a blank wall
    # behind the bar. Wall fronts face the room.
    p("Wall_Window_2m", -2, 2.06)
    p("Wall_Plain_2m", 0, 2.06)
    p("Wall_Plain_2m", 2, 2.06)
    for x in (-2, 0, 2):
        p("Wall_Window_2m", x, -2.06, rz=180)
    p("Wall_Door_2m", -3.06, -1, rz=90)
    p("Wall_Window_2m", -3.06, 1, rz=90)
    p("Door_Glass", -3.06, -1.465, 0.005, rz=0, collide=False)   # swung fully open, flat out of the doorway
    for y in (-1, 1):
        p("Wall_Plain_2m", 3.06, y, rz=-90)
    # Roof: two of the parkour pack's roof tiles, widened to cover the walls, with an eave over the short sides.
    for x in (-2, 2):
        put(f, kit.PARKOUR, "Roof_Tile_4x4", x, 0, 3.12, s=(1, 1.16, 1), folder="Venue/CityCafe")

    # Bar
    p("Counter_Module", 2.2, 0.3)
    p("Display_Case", 1.03, 0.3)
    p("Counter_Module", 2.2, 1.62, rz=180)
    p("Espresso_Machine", 2.42, 1.62, 1.05)
    p("Coffee_Grinder", 1.88, 1.62, 1.05)
    p("Kettle_Gooseneck", 1.68, 1.55, 1.05, rz=70)
    p("POS_Register", 2.50, 0.32, 1.05, rz=180)
    p("Napkin_Sugar_Set", 1.85, 0.12, 1.05)
    p("Takeaway_Cup", 2.10, 0.42, 1.05)
    p("PourOver_Set", 1.80, 0.50, 1.05)
    p("Stool_Bar", 1.90, -0.42)
    p("Stool_Bar", 2.55, -0.42)
    p("Pendant_Lamp", 1.75, 0.3, 2.8)
    p("Pendant_Lamp", 2.65, 0.3, 2.8)
    p("Wall_Shelf", 2.2, 2.0, 1.78)
    p("Coffee_Bag", 1.90, 1.89, 1.695)
    p("Coffee_Bag", 2.05, 1.89, 1.695, rz=-12)
    p("Syrup_Bottles", 2.45, 1.89, 1.695)
    p("Mug", 1.90, 1.89, 2.055, rz=30)
    p("Plant_Snake", 2.50, 1.89, 2.055, s=0.6)
    p("Wall_Menu", 0.2, 2.0, 1.9)
    p("Wall_Sign", 1.25, 2.0, 2.2)
    # Lounge by the back window
    p("Rug_Round", -1.9, 0.75)
    p("Sofa_Bench", -1.9, 1.5)
    p("Table_Round", -1.9, 0.75, 0.014)
    p("Cup_Saucer", -2.05, 0.80, 0.764)
    p("Croissant_Plate", -1.78, 0.68, 0.764)
    p("Plant_Fiddle", -0.72, 1.62)
    p("Pendant_Lamp", -1.9, 0.75, 2.8)
    # The table set for two by the front window, where Ari waits
    p("Table_Square", 0.0, -1.0)
    p("Chair_Cafe", -0.62, -1.0, rz=90)
    p("Chair_Cafe", 0.62, -1.0, rz=-90)
    p("Iced_Latte", -0.15, -0.92, 0.75)
    p("Cake_Slice", 0.14, -1.08, 0.75, rz=25)
    p("Pendant_Lamp", 0.0, -1.0, 2.8)
    p("Plant_Snake", -2.65, -1.72)
    # Outside: the sign over the door, plants by it, the menu board for passers-by
    p("Wall_Sign", -3.20, 0.0, 2.3, rz=-90)
    p("Plant_Fiddle", -3.55, -2.05)
    p("Plant_Fiddle", -3.55, 2.05)
    p("Menu_AFrame", -3.9, 0.15, rz=-70)
    # Terrace behind the cafe (+Y in Blender) and along its far side
    for x in (-1.6, 1.6):
        p("Table_Round", x, 4.2)
        p("Chair_Cafe", x - 0.62, 4.2, rz=90)
        p("Chair_Cafe", x + 0.62, 4.2, rz=-90)
    p("Cup_Saucer", -1.65, 4.25, 0.75)
    p("Iced_Latte", 1.55, 4.15, 0.75)
    p("Table_Square", 4.8, 0.0)
    p("Chair_Cafe", 4.8, 0.62, rz=180)
    p("Chair_Cafe", 4.8, -0.62)
    for x, y in ((-4.2, 5.4), (4.2, 5.4), (5.6, 2.2), (5.6, -2.2), (-4.2, -5.4), (4.2, -5.4)):
        p("Plant_Fiddle", x, y)
    # Parapet round the roof (5 x 5 cells, 20 m), open where the runner lands (-X) and where they leave for
    # the harbor (+Y in Unreal).
    for c in (-800, -400, 0, 400, 800):
        for side in (-1, 1):
            if not (side < 0 and c == 0):
                mesh(f, "Parapet_4m", side * 981, c, -2, yaw=90, folder="Venue/CityCafe")
            if not (side > 0 and c == 0):
                mesh(f, "Parapet_4m", c, side * 981, -2, folder="Venue/CityCafe")
    warm_light(f, 0.0, -1.0, 2.3, intensity=2.2, radius=450)
    warm_light(f, 2.2, 0.9, 2.3, intensity=2.2, radius=450)
    warm_light(f, -1.9, 0.75, 2.3, intensity=2.0, radius=420)


def venue_harbor(f):
    """Demo_Harbor: a 6 x 4 m dock with a kiosk; the open sea-view side is local +Y. A pier is added on that side."""
    KX, KY, KDY = 1.3, 0.9, 0.3135

    def p(name, x, y, z=0.0, rz=0.0, s=(1, 1, 1), collide=True):
        return put(f, HARBOR, name, x, y, z, rz, s, folder="Venue/Harbor", collide=collide)

    def span(name, p0, p1, n):
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        length = math.hypot(dx, dy)
        for i in range(n):
            t = (i + 0.5) / n
            p(name, p0[0] + dx * t, p0[1] + dy * t, p0[2], rz=math.degrees(math.atan2(dy, dx)),
              s=(length / (2 * n), 1, 1), collide=False)

    # The water is the open sea laid at the end of the script, 1.6 m under the deck.
    for x in (-2, 0, 2):
        for y in (-1, 1):
            p("Dock_Platform_2x2", x, y)
        for y in (-3, -5, -7, -9):          # pier toward the arriving route
            p("Dock_Platform_2x2", x, y)
    for x in (-3.05, -1.0, 1.0, 3.05):
        for y in (-2.05, 2.05, -6.0, -10.05):
            p("Dock_Piling", x, y, -0.14, s=(1, 1, 1.25))     # longer, down into the lower water
    p("Dock_Railing_2m", -2, 1.93)
    p("Dock_Railing_2m", -2.93, -1, rz=90)
    p("Dock_Railing_2m", -2.93, 1, rz=90)
    p("Dock_Railing_2m", 2.93, -1, rz=90)
    for y in (-3, -5, -7):                  # along the pier, open at its end where the runner lands
        for x in (-2.93, 2.93):
            p("Dock_Railing_2m", x, y, rz=90)
    p("Dock_Lantern", -2.8, -6.0, rz=180)
    p("Dock_Lantern", 2.8, -6.0, rz=180)
    p("Bollard", -2.6, -9.8, rz=180)
    p("Bollard", 2.6, -9.8, rz=180)
    p("Kiosk_Shack", KX, KY - KDY)
    cz = 1.18
    p("Caydanlik", KX - 0.85, KY - 1.14, cz, rz=20)
    p("Tea_Glass", KX - 0.50, KY - 1.22, cz)
    p("Tea_Glass", KX - 0.37, KY - 1.12, cz, rz=60)
    p("Simit_Plate", KX - 0.02, KY - 1.17, cz)
    p("Fish_Sandwich", KX + 0.42, KY - 1.17, cz, rz=10)
    p("Enamel_Mug", KX + 0.80, KY - 1.14, cz, rz=-30)
    p("Ice_Cream_Cone", KX + 1.12, KY - 1.15, cz)
    fy = KY - 1.012
    p("Ship_Wheel", KX - 0.80, fy, 0.60)
    p("Sign_Harbor_Cafe", KX + 0.65, fy, 0.52)
    lx = KX - 1.512
    p("Lifebuoy", lx, KY - 0.55, 1.50, rz=-90)
    p("Menu_Board_Catch", lx, KY + 0.30, 1.35, rz=-90)
    p("Anchor", lx - 0.06, KY + 0.86, 0.0, rz=-90)
    p("Buoy_Cluster", KX - 1.66, KY - 1.05, 2.40, collide=False)
    p("Seagull", KX - 0.7, KY, 2.995, rz=160)
    p("Barrel", -0.72, 1.62)
    p("Crate_Wood", -0.75, 0.95, rz=8)
    p("Crate_Wood", -0.73, 0.96, 0.30, rz=-14)
    p("Rope_Coil", -1.25, 1.62, rz=200)
    p("Cooler_Chest", -0.72, 0.18, rz=-90)
    p("Parasol", -1.55, 1.50)
    p("Table_Bistro", -1.95, 0.80)
    p("Chair_Bistro", -2.50, 0.80, rz=90)
    p("Chair_Bistro", -1.40, 0.80, rz=-90)
    p("Lemonade", -2.08, 0.86, 0.75)
    p("Fish_And_Chips", -1.86, 0.74, 0.75, rz=25)
    p("Tea_Glass", -1.92, 0.98, 0.75)
    p("Planter_Seagrass", -2.42, 1.70)
    p("Bench_Dock", -2.62, -0.62, rz=90)
    p("Lifebuoy", -2.86, 0.02, 0.60, rz=90)
    p("Deck_Chair", 0.15, -1.08)
    p("Deck_Chair", 1.30, -1.08)
    p("Crate_Wood", 0.73, -1.25, rz=90)
    p("Lemonade", 0.73, -1.22, 0.30)
    p("Fish_Grill", 2.28, -1.05)
    p("Fish_Crate", 2.20, -1.70, rz=-6)
    p("Seagull", 2.93, -1.9, 1.04, rz=120)
    p("Dock_Lantern", -2.8, -1.62, rz=180)
    p("Dock_Lantern", 2.8, -1.62, rz=180)
    span("String_Lights_2m", (-2.8, -1.8, 2.52), (2.8, -1.8, 2.52), 3)
    span("Bunting_2m", (-2.8, -1.78, 2.40), (KX - 1.55, KY - 1.05, 2.40), 2)
    p("Rowboat", -4.6, -3.15, -1.72, rz=7, collide=False)
    warm_light(f, -2.8, -1.62, 2.3, intensity=1.5, radius=400)
    warm_light(f, 2.8, -1.62, 2.3, intensity=1.5, radius=400)


def venue_mountain(f, base_floors):
    """Demo_Mountain: snowy terraces with an 8 x 6 m chalet. The lower terrace and the way in are local +Y."""
    G = 2.0
    F = G + 0.25
    CX, CY = -1.0, 4.0

    def p(name, x, y, z=0.0, rz=0.0, s=1.0, collide=True):
        return put(f, MOUNTAIN, name, x, y, z, rz, s, folder="Venue/Mountain", collide=collide)

    cols = (-10, -6, -2, 2, 6, 10)
    rows = (-10, -6, -2, 2, 6, 10, 14)
    for x in cols:
        p("Snow_Ground_4x4", x, -10, 0)
        p("Snow_Slope_4x4" if x in (-2, 2) else "Snow_Ground_4x4", x, -6, 0)
        for y in (-2, 2, 6):
            p("Cliff_Block_4x4", x, y, 0)
        p("Cliff_Block_4x4", x, 10, -0.3)
        p("Snow_Slope_4x4", x, 10, G)
        p("Cliff_Block_4x4", x, 14, 0)
        p("Cliff_Block_4x4", x, 14, 2)
    # Rock under the terraces, down to street level, around the edge only.
    depth = int(math.ceil(walk_z(base_floors) / 200.0)) + 1      # one more, down into the sea on the north side
    for x in cols:
        for y in rows:
            if x in (cols[0], cols[-1]) or y in (rows[0], rows[-1]):
                for k in range(1, depth + 1):
                    put(f, MOUNTAIN, "Cliff_Block_4x4", x, y, -2.0 * k - 0.05, folder="Venue/Mountain/Rock")

    for sx in (-1, 1):
        for sy in (-1, 1):
            p("Floor_Wood_4x3", CX + sx * 2, CY + sy * 1.5, F)
            p("Chalet_Corner_Post", CX + sx * 4, CY + sy * 3, G)
        p("Chalet_Wall_4m", CX + sx * 2, CY + 3, G, rz=180)
        p("Chalet_Gable_6m", CX + sx * 4, CY, G, rz=sx * 90)
        p("Chalet_Roof_4m", CX + sx * 2, CY, G)
    p("Chalet_Wall_Door", CX - 2, CY - 3, G)
    p("Chalet_Wall_Window", CX + 2, CY - 3, G)
    p("Chalet_Door", CX - 2 - 0.54, CY - 3, F, rz=95, collide=False)    # open flat against the wall
    p("Chimney_Stone", CX - 1.5, CY + 2.1, G + 3.2)
    fy = CY - 3 - 0.15
    p("Deck_Wood_4x4", CX - 2, CY - 5, F)
    p("Deck_Wood_4x4", CX + 2, CY - 5, F)
    p("Deck_Step", CX - 2, CY - 7.22, G)
    p("Lantern_Hanging", CX - 3.25, fy, G + 2.05)
    p("Lantern_Hanging", CX + 3.75, fy, G + 2.05)
    p("Wreath", CX - 2, fy, G + 2.72)
    p("Skis_Pair", CX - 0.55, fy, F)
    for i in range(4):
        p("Fairy_Lights_2m", CX - 3 + i * 2, CY - 3.62, G + 2.62, collide=False)
    p("Table_Rustic", CX + 1.6, CY - 5.2, F)
    p("Bench_Rustic", CX + 1.6, CY - 5.8, F)
    p("Bench_Rustic", CX + 1.6, CY - 4.6, F, rz=180)
    p("Lantern_Table", CX + 1.15, CY - 5.2, F + 0.77)
    p("Hot_Chocolate", CX + 1.75, CY - 5.35, F + 0.77, rz=40)
    p("Roasted_Chestnuts", CX + 2.0, CY - 5.05, F + 0.77)
    p("Menu_Board_Chalet", CX + 4.6, CY - 7.4, G, rz=-18)
    p("Firewood_Stack", CX - 4.45, CY - 1.0, G, rz=90)
    p("Axe_Stump", CX - 5.9, CY - 2.8, G)
    p("Ski_Rack", CX + 4.7, CY + 0.3, G, rz=90)
    p("Fire_Pit", 7.6, -0.6, G)
    p("Stool_Log", 6.45, -1.0, G)
    p("Stool_Log", 8.3, -1.5, G)
    p("Stool_Log", 8.5, 0.35, G)
    p("Snowman", -7.6, -1.6, G, rz=25)
    p("Sled_Wood", -6.4, -8.6, 0, rz=30)
    p("Signpost_Mountain", 4.9, -7.6, 0, rz=-10)
    for x in (-10, -6, 6, 10):
        p("Fence_Wood_4m", x, -3.8, G)
    for x, y, z, s in ((-9, 13.4, 4, 1.0), (-4.6, 14.6, 4, 1.1), (0.4, 13.2, 4, 0.95), (4.8, 14.7, 4, 1.05),
                       (9.4, 13.3, 4, 1.0), (9.8, 5.6, G, 0.9), (-10.2, 5.2, G, 0.95)):
        p("Pine_Tree_Snowy", x, y, z, rz=x * 40, s=s)
    for x, y, z in ((10.6, 1.6, G), (-10.6, -1.0, G), (8.2, -9.6, 0), (-9.6, -9.2, 0), (-10.8, -6.2, 0), (7.0, 14.2, 4)):
        p("Pine_Small_Snowy", x, y, z, rz=x * 25)
    p("Rock_Snowy_Large", 10.2, -6.6, 0, rz=20)
    p("Rock_Snowy_Large", -8.2, 7.0, G, rz=140)
    p("Rock_Snowy_Small", 5.2, -9.9, 0)
    p("Rock_Snowy_Small", 9.0, 3.2, G, rz=60)
    p("Snow_Drift", -4.6, -9.6, 0)
    p("Snow_Drift", 9.4, -2.0, G, rz=70)
    p("Snow_Drift", -8.6, 2.2, G, rz=-30)
    p("Stone_Fireplace", CX - 1.5, CY + 2.2, F)
    p("Rug_Plaid", CX - 1.5, CY + 0.5, F)
    p("Armchair_Cozy", CX - 2.4, CY + 0.25, F, rz=160)
    p("Armchair_Cozy", CX - 0.6, CY + 0.25, F, rz=200)
    p("Blanket_Basket", CX - 3.35, CY + 1.7, F)
    p("Coat_Rack", CX - 3.45, CY - 2.35, F)
    p("Counter_Chalet", CX + 2.4, CY + 1.9, F)
    p("Samovar", CX + 1.75, CY + 1.9, F + 1.06)
    p("Salep_Cup", CX + 2.35, CY + 1.75, F + 1.06)
    p("Mulled_Drink", CX + 2.7, CY + 1.95, F + 1.06)
    p("Tea_Glass", CX + 3.05, CY + 1.75, F + 1.06)
    p("Shelf_Rustic", CX + 2.4, CY + 2.85, F + 1.6)
    p("Hot_Chocolate", CX + 2.1, CY + 2.72, F + 1.6225)
    p("Lantern_Table", CX + 2.75, CY + 2.72, F + 1.6225)
    p("Stove_Iron", CX + 3.25, CY - 2.2, F)
    p("Table_Rustic", CX + 1.4, CY - 0.6, F)
    p("Chair_Chalet", CX + 1.0, CY - 1.3, F, rz=180)
    p("Chair_Chalet", CX + 1.8, CY - 1.3, F, rz=180)
    p("Chair_Chalet", CX + 1.0, CY + 0.1, F)
    p("Chair_Chalet", CX + 1.8, CY + 0.1, F)
    p("Fondue_Pot", CX + 1.4, CY - 0.6, F + 0.77)
    p("Soup_Bowl", CX + 0.9, CY - 0.75, F + 0.77, rz=10)
    p("Strudel_Plate", CX + 1.95, CY - 0.45, F + 0.77)
    p("Chandelier_Wheel", CX, CY, G + 4.75)
    # The pack's preview lights: fireplace, chandelier, fire pit, door.
    warm_light(f, CX - 1.5, CY + 1.3, F + 0.6, intensity=3.0, radius=600)
    warm_light(f, CX, CY, G + 3.4, intensity=3.0, radius=700)
    warm_light(f, 7.6, -0.6, G + 0.6, intensity=2.0, radius=500)
    warm_light(f, CX - 2, CY - 3.6, G + 1.8, intensity=1.5, radius=400)
    # Snowy peaks behind and beside the terraces: the pack's large rock, scaled up, standing on the street.
    ground = -walk_z(base_floors) / 100.0 - 3.0      # bases under the sea and the street
    for x, y, s, rz in ((-34, 62, 34, 20), (18, 78, 44, 140), (62, 40, 30, 250), (-70, 20, 26, 80),
                        (48, -6, 18, 310), (-52, -14, 16, 200), (0, 120, 56, 60)):
        put(f, MOUNTAIN, "Rock_Snowy_Large", x, y, ground, rz=rz, s=s, folder="Venue/Mountain/Peaks", collide=False)
    return (CX - 2.0, CY - 1.4, F)      # 1.6 m straight in from the door, seen through it on the way in


# ================================================================ route 1: city rooftops, heading +X
A = Frame(0, 0, 0, 0)
r1 = Course(A, 0, 1, "city")
r1.run(3, props=False)
point(A, K.START, 0, 0, r1.z, hint="SPACE jumps; hold it to keep hopping. Ari is waiting at the city cafe.")
mesh(A, "Start_Arch", 300, 0, r1.z, yaw=90)
r1.run(2).vault().gap(1).run(3).cp("First gap", "Hold CTRL while running to slide under the gate.")
r1.slide_gate().run(2).hint("Jump beside the tall wall while holding W to wall-run. SPACE kicks off.")
r1.wallrun(3, side=1).run(2).cp("Wall run", "Climb between the two walls: SPACE against each wall in turn.")
r1.shaft_up().run(3).cp("Shaft")
r1.gap(1).run(2).hint("Throw a charge (RMB or Q) at the red switch to open the gate.")
r1.gate().run(2).cp("Gate", "Aim at the purple ring on the crane and hold LMB to swing. SPACE lets go.")
r1.sling(5, side=1).run(3).cp("Crane", "Slide down the slope, then jump on the flat to clear the wide gap.")
r1.slope_down().run(1, props=False).gap(2).run(3).cp("Slope", "A charge at your feet is a rocket jump. Use it on the bare wall.")
r1.gap(1).run(2).rocket_up().run(3).cp("Rocket jump")
r1.slope_down().run(1, props=False).gap(2).run(3)
r1.wallrun(3, side=-1).run(2).bridge(2).run(2).cp("Planks", "Step on the pad to reach the cafe roof.")
r1.pad_up()

# City cafe on a 5 x 5 cell roof; the room's door faces the arriving runner.
c0 = r1.ix
r1.roof(c0, c0 + 4, lane=(-2, -1, 0, 1, 2))
cafe = A.child((c0 + 2) * CELL, 0, r1.z + 2, 0)
venue_city_cafe(cafe)
checkpoint(A, c0 * CELL, 0, r1.z, "City cafe", ring=False)
# Ari waits at the table for two inside, facing the door.
sx, sy = date_spot(-145, 100, 180)
ari = point(cafe, K.DATE, sx, sy, 0, yaw=180, order=0, label="City Cafe", radius=350,
            hint="Ari is waiting inside the cafe. Go in and press E.")
date_character(cafe, 0, "Ari", -145, 100, 0, yaw=180)
r1_cells = c0 + 2

# ================================================================ route 2: down to the harbor, heading +Y
B = A.child((c0 + 2) * CELL, 0, 0, 90)
r2 = Course(B, 3, 2, "harbor")
r2.run(2).hint("Bea is at the harbor cafe, down by the water.", dx=-300).gap(1).run(2).cp("Leaving the cafe")
r2.wallrun(3, side=1).run(2).cp("Long wall", "A charge at your feet is a rocket jump. Use it on the bare wall.")
r2.rocket_up().run(3).cp("High roof")
r2.gap(1).run(2).slide_gate().run(2)
r2.sling(6, side=-1).run(3).cp("Second crane")
r2.slope_down().run(1, props=False).gap(2).run(3).cp("Second slope")
r2.slide_gate().gap(1).run(2).gate().run(2).cp("Harbor gate")
r2.wallrun(3, side=1).run(2).shaft_up().run(3).cp("Harbor shaft")
r2.slope_down().run(1, props=False).gap(2).run(3).cp("Last slope")
r2.bridge(2).run(2).wallrun(3, side=-1).run(2).cp("Above the harbor", "Drop to the pier. Bea is on the dock.")

# Harbor: the pier's end sits 1 m past the last roof edge, 3.2 m below it.
DOCK_Z = 40.0         # deck height; the sea is 1.6 m below it (z -120), under the street and the quay
harbor = B.child((r2.ix * CELL - 200) + 100 + 1000, 0, DOCK_Z, 90)
venue_harbor(harbor)
checkpoint(harbor, 0, 700, 0, "Pier", ring=False, yaw=-90)
# Bea waits by the bistro table under the parasol, facing the pier the runner comes down.
sx, sy = date_spot(-195, 15, 90)
bea = point(harbor, K.DATE, sx, sy, 0, yaw=90, order=1, label="Harbor Cafe", radius=350,
            hint="Bea is waiting on the dock by the parasol. Walk up and press E.")
date_character(harbor, 1, "Bea", -195, 15, 0, yaw=90)
r2_cells = r2.cells() + 4

# ================================================================ route 3: up to the mountain, heading -X
# The coast runs along harbor-local y = 1100, where route 2's last roof ends. Route 3 starts on land behind it,
# 12 m clear of route 2 so the harbor can't be skipped by stepping across, and runs along the waterfront.
COAST = 1100.0
hx, hy, _ = harbor.at(2000, 1700)
C = Frame(hx, hy, 0, harbor.yaw)
r3 = Course(C, 0, 1, "mountain")
# Jump pad on the pier launches up to the first roof; a rocket jump works too.
put(harbor, kit.PARKOUR, "Jump_Pad", 2.0, -5.0, 0.0, folder="Route")
pad = point(harbor, K.PAD, 200, 500, 0, radius=70, name="PierPad",
            hint="Jules is up the mountain. The pad on the pier gets you back to the roofs.")
pad.set_editor_property("pad_target", unreal.Vector(2000, 1200, r3.z - DOCK_Z))   # route 3's first roof
r3.run(3).cp("Back on the roofs").slide_gate().run(2).gap(1).run(2)
r3.shaft_up().run(3).cp("Second shaft")
r3.wallrun(3, side=1).run(2).gate().run(2).cp("Mountain gate")
r3.rocket_up().run(3).sling(6, side=1).run(3).cp("Third crane")
r3.gap(1).run(2).slope_down().run(1, props=False).gap(2).run(3).cp("Mountain slope")
r3.rocket_up().run(3)
r3.slide_gate().run(1, props=False).bridge(2).run(2).wallrun(3, side=-1).run(2).cp("Last wall")
r3.shaft_up().run(3).gap(1).run(2).cp("Below the snow", "Swing from the last crane onto the snow.")
r3.sling(5, side=-1)

# Mountain: the lower terrace's front edge starts where the next roof would.
mountain = C.child((r3.ix * CELL - 200) + 1200, 0, r3.z, 90)
spot = venue_mountain(mountain, r3.floors)
checkpoint(mountain, 0, 900, 0, "Snow terrace", ring=False, yaw=-90)
# Jules waits just inside the chalet, turned toward its open door; the runner meets her a step in front.
jx, jy = spot[0] * 100, -spot[1] * 100
door = (-300.0, -100.0)                 # the chalet doorway (Blender (-3, 1)) in Unreal coordinates
jyaw = math.degrees(math.atan2(door[1] - jy, door[0] - jx))
sx, sy = date_spot(jx, jy, jyaw)
jules = point(mountain, K.DATE, sx, sy, spot[2] * 100, yaw=jyaw, order=2,
              label="Mountain Lodge", radius=350, hint="Jules is inside the chalet. Go in and press E.")
date_character(mountain, 2, "Jules", jx, jy, spot[2] * 100, yaw=jyaw)
r3_cells = r3.cells() + 5

# ================================================================ schedule
times = [cells * CELL / RUN_SPEED for cells in (r1_cells, r2_cells, r3_cells)]
deadline = 0.0
for i, (date, t) in enumerate(zip((ari, bea, jules), times)):
    deadline += (REPLY_BUDGET if i else 0.0) + t * MARGIN
    date.set_editor_property("deadline", float(round(deadline / 5.0) * 5))
    unreal.log("Route %d: %d m, clean run est. %.0f s, deadline %s" % (
        i + 1, (r1_cells, r2_cells, r3_cells)[i] * 4, t, date.get_editor_property("deadline")))

# ================================================================ the world around the routes
# Open sea behind the harbor: one instanced sheet of the pack's water tile, 8 m per tile, just under the
# harbor's own tiles. It stops 60 m along route 3 so that route runs back into the city.
# The sea lies on the harbor's open side of the coast, along the whole of route 3 and past the mountain.
# Its surface is 1.2 m under the street; the city meets it with a quay (kit.sea). The water sheet runs a
# little under the land so no gap shows at the quay.
SEA = (-30000.0, -40000.0, 46000.0, COAST)      # harbor-local x0, y0, x1, y1
WATER_Z = -160.0                                # harbor-local: world z -120
kit.instanced_grid(harbor, "Water_2x2", HARBOR, SEA[0] + 400, SEA[1] + 400, WATER_Z,
                   int((SEA[2] - SEA[0]) / 800), int(math.ceil((SEA[3] - SEA[1]) / 800)) + 1, 800.0, (4, 4, 1),
                   folder="Venue/Harbor")
for x, y, rz in ((-900, -1500, 30), (1400, -2600, 200), (-2600, -700, 110), (600, -5200, 75), (9000, -3000, 140),
                 (17000, -1800, 20)):
    put(harbor, HARBOR, "Rowboat", x / 100.0, -y / 100.0, WATER_Z / 100.0 - 0.12, rz=rz, folder="Venue/Harbor",
        collide=False)

# Background city: keep it off the three lanes, the venues, the sea and the mountain, then fill the rest.
# The city is laid out on one street grid around the three lanes (kit.city): CityPack facades, ground
# floors and roof edges on the lots, roads, pavements and street furniture between them.
kit.lane(A, -1200, -1500, (c0 + 5) * CELL + 400, 1500)
kit.lane(B, -1200, -1500, r2.ix * CELL + 400, 1500)
kit.lane(C, -1200, -1500, r3.ix * CELL + 400, 1500)
kit.keep_out(cafe, -1800, -1800, 1800, 1800)
kit.core(cafe, -1000, -1000, 1000, 1000)
kit.sea(harbor, SEA[0], SEA[1], SEA[2], SEA[3])
kit.void(harbor, -1800, -1800, 1800, COAST)
kit.void(mountain, -9000, -16000, 9000, 1300)      # up to the terrace front: the street and quay reach the rock
# The mountain's foot: snow at street level wherever the mountain zone is land (mountain-local x > -600 is
# south of the coast), and a row of snowy rock along the coast so the snow ends in a cliff, not in the air.
FOOT_Z = -r3.z                                  # mountain-local height of the street
kit.instanced_grid(mountain, "Snow_Ground_4x4", MOUNTAIN, -200, -15900, FOOT_Z, 12, 22, 800.0, (2, 2, 1),
                   folder="Venue/Mountain/Foot")
for k in range(44):
    mesh(mountain, "Cliff_Block_4x4", -800, -16100 + k * 400, FOOT_Z - 200, pack=MOUNTAIN,
         folder="Venue/Mountain/Foot", collide=False)
rng = random.Random(7)
city = kit.skyline(rng)
unreal.log("Background city: %d buildings" % city)

# ================================================================ sky, light, street
# The dark plane lies just under the road tiles and shows only beyond the city's edge.
mid_x, mid_y, _ = B.at(r2.ix * CELL / 2, 0)
street = mesh(Frame(), "/Engine/BasicShapes/Plane", mid_x - 12000, mid_y, -400, scale=(1400, 1400, 1), folder="Sky", collide=False)
street.static_mesh_component.set_material(0, unreal.EditorAssetLibrary.load_asset("/Game/Romance/M_Street"))
kit.spawn(unreal.VolumetricCloud, Frame(), 0, 0, 0, label="Clouds", folder="Sky")

# Falling: the street, the quay and the sea have no floor. A runner whose feet drop below this line has
# fallen and goes back to the last checkpoint (ARomanceMode reads the FallLine tag).
point(Frame(), K.MARKER, 0, 0, -30, tags=("FallLine",), name="FallLine")

sun = kit.spawn(unreal.DirectionalLight, Frame(), 0, 0, 3000, yaw=35, pitch=-42, label="Sun", folder="Sky")
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_intensity(4.4)
sc.set_light_color(unreal.LinearColor(1.0, 0.92, 0.80))
sc.set_editor_property("atmosphere_sun_light", True)

sky = kit.spawn(unreal.SkyLight, Frame(), 0, 0, 3200, label="SkyLight", folder="Sky")
kc = sky.get_component_by_class(unreal.SkyLightComponent)
kc.set_mobility(unreal.ComponentMobility.MOVABLE)
kc.set_editor_property("real_time_capture", True)
kc.set_intensity(1.5)

kit.spawn(unreal.SkyAtmosphere, Frame(), 0, 0, 0, label="SkyAtmosphere", folder="Sky")

fog = kit.spawn(unreal.ExponentialHeightFog, Frame(), 0, 0, 200, label="Haze", folder="Sky")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
# Thicker near the street, so the city sinks into haze below the roofs and fades with distance.
fc.set_fog_density(0.03)
fc.set_fog_height_falloff(0.09)
fc.set_fog_inscattering_color(unreal.LinearColor(0.50, 0.66, 0.78))
fc.set_start_distance(2500.0)

# Same fixed exposure the runner's camera uses, so the editor view, the title camera and play all match.
ppv = kit.spawn(unreal.PostProcessVolume, Frame(), 0, 0, 0, label="Exposure", folder="Sky")
ppv.set_editor_property("unbound", True)
pp = ppv.get_editor_property("settings")
pp.set_editor_property("override_auto_exposure_method", True)
pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
pp.set_editor_property("override_auto_exposure_bias", True)
pp.set_editor_property("auto_exposure_bias", -0.15)
ppv.set_editor_property("settings", pp)

yaw, pitch = kit.aim((-900.0, -1500.0, 1400.0), (2600.0, 100.0, 500.0))
kit.spawn(unreal.CameraActor, Frame(), -900, -1500, 1400, yaw=yaw, pitch=pitch, label="TitleCamera",
          tags=("TitleCamera",), folder="Course")

ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.log("L_AnyRomance: %d actors, saved=%s" % (kit.count, ok))
unreal.log("VENUES cafe=%s harbor=%s mountain=%s" % (cafe.at(0, 0), harbor.at(0, 0), mountain.at(0, 0)))
