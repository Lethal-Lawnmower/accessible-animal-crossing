# Accessibility rewrite: scope, status, and decisions

## Goal

Make Animal Crossing (GameCube) fully playable by blind and visually impaired
players. The game's own gameplay, visuals, timing, controls and saves stay as
they are. Accessibility is added alongside them.

## Base

- Fresh clone of the upstream port, made 2026-11-06:
  https://github.com/flyngmt/ACGC-PC-Port, commit `2d1dc4bd`
  (`v1.0.0-8-g2d1dc4bd`, 2026-11-04).
- Submodules: `extern/aurora` at `5794daa2`, `extern/borealis` at `8e9427fa`.
- License: the decompiled code (ac-decomp) is CC0 1.0, and the PC port layer and
  FixNES are MIT. See `../../LICENSE`.
- Upstream contains no game assets. A legally obtained GAFE01 Rev 0 disc image is
  required to run the game. It must never be committed.

## Scope

| Area | Plan |
|---|---|
| Speech output | Rebuild the output layer on Prism. The old Tolk backend is not carried over. |
| Menu and UI speech | Carry over from the old project after review, adapted to port v1.0.0. |
| Navigation | Complete rewrite. The old navigation code is not ported. |

## Previous project

- Location: `D:\dev\Animal Crossing restored from destructive backup\Linux`.
  This is local only and not part of this repository.
- Old repository: https://github.com/Lethal-Lawnmower/accessible-animal-crossing
  (public). The last push was 2026-04-30, commit `655f40f`.
- Unpushed local work: branch `accessibility-rewrite` at `4099d24`, based on
  upstream 0.9.3 (2026-08-02), with about 6,500 uncommitted changes. That branch
  does not contain `655f40f`.
- The old documents and conclusions are leads, not established facts. Upstream
  has changed a great deal since 0.9.3: it now runs 64-bit on the Aurora renderer
  and is renamed Nookery. Old code will need porting, not copying.

## Current status

- 2026-11-06: repository created from the fresh upstream clone. Nothing has been
  built, run, or tested here yet. No accessibility code exists yet.

## Decision log

- 2026-11-06, user: this folder (`D:\dev\AC`) is the rewrite. Build on a fresh
  copy of the PC port, not on the old uncommitted work.
- 2026-11-06, user: keep the speech and menu work, and rewrite navigation
  completely.
- 2026-11-06, user: everything goes to GitHub from day one, including all code
  commits.

## Next steps

1. Build the unmodified port on Windows and confirm that it runs.
2. Inventory the old speech and menu modules: what each one announces and which
   game hooks it uses.
3. Integrate Prism as the speech backend.
4. Port the menu speech module by module.
5. Design the new navigation from the game's own data structures.
