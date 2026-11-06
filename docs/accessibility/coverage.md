# Accessibility coverage

What speaks, what is still to do, and screens the previous project never
covered. Updated 2026-11-06.

Status values: **done** (implemented and checked in a real run), **built**
(compiles, not yet checked in a run), **new** (never covered, not started).

## Game screens

| Screen | Status | Notes |
|---|---|---|
| Speech output (Prism) | done | [speech.md](speech.md) |
| Dialogue / message window | done | checked with the K.K. and train intro; see [speech.md](speech.md) |
| Choice windows | done | checked with the intro |
| Title screen ("Press START!") | done | `src/actor/ac_animal_logo.c` says "Animal Crossing. Press START!" when the title starts waiting for Start |
| Keyboard / text editor (names, letters) | done | `text_editor.c`. Checked: prompts ("Enter your name.", "Enter Destination."), typing, "deleted d" |
| Inventory and other item menus (tag overlay) | built | `item_menu.c`: slot text (all 3 label lines) with position, section and menu names, action popup. Not checked: the inventory stays closed during the arrival intro, and no usable save exists yet (see below) |
| Clock setting | built | `screens.c`; adds the weekday |
| Town map | built | `screens.c`; reads the map's own label words and resident names; rows lettered from the top |
| Address book | built | `mail_screens.c`; selection checked every frame (covers mouse and page turns) |
| Bulletin board | built | `mail_screens.c`; a post is read when the page finishes turning |
| Letter reading | built | `mail_screens.c`; header, body, footer |
| Warning popups | built | `screens.c`, every line of the popup |
| Diary privacy question (`CPWARNING`) | built | `screens.c` with Yes/No |
| Bank | built | `screens.c`; says deposit or withdraw |
| Loan repayment | built | `screens.c` |
| Birthday entry | built | `screens.c` |
| "Is this OK?" after writing | built | `screens.c`; the question, then the answers once they appear |
| Calendar | built | `calendar.c`: owner's name, month and year ("this month"), then in day mode weekday, date, "today", the played and event icons, event names with position. New: never covered before |
| Diary page | built | `calendar.c`: month, the whole page, "A to write" when it can be written. Writing uses the text editor. New |
| Password screens (make and check) | built | `text_editor.c` and `m_editor_ovl.c`: the window prompt ("Enter a receiver.", "Enter a secret code."), then the field ("Town name", "Player name", "Line 1 of 2"). Moving between fields, including the automatic move when line 1 fills, says the field and its text. New |
| Letter writing fields | built | the same field speech: "Greeting", "Message", "Signature" |
| Gyroid message board | built | only the text editor, which already says "Gyroid message" and the current text |
| Memory card letters and designs, Game Boy Advance designs | built | these use the item menu (tag overlay), so slots already speak; section names added (folders, folder name, your designs) |
| Town tune editor | built | `screens.c`; note names from the editor's own sound table (value 14 is "hold", not "rest" as the old project said) |

## Submenus never covered

From the `mSM_OVL_*` list in `include/m_submenu.h`. Covered as of
2026-11-06 (built, see the table above): calendar, diary, password make and
check, gyroid message board, memory card letters and designs, Game Boy
Advance designs. `CPEDIT` has no screen of its own (its move and draw
functions are empty).

Still not covered:

- Pattern design editor (`DESIGN`, `src/game/m_design_ovl.c`, 3,237 lines).
  A 32 by 32 drawing grid with palette, pen, fill and undo tools; most of
  its state fields are unnamed in the decompilation. Making it usable needs
  its own design (how to read rows of pixels, colours and tools), not just
  announcements. Deferred.
- Calendar diary scrolling: the diary page is read in full when it opens;
  scrolling with the stick is not announced. F5 repeats the page.
- The Game Boy Advance link itself is not expected to work in the PC port;
  only the design list was given names.

## Nookery's own screens (new in the upstream port)

The upstream port has its own menus, built with RmlUi (`pc/src/ui/`). The
previous project never spoke the port's menus.

| Screen | Source | Status |
|---|---|---|
| Prelaunch (choose disc, settings, controls, quit) | `pc/src/ui/prelaunch.cpp` | done: buttons spoken as focused |
| Settings | `pc/src/ui/settings.cpp` | done on the first tab: "label: value", the help text, value changes |
| Town select | `pc/src/ui/town_select.cpp` | built (generic), not checked |
| Controller configuration | `pc/src/ui/controller_config.cpp` | built (generic), not checked |
| In-game menu bar (Escape) | `pc/src/ui/menu_bar.cpp`, `overlay.cpp` | built (generic), not checked |

One module, `pc/src/access/host_menu.cpp`, speaks all of these by following
RmlUi focus: the focused control's text (label: value), "unavailable" for
disabled controls, the help pane shown to the right of the focused control
(queued), and only the new value when a setting changes. Not covered: the
operating system's file and folder pickers (Windows dialogs, normally read
by the screen reader itself) and the status line at the bottom of the
prelaunch screen.

## Test save

2026-11-06: a web search found no downloadable GAFE01 save, only save
editors and forum threads; none was downloaded.

The old project's saved town (every copy found is the same file) is rejected
by this port: `[PC] GCI: auxiliary checksums invalid; refusing destructive
recovery`. It was written by the older 32-bit port. The game then reports the
town data as corrupted; the port refused to erase it. Checking the gameplay
screens needs a save made with this port.

## Gameplay feedback (not part of this pass)

The old project also announced gameplay events, such as fishing bites,
catching bugs, digging, shaking trees, balloons and furniture moves, plus
helper modes (furniture mode, magnet, auto-input) and information keys
(T time, G bells, M location). These are outside the current pass, which
covers menus, dialogue and options. Navigation is being rewritten separately.

## Hotkeys

| Key | Function | Status |
|---|---|---|
| F5 | Repeat the last announcement | done |
| F5 in the text editor | Read back the whole text | built |

F5 was the previous project's repeat key. The port's own "Keyboard typing"
toggle (Tab) shows on the editor screen; while it is on, Space types a space
instead of pressing A.
