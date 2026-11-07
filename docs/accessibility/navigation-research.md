# Navigation research carried over from the previous project

Written 2026-11-07. This is an index and summary of the previous project's
navigation, movement, physics and collision research. It is the starting
point for the navigation rewrite. Nothing here is implemented yet.

## How to treat this material

- The old research is **leads, not truth**. It was done against an older
  copy of the decompilation and the old port (0.9.3 plus a diagnostic
  build). Every claim we rely on must be checked again against this
  repository's source before it shapes code.
- Spot check, 2026-11-07: the key functions are at the same places in this
  source. `mCoBG_AdjustActorY` is at `src/game/m_collision_bg.c:381`,
  `mCoBG_GetWallPriority` at `:1076` with the same `flag |= 1 << (u64)unit`
  at `:1112`. The walk and dash speed constants 4.875 and 7.5 are in
  `src/game/m_player_main_walk.c_inc:160-162`. Furniture collision
  registration `aMR_RegistMoveBg` is at `src/actor/ac_my_room.c:1015`.
- The old pathfinder, obstacle grid and their conclusions are not ported
  (scope decision, 2026-11-06). The research below is what the old project
  learned about *why* that design failed and how the engine really works.
- The old research folder also holds harnesses: `navigation-tests/`
  (Python) and `portable-tests/` (Node models that compile original game
  functions and compare them). They may be reused as ideas; their licences
  and provenance must be checked before copying any into this repository.
  The old folder contains a ROM and saves; never copy those.

## Where the documents are

In the previous project, under `research/`. Read in this order:

1. `NAVIGATION_RESEARCH_STATUS.md` (latest status, 2026-09-29): what was
   established, its limits, and "continue here".
2. `LEGACY_DOCUMENTATION_AUDIT.md`: 12 older claims shown to be wrong.
3. `ENGINE_NAVIGATION_DEEP_DIVE.md`: movement, ground, walls, acre
   crossing, NPCs, world changes, furniture, entrances, proposed contracts.
4. `NAVIGATION_BRAINSTORM_AND_AUDIO_CONTRACT.md`: the user's requirements,
   and what the old navigation click actually meant.
5. `BUILDING_ENTRANCE_AND_NAVIGATION_CONTRACTS.md`: per-building entry
   conditions, and proven targeting errors in the old navigator.
6. `NAVIGATION_WORLD_DATA_AUDIT.md`: what world data exists in RAM before
   the first save, and eight proven defects in the old navigator.

Topic documents, same folder: `MOVEMENT_STATES_AND_COLLISION_FLAGS.md`,
`MOVEMENT_ENGINE_TRACE_AUDIT.md`, `COLLISION_AND_APPROACH_RESEARCH.md`,
`COLLISION_RESPONSE_AND_REPLAY.md`, `REGISTERED_DENSE_AND_TERRAIN_RESEARCH.md`,
`WALL_CROSSING_AND_CONTACT_VALIDATION.md`, `WALL_ENDPOINT_VALIDATION.md`,
`STATIC_WALL_PREDICTION.md`, `GROUND_PREDICTION_VALIDATION.md`,
`BG_STAGE_VALIDATION.md`, `LIVE_*` (live captures: tree collision, house
round trip, input, geometry, acre crossing), `PUBLIC_BUILDING_APPROACH_GAP.md`,
`NEXT_DOORWAY_TESTS.md`, `NAVIGATION_FIXTURE_READINESS.md`.

Older material (May 2026, several parts corrected by the audit above):
`source/docs/RESEARCH_nav_mod_internals.md`, `REFERENCE_system_map.md`,
`RESEARCH_door_and_river.txt`, `RESEARCH_building_entrances.txt`,
`CHANGES_pathfinder_terrain.txt`, `ANALYSIS_nav_rewrite_drift.md`,
`LINUX_BUILD_AND_NAVIGATION_RESEARCH.md`.

## What the engine actually does (summary of the old findings)

Labels: *source* means read in the code; *tested* means original functions
compiled in a small test with supplied inputs; *live* means seen in a running
game. None of it was checked against this repository's build yet, apart from
the spot check above.

**Movement is continuous and history-dependent, not tile steps** (source).
The player turns gradually toward the stick direction, accelerates by about
0.609 and brakes by about 0.326 per update, with target speed 4.875 (walk) or
7.5 (dash) times stick strength, divided by a slope factor that uses *squared*
length. Speed is cut by the cosine of the heading error. Heading, visual
facing, speed and animation are separate values. Zero speed does not mean
no movement: collision push and outside forces (for example the postman's
wings) still move the player.

