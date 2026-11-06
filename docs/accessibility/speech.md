# Speech output, dialogue and choices

Status as of 2026-11-06. See [coverage.md](coverage.md) for which screens speak.

## Speech output (Prism)

- Library: [Prism](https://github.com/ethindp/prism) v0.18.3, MPL-2.0, as the
  `extern/prism` git submodule. Prism picks the best output, normally the
  running screen reader. No other speech system is used, directly or as a
  fallback (project rule). Which output Prism itself falls back to when no
  screen reader is running is Prism's decision and has not been examined yet.
- Build: `cmake/Accessibility.cmake` builds Prism as a separate
  ExternalProject, because Prism's CMake sets global options
  (`BUILD_SHARED_LIBS`, C/C++ standard, MSVC runtime) that must not leak into
  the game. It is a shared library; `prism.dll` is copied next to `Nookery.exe`.
  Prism needs CMake 4.1+ (C23) and, on Windows, ATL.
  `tools/build-windows-msvc.ps1` finds both.
- Code: `pc/src/access/speech.cpp`, interface `pc/include/access/speech.h`.
  All Prism calls run on one worker thread, so speech never stalls a frame.
  An interrupting request clears the queue first, so new focus replaces stale
  speech instead of piling up. Every request is logged at debug level as
  `say!` (interrupt) or `say ` (queued) in the `access::speech` log module.
- Startup: `pc/src/aurora_main.cpp` starts speech right after logging, and an
  RAII guard stops it on every exit path.

## Game text decoding

`pc/src/access/game_text.c` converts the game's one-byte encoding to UTF-8,
following the `CHAR_*` list in `include/m_font.h`. Picture symbols become
short words ("heart", "music note"). Embedded control codes are skipped with
their parameters.

Corrections to the previous project's table:

| Code | Old table | Actually |
|---|---|---|
| 59 | `;` | sweat-drop symbol (208 is the semicolon) |
| 92 | `\` | "annoyed" symbol (222 is the backslash) |
| 213–216 | dropped | curly quotes and apostrophes |
| 217–222 | dropped | OE ligatures, French ordinal suffixes, backslash |

## Dialogue (message window)

Code: `pc/src/access/dialogue.c`. Hooks are marked `/* accessibility */`:

| Hook | File | When |
|---|---|---|
| `acc_dialogue_message_loaded(msg, TRUE)` | `src/game/m_msg_appear.c_inc` | message window opens; speaker named |
| `acc_dialogue_message_loaded(msg, FALSE)` | `src/game/m_msg_main.c_inc`, `mMsg_ChangeMsgData` | follow-up message in an open window |
| `acc_dialogue_page_advanced` | `src/game/m_msg_normal.c_inc` | player pressed A for the next page |
| `acc_dialogue_page_shown` | `src/game/m_msg_cursol.c_inc` (Last, Continue, Button, Button2 handlers) | the game finished displaying a page |

How it reads a page: it copies the message buffer and runs the game's own
`mMsg_Copy*` functions (`src/game/m_msg_main.c_inc`) on the copy, the same
ones the display cursor uses. Names, the villager catchphrase, items with
articles, dates, town and island names, letter text and capitalisation
therefore match what is displayed, and the live message is never modified.
Each page is spoken in full as soon as it starts, without waiting for the
letter-by-letter display. The speaker's name is prefixed when the window
opens ("K.K.: …").

Exception: `PUT_STRING_RANDOM_NUMBER_2` gets its value from the game's random
number generator (`mMsg_CopyRamdomNumber2`). Calling it early would consume a
random number and change gameplay. Such pages are spoken from the live buffer
once the game has displayed them.

Corrections to the previous project's reader, from reading the source:

- These insertion codes were skipped, so their text was missing from speech:
  villager catchphrase (`TAIL`), island name, letter text (`MAIL`),
  "determination" strings, capital-letter and cut-article codes.
- a.m./p.m. came from the clock hour. The game uses the message's
  `mMsg_STATUS_FLAG_USE_AM` flag.
- The page-advance hook passed `end_text_cursor_idx` straight through. When
  the player presses A, that index still points at the button-prompt code;
  the cursor steps past it on its next run
  (`mMsg_Main_Cursol_Button_ControlCursol`). Read from there, the page ends
  immediately. The new reader skips the prompt first. Whether the old build
  actually stayed silent on later pages was not re-tested.

## Choices

Code: `pc/src/access/choice.c`, hooked in `src/game/m_choice_main_normal.c_inc`.
When the window becomes interactive, the option list and the highlighted
option are queued after the question, so the question is not cut off (the
previous project interrupted it). Moving the selection interrupts with
"label, n of total".

## Runtime verification, 2026-11-06

Windows 11, NVDA running, build of this branch, fresh throwaway profile
(`--user-dir`), GAFE01 Rev 0 disc image. Keys were sent to the game window
with `keybd_event` (Enter = Start, Space = A, W/S = stick). Speech text was
read from the `access::speech` debug log after a normal window close (the log
file is buffered until exit).

- Prism selected NVDA.
- New game, K.K. Slider intro: all nine dialogue pages were logged in order,
  starting "K.K.: So, you've decided to move out? Get your own place? See the
  world?", including every page reached with A. Apostrophes were intact.
- The choice window after "So, are you ready to hop on that train…" was logged
  as "Choices: I'm ready to go!, Before I go…." (doubled period since fixed),
  then "I'm ready to go!, 1 of 2". Moving down and up logged "Before I go…,
  2 of 2" and "I'm ready to go!, 1 of 2".

Not established: what NVDA actually said aloud (this checked the text handed
to Prism, not audio), and pages using the random number or the other
insertion codes, which this scene did not contain.

## Item menus (tag overlay)

Code: `pc/src/access/item_menu.c`, hooks in `src/game/m_tag_ovl.c`:
end of `mTG_init_tag_data_item_win` (slot focus), `mTG_init_tag_data`
(action popup), `mTG_move_cursol_base` (section change), `mTG_select_tag_base`
(action highlight), `mTG_tag_ovl_construct` (menu name).

A slot is read as its label lines joined, then "n of total" from the
section's grid size (`mTG_table_data`). A menu or section name interrupts;
the slot that follows is queued behind it so it is not cut off. Status:
built, not runtime-checked.

## Text editor

Code: `pc/src/access/text_editor.c`, hooks at the end of
`mED_editor_ovl_construct` and `mED_move_Play` in `src/game/m_editor_ovl.c`.
The name-entry window (`src/game/m_ledit_ovl.c`) hands over its own prompt.

Spoken: on opening, the prompt or editor kind, letter mode and current text;
typed characters (punctuation spelled out, capitals as "capital X");
"deleted X" on backspace; the character under the text cursor when it moves;
the grid character when the on-screen grid cursor moves; mode changes.

Runtime check, 2026-11-06 (same method as above): new game to Rover on the
train. Logged "Text entry, lowercase, empty. exclamation mark" on opening (a
generic name, since replaced by the window's prompt), then "a", "d",
"deleted d" for typing a, d, Backspace. The same run confirmed insertion
codes: "Is it... Let's see now... 11:31 p.m. on Friday, November 6th, 2026?",
plus both train choice windows.

Not established: arrow-key text cursor moves (the test sent non-extended
arrow key codes, and nothing was logged), and the on-screen grid with a
controller.
