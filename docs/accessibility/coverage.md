# Accessibility coverage

What speaks, what is still to port from the previous project, and screens
the previous project never covered. Updated 2026-11-06.

Status values: **done** (implemented and runtime-checked), **built** (compiles,
not runtime-checked), **to port** (exists in the old project, not here yet),
**new** (never covered before).

## Game screens

| Screen | Status | Notes |
|---|---|---|
| Speech output (Prism) | done | [speech.md](speech.md) |
| Dialogue / message window | done | checked with K.K. intro; see [speech.md](speech.md) |
| Choice windows | done | checked with K.K. intro |
| Title screen ("Press START!") | new | nothing is announced |
| Inventory and other item menus (tag overlay) | built | `pc/src/access/item_menu.c`: slot text (all 3 label lines) with position, section and menu names, action popup. Not yet runtime-checked |
| Keyboard / text editor (names, letters) | done | `pc/src/access/text_editor.c`. Checked on the train name screen: opening, PC-keyboard typing ("a", "d"), backspace ("deleted d"). The window prompt ("Enter your name.") is now used as the title; that change is built, not re-run |
| Clock setting (time-in) | to port | |
| Town map | to port | old: acre, building, residents |
| Address book | to port | |
| Notice board | to port | |
| Letter reading (board) | to port | |
| Warning popups (incl. memory card warnings) | to port | |
| Bank (ABD) | to port | |
| Loan repayment | to port | |
| Birthday entry | to port | |
| "Is this OK?" edit confirmation | to port | |
| Town tune editor | to port | |

## Other submenus never covered

From the `mSM_OVL_*` list in `include/m_submenu.h`. The previous project had
no speech for these: diary, calendar, password make and check, house gyroid
message board (`HBOARD`), memory card letters and designs (`CPMAIL`,
`CPEDIT`, `CPORIGINAL`), pattern design (`DESIGN`), Game Boy Advance.

## Nookery's own screens (new in the upstream port)

The upstream port added its own menus, built with RmlUi (`pc/src/ui/`). The
previous project's port version had different PC menus, and it never spoke
them either.

| Screen | Source | Status |
|---|---|---|
| Prelaunch (choose disc, start game) | `pc/src/ui/prelaunch.cpp` | new; skipped when a disc is given on the command line |
| Town select | `pc/src/ui/town_select.cpp` | new |
| Settings | `pc/src/ui/settings.cpp` | new |
| Controller configuration | `pc/src/ui/controller_config.cpp` | new |
| In-game menu bar / overlay | `pc/src/ui/menu_bar.cpp`, `overlay.cpp` | new |

## Gameplay feedback (not part of this pass)

The old project also announced gameplay events, such as fishing bites,
catching bugs, digging, shaking trees, balloons and furniture moves, plus
helper modes (furniture mode, magnet, auto-input). They are outside the
current pass, which covers menus, dialogue and options. Navigation is being
rewritten separately.

## Hotkeys

| Function | Status |
|---|---|
| Repeat last dialogue page | built (`acc_dialogue_repeat`), no key bound yet |
| Read back the whole text in the editor | built (`acc_text_editor_read_all`), no key bound yet |

Note: the port's own "Keyboard typing" toggle (Tab) appears on the editor
screen; while it is on, Space types a space instead of pressing A.