**Ground contact depends on the previous frame** (source, tested). Walking
downhill keeps contact only if the player was grounded and the drop is no
more than the horizontal move. Water is handled separately. A tile is not one
surface: a tile can be split into triangles with different materials (bridge
and river within one tile), and height comes from four triangles, not a
smooth blend.

**Walls are ordered** (source, tested on 22,028 cases). The player's wall
check runs a distance pass, an ordered endpoint pass and a crossing pass,
using height bounds, previous position and approach side. A contact can be
recorded without any push. Only two contact slots exist, so they are not a
list of nearby obstacles.

**Trees, rocks and other column items make extra walls between neighbours**
(source, tested). A gap between two circles is not proof the player fits
through. Player collision radius is 18 world units.

**A known ordering bug in the wall-priority code** (tested). With 33 or more
equally distant walls it repeats or skips walls. Live captures never had more
than 7 walls, so it does not explain old failures. Track it; do not "fix"
game behaviour.

**Acre crossing is a scripted transition** (source). It needs stick strength
above 0.65, facing within about 40 degrees of the edge, an eligible boundary,
and no NPC within 6 units; then the game moves the player itself over 36
updates. Gentle steering may never trigger it.

**World data exists before the first save** (source, live). `Save_Get` reads
RAM, not the memory card. The town, foreground items and resident identities
are in RAM after generation; resident homes are assigned later in start-up.
Navigation must use live RAM, never a card file, and must not need a save.

**The world changes during play** (source). Chopping a tree writes a stump
and changes collision data. Dashing can erase flowers. Some writes do not set
the field-changed flag, and reading the item table clears that flag, so it is
not a reliable change counter. Actors unloading can put items back into the
field.

**Buildings have separate entry rules** (source, tested). Each building has a
different position offset, entry circle, required facing and action:
shops need the player still walking into the door; the post office and Able
Sisters need A; the museum and police station need the player to accept a
door request; resident houses check the item in front plus A; player houses
depend on which of the four slots it is. The old navigator aimed at wrong
points for three shop stages and then rounded targets to tile centres, which
can move a point further than the entry circle's size.

**NPCs move on their own schedules** (source). A villager is a moving target
whose actor can unload and reload; identity must use the game's own
comparison, not a pointer or roster slot.

**Furniture is moving collision** (source). Registered furniture uses live
position, rotation and uneven extents; rotating or pushing it changes
collision without re-registering. Indoor navigation was never tested live.

**Collision queries have side effects** (source). They use shared scratch
memory and write to actors, so the game's own collision functions cannot be
called freely to test a planned route.

## Why the old navigator failed (proven, from the audits)

- A yes/no tile grid stood in for real movement and collision.
- The grid was first built before resident homes and final items existed.
- Villager house footprints were placed one tile south of the real house.
- Shop entrances used wrong offsets; targets were rounded to tile centres.
- "Nearest walkable tile" snapping could pick a tile up to eight away and
  unconnected, and the pathfinder cleared blocked start and goal tiles.
- "Arrived" was announced from tile counts, without checking facing, the door
  action or the scene change.
- The tick sound meant only "crossed a tile boundary", not progress.
- Pathfinder limits (non-optimal heuristic, a full heap dropping entries,
  64-segment cap) could truncate routes silently.

## Requirements from the user (recorded in the old project, 2026-09-28)

- Navigation must follow the live world: chopped trees, placed and removed
  items, moving villagers, shop upgrades, villagers moving in and out.
- Every normal town layout must work (two and three tiers, any building
  placement). No curated seed, no fixed coordinates.
- Trips from A to B, multi-stop trips, across acres, and into buildings.
- Both automatic walking and directional guidance during manual walking
  are wanted; how they are controlled and sound is not yet decided.
- Arrival, entrance-ready and entered must be separate, truthful states.
  A timeout is never success. The player can always take over.
- Later, indoor navigation and accessible furniture arrangement, sharing the
  same live-world understanding.

## Open design questions

The old project left these open, and they are still open:

- Route plan from collision geometry, or use isolated copies of the game's
  own collision checks, or a mix.
- How to detect world changes reliably (hooks on the writes, regular checks,
  checking the next stretch before moving).
- Audio and speech design for guidance, progress, obstruction and arrival.
- Whether to keep a movement click.

## Next steps for the rewrite

1. Re-check the summary above against this source, one area at a time,
   starting with world data in RAM and the player movement update.
2. Add passive logging hooks (no input injection) to record the player's
   per-frame movement stages and the local collision geometry.
3. Decide the architecture with the user before writing the planner.
