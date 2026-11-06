# Accessibility rewrite: scope, status, and decisions

Topic documents:

- [coverage.md](coverage.md): which screens speak, and what is left
- [speech.md](speech.md): Prism speech output, dialogue and choice speech

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

- 2026-11-06: repository created from the fresh upstream clone and pushed to
  branch `rewrite` of the existing GitHub repository.
- 2026-11-06: the unmodified port builds and starts on Windows 11 (x64).
  See "Windows build check" below.
- 2026-11-06: Prism speech output, dialogue speech and choice speech
  implemented and checked in a real run with NVDA (K.K. intro). See
  [speech.md](speech.md). Menus are next; see [coverage.md](coverage.md).
- 2026-11-06: text editor speech checked on the name screen; item menu
  (inventory) speech built but not yet runtime-checked.

## Windows build check (2026-11-06)

Source: commit `2d1dc4bd` plus this branch's documentation commits. No game code
was changed.

Build: `tools\build-windows-msvc.bat` (preset `windows-msvc-relwithdebinfo`).
It finds Visual Studio with vswhere and puts Visual Studio's own CMake and Ninja
first on PATH. Output: `build\windows-msvc-relwithdebinfo\bin\Nookery.exe`.

- Visual Studio 2022 Build Tools (MSVC 14.44): build succeeded, all 4,751 steps.
- Visual Studio 2026 Build Tools (MSVC 14.51), which the script picks as the
  newest: build succeeded, all 4,751 steps.
- First attempt failed: a third-party CMake 3.31.12 earlier on PATH produced
  `fatal error RC1107` when compiling zlib-ng's resource file. The same `rc.exe`
  command succeeded when run directly. Using Visual Studio's bundled CMake
  (3.31.6-msvc6) fixed it. The script now does this automatically.

Runtime check: launched with a GAFE01 Rev 0 disc image passed on the command
line, with `--user-dir` and `--log-dir` pointing at a throwaway folder, for
20 to 25 seconds, then force-closed. This was done twice, once per build.

- The process stayed running and opened a window titled `Nookery`.
- The renderer was D3D12. The log had no warning, error, or fatal lines.
- The log reached `[PC] boot: calling initial_menu_init` (and `dvderr_init` in
  the first run).

Not established: this check did not confirm the title screen, play any
gameplay, or check audio by ear. It shows only that the build starts and begins
booting the game.

## Decision log

- 2026-11-06, user: this folder (`D:\dev\AC`) is the rewrite. Build on a fresh
  copy of the PC port, not on the old uncommitted work.
- 2026-11-06, user: keep the speech and menu work, and rewrite navigation
  completely.
- 2026-11-06, user: everything goes to GitHub from day one, including all code
  commits.
- 2026-11-06, user: investigate the old project's menus, UI, NPC dialogue,
  all dialogue, options and keyboard, carry them over to the new port, and
  look for screens that were never covered.
- 2026-11-06, user: reuse the existing public repository
  `Lethal-Lawnmower/accessible-animal-crossing`. The rewrite lives on its own
  branch, `rewrite`. The old `master` branch is left unchanged.

## Next steps

1. Runtime-check the inventory speech (needs a game past the intro; a test
   save would speed this up).
2. Port the remaining old menu screens: clock, map, address book, notice
   board, letters, warnings, bank, loan, birthday, edit confirmation, town
   tune. Then the submenus never covered (diary, calendar, passwords,
   memory card screens, pattern design).
3. Add hotkeys (repeat dialogue, read back editor text) and the title screen
   announcement.
4. Make the port's own RmlUi menus (prelaunch, settings) speak.
5. Design the new navigation from the game's own data structures.
