# Agent instructions: Animal Crossing accessibility rewrite

This repository is an accessibility rewrite of Animal Crossing (GameCube, GAFE01
USA) for blind and visually impaired players. It is built on a fresh copy of the
open-source Nookery PC port (formerly ACGC-PC-Port). Read
[docs/accessibility/README.md](docs/accessibility/README.md) before substantive
work. It holds the scope, current status, and decision log.

## Scope (user decision, 2026-11-06)

- Speech and menu accessibility from the previous project is carried over. Review
  it first; do not copy it in bulk.
- Navigation gets a complete rewrite. Do not port the old pathfinder, obstacle
  grid, cliff handling, or their conclusions. The old research may be used as
  leads, verified against this source and the decompilation before relying on it.
- Speech output must go through Prism. The old project used Tolk. Do not use
  Tolk, NVDA DLLs, SAPI, or any other speech system, as a direct implementation
  or a fallback.

## Previous project (read-only reference)

`D:\dev\Animal Crossing restored from destructive backup\Linux` holds the old
work, built on port version 0.9.3. It contains a ROM and save files. Never copy
those into this repository.

## GitHub workflow

Everything goes to GitHub from day one. Commit in small, descriptive steps and
push regularly, so no work exists only on one machine.

Never commit or push disc images, ROMs, extracted game files (`orig/`), saves
(`.gci`), or other game material. `.gitignore` blocks the common ones. Check
`git status` before every commit.

## Upstream

The `upstream` remote is https://github.com/flyngmt/ACGC-PC-Port. Accessibility
changes should stay easy to rebase: put new code in its own files where you can,
and keep edits to upstream files small and clearly marked.

Licensing: the decompiled code is CC0, and the PC port layer and FixNES are MIT.
See `LICENSE` and keep the upstream notices.
