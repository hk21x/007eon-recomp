# 007: Everything or Nothing - XBE Recomp

An unofficial native PC port of the original Xbox version of 007: Everything or Nothing, with extended graphics options and other enhancements.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) tool.

> [!NOTE]
> This project does not include any game assets. You must provide the files from your own legally acquired copy of the game to install or build.

## Installation

- Put your game files in the `assets` folder. This can be an ISO, or the extracted data files.
- Run `EoN.exe`.
- Recomp settings can be adjusted in `settings.toml` beside the exe.

## Features

### Display
- **Widescreen** - The game's own 16:9 mode on a widescreen display.
- **Ultrawide** - With `aspect = "screen"`, the 3D view widens to fill a 21:9 or wider screen; the HUD and menus keep their 16:9 shape in the centre.
- **Widescreen Driving** - The driving missions, 4:3 on the console, widen to the screen's shape too; their HUD keeps its 16:9 shape in the centre.
- **Field of View** - `fov_percent` (50 to 150, 100 the game's) widens or narrows the view on foot.

### Graphics

### Controller
Supports Xbox, PlayStation and Switch controllers, with hot plugging and rumble.

### Keyboard and Mouse
- **Mouse aiming** - The mouse turns the camera and aim directly (not through the stick's curve), with sensitivity and invert options; the cursor stays in the window while playing.
- **Keyboard prompts** - While you play on the keyboard and mouse, the game's button prompts show the keys and mouse buttons bound to them; they switch back when you pick up the controller.
- **Default layout** - WASD to move, left mouse button to fire, right mouse button to lock on, Space to jump or confirm, Q and E to punch left and right, C for wall cover, F for the White button, 1 to 4 or the arrow keys for the d-pad (weapons and gadgets), Esc to pause.
- Every key and mouse button can be rebound in the settings window (F1, Controls tab).

### General
- **Modern Button Pictures** - The prompts show today's Xbox pad: A B X Y in the modern style, LB and RB in place of the White and Black buttons, LT and RT, and the Menu and View buttons in place of Start and Back (`modern_buttons`, on by default). They are drawn at four times the console's resolution, so they stay sharp at 1440p and 4K.
- **60 FPS Driving** - The driving missions draw at 60 frames a second instead of 30, the game's timing unchanged (`driving_60fps`, on by default).
- **Skip Intro Movies** - `skip_intro = true` goes past the THX, MGM and EA logos at start.
- **Cutscenes** - A PAL disc's cutscenes play (the game runs as on a PAL console at 60 Hz).

### Miscellaneous
- **Boot Splash** - Place a `splash.png` (or `.bmp`) beside the exe to display a splash screen at launch.

## System Requirements

### Minimum
- **OS**
  - Windows 10 64-bit
- **GPU** with support for Direct3D 12

## Building

### 1. Clone the Repository
Clone this repository using [Git](https://git-scm.com/).

### 2. Add the Required Game Files
Place your extracted game data files in the `assets` folder. `default.xbe` must be there, with the game's folders beside it.

### 3. Install Dependencies
Install Visual Studio 2022 with **Desktop development with C++**, **C++ Clang tools for Windows** and **C++ CMake tools for Windows** (`build.bat` offers to add the last two).

### 4. Build the Project
Run `build.bat`. The first build recompiles the game and takes a while. The exe is written to `out\build\release`.

## Credits

- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp), the recompiler and runtime this is built on.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
