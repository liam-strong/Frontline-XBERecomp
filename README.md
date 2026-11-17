# Medal of Honor: Frontline - XBE Recomp

An unofficial native PC port of the original Xbox version of Medal of Honor: Frontline, in progress.

The game's XBE is statically recompiled from x86 to C++ and built into a native 64-bit Windows executable with the
[Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) SDK, vendored in `sdk/`. The Xbox kernel and XDK
libraries run as host code, graphics go through Direct3D 12 and audio through XAudio2. This tree was started from
the [Rising Sun port](../MOHRS-XBERecomp-main) and keeps that port's host code; Frontline's own image, hooks and
settings live here.

> [!NOTE]
> This project does not include any game assets or recompiled game code. You must provide the files from your own
> legally acquired copy of the game to build or run it.

## Status

Single-player boots and runs with graphics, audio, saves, controller and keyboard/mouse support. Frontline ships
one image, `default.xbe`, as `MOHF.exe`. Title ID `4541001A`, XDK **4928** (`config/game.toml`).

Keyboard and mouse follow the game's own default pad layout (the table at `0x145030`): A use, B crouch, X reload,
Y jump, Black next weapon, White previous weapon, left trigger aim, right trigger fire. Bindings are in
`settings.toml` under `[keys]` and on the F2 page. A layout chosen in the game's own options changes what a
button does.

| Action | Default |
|---|---|
| Move | W A S D |
| Walk (hold) | Alt |
| Fire / aim | Left mouse / right mouse |
| Use | E, F |
| Next / previous weapon | Q, Mouse 4 / G |
| Reload / crouch | R / C, Ctrl |
| Jump / melee | Space / V, middle mouse |
| Center view | Z |
| Sniper zoom | Wheel, 1 and 2 |
| D-pad left / right | 3 / 4 |
| Back / pause | Tab / Escape |

F1 opens graphics and saved places, F2 opens controls. Graphics options are the ones this renderer applies on
NVIDIA and AMD: display mode, resolution, render scale, anisotropic filtering, vsync, frame cap, widescreen,
field of view (70 is the original 4:3 view; widescreen adds Hor+), and the FPS counter. Mouse sensitivity and
invert are on the controls page. Saved spots are kept in `places.toml` beside the executable.

On foot, mouse look is added at `0x6351A` before the stick rates reach yaw (`player+0x3E8`) and pitch
(`player+0x3EC`). On a mounted gun, `MountedLook` at `0x62A4B` does the same for the gun's aim (`+0x528` /
`+0x52C`). The game still clamps that aim to its own limits. Reading the mouse there also keeps the mouse
buttons live, so left click fires while mounted.

A launch continues the newest save (`load_last_save` in `settings.toml`, on by default, and on the F1 page).
That save is the `game.xsv` written most recently. With the setting off, or with no save on disk, the game
opens on the menu. A boot that is already carrying a mission keeps that mission.

Switching or reloading a weapon used to paint the near ground a flat colour (blue, red, yellow, purple or
black) until the next shot. `CreateVertexShader` left the program length empty, and `GetVertexShaderSize`
reads that field. Frontline places its two world shaders back to back from that length, so the second was
loaded on top of the first and the level was drawn with a point-light program reading leftover bone constants.
The shader record now stores the instruction count at offset 8 (`sdk/runtime/xdk/d3d8.cpp`).

See [field of view](docs/fov.md), [frame timing and settings](docs/frame-timing.md),
[controller routing](docs/controller-routing.md), [performance measurements](docs/performance.md),
[PC Options](docs/pc-options.md), the [menu mouse adapter](docs/menu-mouse.md) and
[docs/PLAN.md](docs/PLAN.md). Those notes were written for the Rising Sun port; the links stay because the
shared host code is the same. Frontline's own addresses are in `config/game.toml` and `src/game/hooks.cpp`.

## Repository layout

| Path | What |
|---|---|
| `sdk/` | XboxXBE-Recomp, vendored and patched here (base commit in `sdk/UPSTREAM.txt`) |
| `config/game.toml` | Recompiler config for `default.xbe`: symbols, data, hooks |
| `src/common/` | Host code shared by every image (`game_info.cpp` describes the game to the runtime) |
| `src/game/` | Hooks for `default.xbe` (`MouseLook`, `MountedLook`, `SetFov`) |
| `tools/` | `xiso.py` (list/extract XDVDFS images), `xbe_info.py` (XBE header, XDK versions, sections), `viv.py` (list/extract EA .viv archives), `dev.bat` (toolchain environment), RE helpers `disasm.py`, `lib_inventory.py`, `symbols_compare.py`, `hle_audit.py`, `dsp_image_check.py` (see [docs/xdk5455.md](docs/xdk5455.md)) |
| `docs/` | Plan and notes carried from the Rising Sun port |
| `assets/` | Your game files (ignored by git) |
| `generated/`, `out/` | Recompiled code and build output (ignored by git) |

## Building

### 1. Game files
Put your disc image in `assets/` and extract the XBE from it into `assets/xbe/`:
```
python tools/xiso.py xbes "assets/Medal of Honor Frontline.iso" assets/xbe
```
The build reads `assets/xbe/default.xbe`; the game itself runs from the `.iso` in `assets/`. Keep the XBE out of
`assets/` itself, or the runtime will look for loose data files beside it.

The recognized `default.xbe` is the USA retail rip. Its SHA-1 is
`41c0a70f18ad48b39865df4b9854f773ad950ee2`.

### 2. Dependencies
Visual Studio 2022 with **Desktop development with C++**, **C++ Clang Compiler for Windows** and **C++ CMake tools
for Windows**. `build.bat` offers to install the Clang and CMake components if they're missing. Python 3 for the
tools.

### 3. Build
Run `build.bat`. It writes `config/game.toml` with `xbe_symbols` on the first build, recompiles the XBE and builds
`out\build\release\MOHF.exe`.

The build extracts the original title logo from your XBE and embeds it as the Windows icon. Generated icon files
stay in the build folder; no extra Python packages are needed.

The version is `project(... VERSION)` in `CMakeLists.txt` (currently 0.0.1). It is in the executable's file
properties and on the first line of `logs\mohf.log`.

## License

This repository's own code is under the [MIT License](LICENSE). The vendored SDK in `sdk/` has its own MIT license
(`sdk/LICENSE`) and lists its third party code in `sdk/THIRD_PARTY.txt`. Medal of Honor: Frontline, its code and
its data belong to Electronic Arts and are not covered; this project is not affiliated with or endorsed by EA.

## Credits

- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) and [BLACK-XBERecomp](https://github.com/TGP482/BLACK-XBERecomp)
  by TGP482, which this port is built on and modelled after. See `sdk/README.md` and `sdk/THIRD_PARTY.txt`.
- The Rising Sun port in `MOHRS-XBERecomp-main`, which supplied the shared host, the settings, and the notes in `docs/`.
