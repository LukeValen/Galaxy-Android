# Galaxy Android

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
[![Platform: Android ARM64](https://img.shields.io/badge/Platform-Android%20ARM64-brightgreen.svg)](docs/TECHNICAL.md)
[![Graphics: OpenGL ES 3.2](https://img.shields.io/badge/Graphics-OpenGL%20ES%203.2-orange.svg)](docs/TECHNICAL.md)
[![Audio: AAudio](https://img.shields.io/badge/Audio-AAudio%20Low--Latency-purple.svg)](docs/TECHNICAL.md)

**Play Super Mario Galaxy natively on your Android phone or tablet.**

**Galaxy Android** is a **100% native Android ARM64 port** of *Super Mario Galaxy*, compiled directly from reconstructed C++ source code. Maintained by [OpenPort Labs](https://github.com/OpenPort-Labs/Galaxy-Android).

It is **not an emulator**: the game code executes natively on your phone's processor using OpenGL ES 3.2 and AAudio, delivering fluid 60/120 FPS performance, low battery consumption, and zero emulation overhead.

---

## Acknowledgements & Deep Gratitude

This project is made possible through the pioneering work of the community:

- **[bigmak94 / GalaxyQuest](https://github.com/bigmak94/GalaxyQuest)**: For the standalone mobile/ARM port architecture, memory reservation shims, and the little-endian asset cooker toolchain.
- **[Petari Decompilation Project](https://github.com/SMGCommunity/Petari)**: For the reverse-engineering and C++ decompilation of *Super Mario Galaxy*.

We express our deepest thanks to both projects and their contributors for keeping game preservation alive.

---

## Key Features

### True Native Hor+ Ultrawide (No Stretch, No Zoom)
- Dynamically adapts the 3D camera projection to modern mobile aspect ratios (such as 19.5:9, 20:9, and foldable aspect ratios).
- Horizontally expands field of view (Hor+) so you see more of the galaxy around Mario without vertical cropping.
- Keeps 2D HUD elements (health circle, coin counters, star bit count, dialogs) centered in a 16:9 safe zone so circular UI elements remain perfect circles and are never obscured by camera hole-punch cutouts or rounded screen corners.

### Frosted Glass Minimalist Touch Controls
- **Sleek Translucent Dark Glass Theme**: Semi-transparent frosted glass backplates with ultra-thin luminous perimeter rings and tactile depression feedback.
- **Dynamic Luminous Laser Tether Joystick**: Vector laser line connects the stick origin to your thumb knob, providing immediate visual feedback of analog magnitude and deflection angle. Supports dynamic floating stick anchor mode.
- **Directional Camera D-Pad**: Unibody circular navigation disc replicating the Wii Remote D-Pad for 45° camera rotation and recentering.
- **Direct 1:1 Star Pointer Finger Tracking**: Touch and drag anywhere on screen to collect and aim star bits.

### Physical Gamepad Support
- Full plug-and-play support for Bluetooth and USB gamepads (Xbox Wireless, PlayStation DualShock 4 / DualSense, Nintendo Switch Pro, Razer Kishi, Backbone One, etc.). See [Controls Guide](docs/CONTROLS.md) for full mappings.

### Google Material 3 Settings Drawer
- Slide-out drawer docked cleanly on the top-left edge of the screen (`⚙`).
- Native Hor+ Ultrawide toggle, button opacity, button scaling, joystick modes, and drag-and-drop touch layout editor.

### Cinematic Cutscenes & Low-Latency Audio
- Software-decoded THP video playback for the prologue and ending cinematics.
- Real-time DSP mixer running through Android's low-latency AAudio subsystem.

---

## System Requirements

| Specification | Minimum Requirement | Recommended |
|---|---|---|
| **Operating System** | **Android 11 or newer (API Level 30+)** | Android 12, 13, 14, or 15 |
| **CPU Architecture** | 64-bit ARM (`arm64-v8a`) | 64-bit ARM (`arm64-v8a`) |
| **Graphics API** | OpenGL ES 3.2 | OpenGL ES 3.2 |
| **Audio Subsystem** | AAudio | AAudio |
| **RAM** | 3 GB | 4 GB or higher |
| **Storage** | ~2.5 GB (without movies) / ~5 GB (with movies) | Fast UFS internal storage |

---

> [!IMPORTANT]
> **Legal Notice:**
> **You must provide your own legally dumped copy of Super Mario Galaxy.** Neither this repository nor the compiled APK contains any copyrighted Nintendo game assets, levels, audio, textures, or cinematics. You extract and convert your own legally owned disc once on your phone or PC.

---

## Quick Start: How to Run on Android

For detailed instructions, see the complete [Installation Guide](docs/INSTALLATION.md).

### Method 1: 100% PC-Less Setup on Your Phone (Easiest)

With the dedicated **GalaxyInstaller** app, your phone does everything on-device:

1. **Place your game dump on your phone**:
   - Copy your legally dumped game disc (`.rvz`, `.wbfs`, `.iso`, or `.wia`) into your phone's **`Download`** folder.
2. **Download and install the APKs from [Releases](https://github.com/OpenPort-Labs/Galaxy-Android/releases/latest)**:
   - Main Game: **`GalaxyAndroid.apk`** (or `GalaxyMobile.apk`)
   - On-Device Cooker: **`GalaxyInstaller.apk`**
3. **Open GalaxyInstaller**:
   - Grant storage permissions. The app automatically detects your game dump in `Download`.
   - Tap **"Start Cooking Game Data"**.
4. **Launch & Play**:
   - Tap **"Launch Game"** and enjoy native 60 FPS Super Mario Galaxy on Android!

---

### Method 2: PC Toolchain (USB / ADB)

1. **Extract your game disc** with Dolphin Tool into `extracted/`:
   ```bash
   tools/dolphin/DolphinTool.exe extract -i "Super Mario Galaxy.rvz" -o extracted
   ```
2. **Cook assets to little-endian format**:
   - Windows: `py tools\cook\cook.py extracted cooked --with-movies`
   - Linux/macOS: `python3 tools/cook/cook.py extracted cooked --with-movies`
3. **Stream data to phone**:
   - Windows: Run [`transfer_game.bat`](transfer_game.bat)
   - Cross-Platform: `py tools/push_data.py cooked`
4. **Deploy APK**:
   - Windows: Run [`deploy_to_phone.bat`](deploy_to_phone.bat)
   - ADB: `adb install -r out-mobile/GalaxyMobile.apk`

---

## Documentation

- [Controls & Input Reference](docs/CONTROLS.md) - Touch overlay, pointer aim, and gamepad layouts
- [Technical Architecture](docs/TECHNICAL.md) - 32-bit address space reservation, GLES 3.2 translation, and Hor+ ultrawide
- [Installation Guide](docs/INSTALLATION.md) - Comprehensive setup instructions
- [Desktop Pipeline Guide](docs/DESKTOP_INSTALLER.md) - Automated PC extraction and conversion
- [Decompilation Notes](docs/decomp/getting_started.md) - Decompilation symbol mappings and splits

---

## Building from Source

### Requirements
- Android NDK `r25` or `r26`
- Android SDK (API 30+ and Build-Tools 34+)
- CMake 3.22+ and Ninja
- JDK 17+ and Python 3.8+

### Native Library & Packaging
```bash
# 1. Compile native engine library (libgame.so)
cmake --build build-mobile --target game

# 2. Package and sign APK
python package_mobile_apk.py
```
The output APK is generated at [`out-mobile/GalaxyMobile.apk`](out-mobile/GalaxyMobile.apk).

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. See the [`LICENSE`](LICENSE) file for details.

*Super Mario Galaxy is © Nintendo. This project is a non-commercial open-source preservation and research initiative.*
