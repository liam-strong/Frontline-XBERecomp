Medal of Honor: Frontline - XBE Recomp @VERSION@ (preview)
============================================================

An unofficial native PC port of the original Xbox version of Medal of Honor: Frontline. Not affiliated with
Electronic Arts. This download contains no game data and no game code: you need your own copy of the Xbox game.


Requirements
------------
- Windows 10 or 11, 64-bit
- A graphics card with Direct3D 12
- Room for your disc image (about 3 GB) in assets\, and about 1 GB more for the first start's build
- A controller (XInput) or keyboard and mouse


Installing
----------
1. Unzip this folder anywhere you can write to (not Program Files).
2. Put your Xbox disc image (.iso) into the assets folder. An image of the game partition (xiso) or a full redump
   both work. The recognized default.xbe is the USA retail rip. Other XBE hashes trigger a warning before starting.
   Keep only the copy you want to use in assets.
3. Start MOHF.exe. The first start translates the game's code from your disc image and builds it into MOHF.dll
   behind a progress window, which takes a few minutes. Later starts go straight to the game. If the build is
   interrupted, the next start carries on where it stopped.


Controls
--------
Controllers work as on the Xbox. Keyboard and mouse follow the game's own default pad layout. In single player,
keyboard/mouse and the first controller both control the player; switch between them at any time.

  Move                  W A S D          Walk (hold)           Alt
  Fire                  Left mouse       Aim                   Right mouse
  Use                   E, F             Next weapon           Q, Mouse 4
  Previous weapon       G                Jump                  Space
  Reload                R                Crouch                C, Ctrl
  Melee                 V, Middle mouse  Center view           Z
  Sniper zoom           Wheel, 1 and 2   D-pad left/right      3, 4
  Back                  Tab              Pause                 Escape

F1 opens graphics and saved places. F2 opens controls. Gameplay bindings can be changed on the F2 page and in the
[keys] table of settings.toml.


Settings
--------
F1 changes the display mode, resolution, render scale, anisotropic filtering, VSync, frame cap, widescreen, field
of view and the FPS counter. Mouse sensitivity and mouse inversion are on F2. Saved spots are kept in places.toml
beside the executable.

Everything is also in settings.toml beside the executable, written on the first start. Some settings are only there:

  load_last_save = true         start from the newest save; the menu opens when this is off or there is no save
  skip_intros = true            no EA logo and opening movie at start
  skip_cutscenes = true         no level intro and outro movies
  mouse_vertical_sensitivity    vertical mouse speed, on top of mouse_sensitivity
  invert_mouse_x                mirrored horizontal mouse look
  walk_speed = 50               percent of running speed while Walk is held
  width / height                output resolution, 0 for the desktop's
  fov = 70                      horizontal degrees on a 4:3 frame; widescreen adds Hor+
  anisotropic_filtering         0 as the game asks, else a level such as 2, 4, 8 or 16


Files
-----
  assets\        your disc image
  hdd\           the Xbox hard disk: profiles and saved games (hdd\E\UDATA\4541001a)
  settings.toml  settings
  places.toml    saved spots from the F1 page
  logs\          logs of the last runs
  CrashDumps\    written if the game crashes
  compiler\      the tools of the first start's build

To move to a new version, keep hdd\, settings.toml and places.toml, and replace the rest.


Known limits of this preview
----------------------------
- Single player. There is no co-op executable, and multiplayer over a network and Xbox Live are not supported.
- A launch continues the newest save when load_last_save is on.


Reporting problems
------------------
Please include the log from logs\ (and the crash dump from CrashDumps\ if there is one), the mission and what you
were doing.


Credits
-------
Built with Xbox XBE Recomp by TGP482 (https://github.com/TGP482/XboxXBE-Recomp), modelled on BLACK-XBERecomp.
This port is under the MIT License (LICENSE.txt), the SDK under its own (LICENSE-XboxXBE-Recomp.txt); third party
licenses are in THIRD_PARTY.txt and compiler\LLVM-LICENSE.txt. The game and its data belong to Electronic Arts.
