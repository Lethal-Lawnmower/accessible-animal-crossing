REPOSITORY NOTICE

This repository is in early development and does not yet have full
documentation. A proper write-up explaining the project architecture,
development decisions, and contribution guidelines will be added as the
mod matures.

This mod was built with the assistance of AI tools for code generation.
All planning, design decisions, accessibility research, testing, and
direction were done by Lethal Lawnmower. The AI handled implementation
only — every feature, every design choice, and every accessibility
consideration came from me directly.

This is still very early access software. Things will break, things will
change, and the codebase is actively evolving. Use at your own risk.

Feedback and bug reports are welcome and actively encouraged. Please join
the Accessibility Discord server and post in the Animal Crossing channel:
https://discord.gg/JuumMWgAdJ

This mod is built on top of the Animal Crossing GameCube PC Port by
FlyingMeta (https://github.com/flyngmt/ACGC-PC-Port), which is itself
built on the ACreTeam decompilation (https://github.com/ACreTeam/ac-decomp).
Full credits are in README.txt.


EARLY ACCESS TEST BUILD

This is an early access test build of the Animal Crossing GameCube PC
Port accessibility mod. This build is mainly for testing and gathering
feedback. Things will be broken. Things will break. Things won't be
polished and that's perfectly OK.

Please submit any feedback, bugs, and issues in the Accessibility
Discord server in the Animal Crossing channel.

One final note: I am not responsible for how you obtain a copy of the
game. That is solely on you and I will not be answering questions on
how to find the game.


Animal Crossing GameCube PC Port - Accessibility Mod
Version 0.1.1

About This Mod

This is an accessibility mod for the Animal Crossing GameCube PC port
by FlyingMeta, built on top of the ACreTeam decompilation project.

The mod adds screen reader support, spatial navigation, and audio cues
throughout the game. Dialogue, menus, inventory, fishing, bug catching,
navigation, furniture placement, and more are all narrated through your
screen reader as you play.

It works with NVDA, JAWS, Windows Narrator, and any screen reader
supported by the Tolk library. NVDA is recommended.

Note: Fishing, bug catching, and buried item detection have not yet
been fully tested. Use these features with that in mind and report any
issues.


Getting Started
---------------

When you first launch the game, there is a short animation sequence on
the title screen. Give it a moment before pressing any keys. Once the
animation finishes, the menu will appear and you will hear "Start Game"
spoken by your screen reader.

Use W and S (or the arrow keys) to move between Start Game and Options.
Press Space (the A button) to confirm your selection.

To start a new game, select Start Game. The game will begin with a
train ride where a cat named Rover asks you questions to set up your
character — your name, your town name, and other details. All of his
dialogue is spoken through your screen reader. Answer using the
on-screen keyboard (the mod narrates each character as you move the
cursor) and press Space to confirm.

After the opening sequence, you will arrive in your new town. Your
first task is usually to find Nook's store. To do this:
  1. Press Shift+L to cycle to the Buildings category.
  2. Press L repeatedly until you hear "Nook's Store" (or one of its
     upgrade names: Nook's Cranny, Nook 'n' Go, Nookway, Nookington's).
  3. Press K for turn-by-turn directions to the store.

The game world is divided into a grid of acres, labeled A-1 through
F-5. As you walk between acres, the mod announces the new acre label
(for example, "Acre C-3"). This helps you build a mental map of your
town. The navigation system can find anything in the entire town
regardless of which acre you are in.

A subtle click sound plays each time you cross into a new tile on the
ground. When the navigation system tells you something is "12 steps
south," you can count 12 clicks to know you have arrived.


What You Need
-------------

1. This mod (the contents of this zip file).

2. A legally obtained copy of Animal Crossing (USA) for GameCube.
   You need the disc image file in ISO, GCM, or CISO format.
   The game ID is GAFE01.

   This mod does not include any game assets. You must provide your own
   disc image from a copy you legally own.

3. A Windows PC (Windows 10 or later, 64-bit or 32-bit).

4. A screen reader (strongly recommended):
   - NVDA (free): https://www.nvaccess.org/
   - JAWS: https://www.freedomscientific.com/
   - Windows Narrator also works.

   The mod detects your active screen reader automatically. If no screen
   reader is found, it falls back to Windows SAPI speech.


Installation
------------

1. Extract this zip file to any folder on your computer.

2. Place your disc image file in the "rom" folder inside the extracted
   files. The file should be named something like:
   Animal Crossing (USA).iso
   The exact filename does not matter as long as the file is in the rom
   folder and is in ISO, GCM, or CISO format.

3. Make sure your screen reader is running.

4. Run AnimalCrossing.exe to start the game.

5. The game will launch. If your screen reader is detected, you will
   hear the title screen menu options spoken as you navigate them.


Default Controls
----------------

These controls emulate the GameCube controller. You can change them by
editing keybindings.ini in a text editor.

Movement:
  W/A/S/D         Walk (north/west/south/east)

Buttons:
  Space            A button (talk, pick up, confirm)
  Left Shift       B button (cancel, run, back out)
  X                Open inventory
  Y                Pick up item or use equipped tool
  Enter            Start (open map or pause)
  Z                Camera snap
  Q                L button (open chat or letter interface)
  E                R button (quick-select equipped item)

Camera:
  Arrow keys       Move camera (C-stick)

D-Pad:
  I/K/J/L          Up/Down/Left/Right (used in menus and pattern select)

Note: When the accessibility system is active, J, K, and L are used by
the navigation system and will not send D-pad input to the game. The
arrow keys are similarly consumed when Furniture Mode is active indoors.


Accessibility Features
----------------------

Everything below works automatically once the game is running with a
screen reader active. No setup or configuration is required.


DIALOGUE AND TEXT

All dialogue is spoken through your screen reader the moment it appears,
without waiting for the slow character-by-character text display. The
speaking character's name is announced before their dialogue so you
always know who is talking.

The mod understands the game's full character set including accented
characters, symbols, and special characters used by villagers. Inline
variables like your player name, town name, item names, dates, and
times are all resolved and spoken naturally.

Multi-page dialogue is handled automatically. Each new page is spoken
when it appears. You press A to advance to the next page as normal.


MENU NARRATION

Every menu in the game is narrated:

- Inventory: Item names are spoken as you move the cursor. Empty slots
  are announced as "Empty." Section names (Pockets, Letters, Money,
  Wallpaper and Carpet, etc.) are announced when you switch between
  them. Action popups (Drop, Give, Grab, etc.) are read aloud.

- Choice windows: When a Yes/No or multiple-choice prompt appears, all
  options are listed with the selected one marked. Moving the cursor
  announces the newly highlighted option.

- On-screen keyboard: When typing letters, town names, or other text,
  the character under the cursor is spoken as you move. Mode changes
  (uppercase, lowercase, symbols) are announced. Each character you
  type is spoken, and backspace says "delete."

- Shop menus: Items for sale are scanned and included in the navigation
  system's Items category when you are inside Nook's shop, Able Sisters,
  or Crazy Redd's tent.

- Town map: Acre labels, building names, resident names, and your
  current position ("you are here") are all announced as you move the
  cursor.

- Bulletin board: Entry number, date, and full message text are read
  when you open the board and when you page through entries.

- Letters: The full letter is read aloud (header, body, and footer)
  when you open one.

- Bank overlay: Your wallet balance and account balance are announced
  on open. As you adjust the transfer amount, the digit position name
  (hundreds, thousands, etc.) and current values are spoken.

- Loan repayment: Your available money and remaining loan are announced.
  As you adjust the repayment amount, updated values are spoken.

- Address book: Names and positions are announced as you browse.

- Clock adjustment: Field names (Hour, Minute, Month, Day, Year) and
  their values are spoken as you navigate and change them.

- Birthday selection: Month and day fields are announced as you set
  your birthday.

- Town tune editor: Slot number and note name are spoken as you compose.

- Confirmation dialogs: Options like "Is this OK? Yes / Rewrite / Throw
  it out" are read aloud.

- Warning popups: All 28 types of warning messages in the game are
  spoken (bells limit, mailbox full, inventory full, etc.).

- Title screen: The main menu (Start Game, Options) and the Options
  submenu (Resolution, Fullscreen, VSync, MSAA, Preload Textures) are
  fully narrated. Option values are announced as you change them.


SCENE AND LOCATION AWARENESS

- When you enter a new area, the location name is announced
  automatically. Villager houses include the resident's name (for
  example, "Murphy's House").

- When you walk into a new acre outdoors, the acre label is announced
  (for example, "Acre C-3").

- Press T at any time for the current time and weather ("3:45 p.m.,
  raining").

- Press G for your wallet balance ("12500 bells").

- Press M for your current location. Outdoors this gives the acre
  label. Indoors it gives the building or room name, including the
  villager's name for NPC houses.


NAVIGATION SYSTEM

A full navigation system lets you find anything in your town.

Press Shift+L or Shift+J to cycle through categories:
  - Villagers (all villagers currently in your town)
  - Buildings (shops, museum, post office, police station, and more)
  - Houses (your house and all villager houses, labeled by resident)
  - Items (dropped items outdoors, shop items indoors)

Press L or J to cycle through items in the current category. Each item
is announced with its name, compass direction from you, and distance
in steps.

Press K for directions to the selected target. Outdoors, this uses A*
pathfinding to calculate a walkable route around water, cliffs, and
obstacles, then gives you turn-by-turn directions like:
  "Path to Museum: 12 south, 6 east, 8 south. 52 steps total."

The navigation system scans the entire town — all 30 acres — so every
building, villager, and house is always available regardless of how far
away you are. It supplements live actor data with save-file information
to give you a complete picture of your town.

NPC proximity alerts: When a villager comes within range, you hear
"[Name] nearby, [direction]" automatically, so you never accidentally
walk past someone.


FISHING

Fishing is one of the hardest activities for blind players because the
original game distinguishes nibbles from bites only through controller
vibration intensity and a subtle visual difference. This mod makes it
fully accessible:

- "Line cast" when you cast your line.
- "Nibble" when the fish nibbles (do NOT press A).
- "Bite now!" when the fish bites (press A immediately to catch it).
  This is the most critical accessibility cue in the mod.
- "Caught [fish name]!" when you land a fish, with the specific species
  or trash item named.
- "Fish got away" if you miss the timing.


BUG CATCHING

The mod continuously scans for insects when you are outdoors with a net
equipped:

- "[Bug name] nearby" when a bug is first detected.
- Patience alerts as you approach: "calm," "nervous, slow down," "about
  to flee, stop moving," or "fleeing" based on how close you are to
  spooking it. Each species has its own spook distance.
- "In range, press A to swing net" when you are close enough to catch
  it.
- "Caught [bug name]!" on a successful catch.
- "Miss" if you swing and miss.

Press F3 to toggle auto-catch mode. When enabled, the mod automatically
navigates the player toward detected bugs, slows the approach speed to
avoid spooking them, and swings the net when in range. After catching a
bug, it targets the next nearest one if any remain in the acre.
Auto-catch is on by default when the net is equipped.

NOTE: Auto bug catching is still in very early development and is known
to break often. It may walk into obstacles, fail to detect certain bugs,
or behave unexpectedly in some situations. Feedback on auto-catch is
especially welcome — please report any issues you encounter so this
feature can be improved.


FOSSIL AND BURIED ITEM DETECTION

Press F2 to scan the area around you for buried items. The mod checks a
9 by 9 tile area and reports how many buried spots it found, the compass
direction to the nearest one, and the distance in steps.

When you dig up a buried item, the mod announces what you found:
"Unearthed [item name]."


MAGNET AUTO-COLLECT

When you shake a tree or hit a rock and items fall to the ground, the
magnet system automatically picks them up for you so you don't have to
navigate to each dropped item. Each pickup is announced: "Picked up
[item name]."

The magnet only collects items that the game just dropped (from trees,
rocks, etc.). It does not pick up items you deliberately placed on the
ground from your inventory.

Press F1 to toggle the magnet on or off. It is on by default.


TREE AWARENESS

When you walk near a tree, the mod tells you what kind of tree it is
and what it contains:

- "Apple tree, 3 fruit" or "Orange tree, empty"
- "Money tree, 1000 bells"
- "Oak tree, furniture hidden" or "Cedar tree, wasps"
- "Oak tree, present"
- And other variations for special trees.

When you shake a tree:
- "Wasps incoming, run!" gives you a critical early warning.
- "[Item] fell" tells you what dropped.


BALLOON TRACKING

When a balloon spawns, you hear "Balloon spotted, traveling [direction]."

As a balloon passes nearby: "Balloon nearby, [direction]."

If a balloon gets stuck in a tree: "Balloon stuck in tree, [direction],
[steps] steps, shake tree to get it."

If a stuck balloon is about to escape: "Balloon escaping soon, hurry."

When you knock a present down: "Present dropped, [direction], [steps]
steps." If it falls in water: "Present fell in water, lost."


FURNITURE MODE (Indoors)

Press F to toggle Furniture Mode when you are inside a room. This gives
you a virtual grid cursor you can move with the arrow keys to explore
the room layout tile by tile.

Each tile announces what is there: furniture name and size ("Regal Sofa,
1 by 2"), "Empty," or "Wall." Wall adjacency is also reported.

Press R at any time indoors for a room overview that lists every piece
of furniture from northwest to southeast with positions.

Press Shift+F for a readout of the current wallpaper and carpet.

When you push or pull furniture, the mod announces the furniture name
and the direction it moved. If Furniture Mode is on, you also get wall
adjacency and nearby furniture information at the new position. If the
move is blocked, it tells you what is in the way.

When furniture is rotated, the mod announces the new facing direction.


BRAILLE DISPLAY SUPPORT

All speech output is also sent to braille displays through the Tolk
library if one is connected.


Hotkey Quick Reference
----------------------

Information readouts (press any time):
  T               Time and weather
  G               Wallet balance
  M               Current location

Toggle features:
  F1              Magnet auto-collect on/off
  F2              Scan for buried items nearby
  F3              Auto-catch mode on/off (bug catching)
  F5              Repeat last spoken text

Furniture mode (indoors only):
  F               Toggle Furniture Mode on/off
  Shift+F         Announce wallpaper and carpet
  R               Room overview (list all furniture)
  Arrow keys      Move grid cursor (while Furniture Mode is on)

Navigation system:
  Shift+L         Next category (Villagers, Buildings, Houses, Items)
  Shift+J         Previous category
  L               Next item in category
  J               Previous item in category
  K               Directions to selected target (A* pathfinding)
  Shift+K         Full pathfinding with turn-by-turn directions


Settings Files
--------------

keybindings.ini
  Maps keyboard keys to GameCube controller buttons. Open this file in
  a text editor to remap controls. Key names use SDL2 scancode names.
  The file contains comments explaining the format.

settings.ini
  Graphics settings: window size, fullscreen mode, vsync, anti-aliasing,
  and texture preloading. Edit this file to change display settings.


Known Limitations
-----------------

These features are not yet implemented or have known issues:

- Native OS text input (bypassing the on-screen grid keyboard) is not
  yet available. You must use the in-game keyboard for typing letters,
  names, and other text. The on-screen keyboard is narrated, but a
  native input method would be faster.

- The catalog browser, music browser, and needlework/design editor do
  not yet have full narration.

- The save screen dialogue (Gyroid at the phone) may not always be read
  aloud.

- NES console mini-games inside the game are not accessible.

- Multiplayer is not supported (the base port does not support it).

- There is no in-game accessibility options menu. All accessibility
  features activate automatically when a screen reader is detected.

- Accessibility hotkeys are hardcoded and cannot be remapped through
  keybindings.ini. Only the base game controls (movement, buttons) can
  be remapped.

- The navigation system uses four cardinal directions only (north,
  south, east, west). Diagonal directions are not used, as cardinal-only
  directions are clearer for screen reader navigation.

- Fishing, bug catching, and buried item digging have not been fully
  tested in this release. Feedback on these features is especially
  welcome.


Troubleshooting
---------------

If audio sounds slow or dialogue timing feels off, open the in-game
Options menu from the title screen and try switching to windowed mode
with vsync enabled. You can also edit settings.ini directly — set
fullscreen to 0 and vsync to 1. Only adjust settings if you are
experiencing a specific issue. The defaults work well for most systems.

If the title screen seems unresponsive at first, wait for the opening
animation to finish before pressing any keys. The menu will appear
after a few seconds and your screen reader will announce it.


Credits and Acknowledgements
----------------------------

This accessibility mod was built on top of the work of many people:

Animal Crossing GameCube Decompilation
  By the ACreTeam (https://github.com/ACreTeam/ac-decomp)
  The decompilation project that reversed the original game code into
  readable C, making mods like this possible.

Animal Crossing GameCube PC Port
  By FlyingMeta (https://github.com/flyngmt/ACGC-PC-Port)
  The PC port that runs the decompiled game natively on Windows with
  modern rendering, keyboard controls, and quality-of-life features.

Tolk Screen Reader Abstraction Library
  By Davy Kager
  The library that bridges this mod to NVDA, JAWS, Narrator, and other
  screen readers through a single API.

Accessibility Mod
  By Lethal
  Designed and built for the blind gaming community.


Legal Notice
------------

This mod contains no copyrighted game assets. It is a modified build of
the open-source PC port with accessibility features added. To play, you
must provide your own legally obtained copy of Animal Crossing for
Nintendo GameCube (USA version, game ID GAFE01) in disc image format.

Animal Crossing is a trademark of Nintendo. This project is not
affiliated with or endorsed by Nintendo.
