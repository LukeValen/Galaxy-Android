---
title: Galaxy Android - Technical Architecture & Engineering Deep-Dive
description: In-depth technical architecture of Galaxy Android, detailing the ARM64 native porting process, 32-bit address space reservation, OpenGL ES 3.2 graphics pipeline, Hor+ ultrawide rendering, and asset cooking.
tags:
  - architecture
  - android
  - arm64
  - opengl-es
  - decompilation
  - technical
  - reverse-engineering
  - petari
  - galaxy-quest
  - galaxy-android
---

# Galaxy Android - Technical Architecture

Galaxy Android is a **100% native Android ARM64 port** of the classic 2007 gravity platformer. It is not an emulator; the original game logic and engine routines execute directly on the device's CPU cores using native Android APIs.

> **Acknowledgements & Foundation:**
> This project builds upon the foundational reverse-engineering and decompilation work of the [Petari Decompilation Project](https://github.com/SMGCommunity/Petari) and the groundbreaking standalone mobile platform architecture developed by [bigmak94/GalaxyQuest](https://github.com/bigmak94/GalaxyQuest). We express our utmost gratitude to these communities and creators.

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Android System (OS Layer)                   │
├───────────────────────────────┬─────────────────────────────────┤
│    NativeActivity (Java)      │     wrap.sh & libreserve.so     │
├───────────────────────────────┴─────────────────────────────────┤
│            libgame.so (Galaxy Android Native Engine)            │
│  ┌──────────────────────┬─────────────────────────────────────┐ │
│  │   Original Gameplay  │         Android Platform Layer       │ │
│  │   & Scene Logic      ├─────────────────────────────────────┤ │
│  │   (Decompiled C++)   │  - GLES 3.2 GX Renderer             │ │
│  │                      │  - AAudio Low-Latency Mixer         │ │
│  │  MEM1 (0x80000000)   │  - Multi-touch AInputQueue Engine   │ │
│  │  MEM2 (0x90000000)   │  - Hor+ Ultrawide Camera Mod        │ │
│  │                      │  - THP Video Decoder Engine         │ │
│  └──────────────────────┴─────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│       Cooked Game Assets (/sdcard/GalaxyData/game)              │
│       (Little-endian textures, models, archives, audio)         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. The 32-Bit Pointer Memory Window Challenge

### The Problem
The Wii hardware (Broadway PowerPC) used a 32-bit physical address space:
- **MEM1 (Fast SRAM):** Base `0x80000000` (24 MB)
- **MEM2 (GDDR3 RAM):** Base `0x90000000` (64 MB)

The original game code routinely casts pointers directly to 32-bit unsigned integers (`u32`) and relies on pointer arithmetic within these specific address ranges. On a 64-bit Android OS (`arm64-v8a`), the Linux kernel and the dynamic linker allocate memory across a 48-bit virtual address space (frequently above `0x7000000000`). Truncating a 64-bit pointer to `u32` leads to immediate segmentation faults.

### The Solution: Address Space Reservation
Galaxy Android uses a two-stage preloading strategy to guarantee that `0x80000000` through `0xA0000000` is strictly reserved and available:

1. **`wrap.sh` Launcher Hook:**
   - Android permits an application package to include a `wrap.sh` script inside `lib/arm64-v8a/`.
   - The OS executes `wrap.sh` before initializing the Android Runtime (ART) process.
   - `wrap.sh` exports `LD_PRELOAD` pointing to a tiny helper library: [`libreserve.so`](../platform/mobile/libreserve.so).

2. **`libreserve.so` Constructor:**
   - Runs before any shared libraries or runtime threads are created.
   - Calls `mmap(reinterpret_cast<void*>(0x80000000), 0x20000000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0)`.
   - This reserves the entire 512 MB memory window from `0x80000000` to `0xA0000000`, preventing the system linker from putting standard Android libraries or heap structures in that area.

3. **Loading `libgame.so`:**
   - The platform loader uses `android_dlopen_ext` with `ANDROID_DLEXT_LOAD_AT_FIXED_ADDRESS` to place the game engine library starting at `0x98000000`.
   - MEM1 (`0x80000000`) and MEM2 (`0x90000000`) are mapped as direct read/write memory pools matching original Wii addresses exactly.

---

## 3. Graphics Subsystem: GX to OpenGL ES 3.2

The GameCube/Wii utilized the **GX** fixed/programmable pipeline, featuring up to 16 Texture Environment (TEV) stages, indirect texturing, alpha testing, and custom display list formats.

### Translation Architecture
- **Shader Generation:** TEV stages are dynamically compiled into modern GLSL ES 3.2 fragment shaders. Color/alpha blending registers (`c0`, `c1`, `c2`, `prev`) map to vector registers in the shader.
- **Indirect Texturing:** Wii matrix-driven coordinate warping (used extensively for water surfaces, heat haze, and star fields) is translated into dependent texture lookups in GLSL.
- **Display Lists:** Geometry streams are parsed into standard OpenGL ES Vertex Buffer Objects (VBOs) and Vertex Array Objects (VAOs).

### Hor+ Dynamic Ultrawide Projection
The original game supported 4:3 and anamorphic 16:9 aspect ratios. On modern smartphones (19.5:9, 20:9, 21:9):
- **Camera Frustum:** The 3D projection matrix is dynamically recalculated using the horizontal expansion method (Hor+). The vertical field of view remains constant, while horizontal FOV is expanded:
  $$\tan\left(\frac{\text{FOV}_h}{2}\right) = \tan\left(\frac{\text{FOV}_v}{2}\right) \times \text{Aspect Ratio}$$
- **2D HUD Safe Zone:** All 2D interface elements (health meter, coin counters, dialogs) are projected within a centered 16:9 safe region using OpenGL scissor boxes, preserving circular geometry and preventing obstruction by camera hole punches.

---

## 4. Asset Cooker Pipeline (`cook.py`)

Original Wii disc partitions contain big-endian files (`.arc`, `.bcsv`, `.kcl`, `.thp`, `.brres`). Byte-swapping every asset on a mobile CPU during gameplay would introduce stutter.

The asset cooker ([`tools/cook/cook.py`](../tools/cook/cook.py)):
1. **Recursively unpacks** all RARC archives and Yaz0-compressed archives.
2. **Converts big-endian binary tables (BCSV):** Headers, offsets, and row entries are rewritten into 64-bit little-endian binary formats.
3. **Collision Models (KCL):** Octree nodes, face planes, and attribute flags are converted to little-endian IEEE-754 floats.
4. **Textures (BTI / TPL):** CMPR (DXT1), RGB565, and RGBA8 textures are converted into formats natively compatible with mobile GPUs.
5. **Output Structure:** The result is placed in `cooked/sys/` and `cooked/files/`, ready for direct streaming into mobile storage.

---

## 5. Audio Subsystem (AAudio)

- Original audio routines relied on the Wii DSP mixer and AX voice manager.
- Galaxy Android implements a low-latency PCM software mixer that writes directly to an Android `AAudioStream`.
- Features:
  - 48,000 Hz, 16-bit stereo output.
  - Ring-buffer latency under 15 milliseconds.
  - Handles dynamic interactive music layers (e.g. adding brass or percussion instruments during speed-up or Yoshi riding).

---

## 6. Input & Android Lifecycle

- Managed through `android_native_app_glue` within `platform/mobile/mobile_app.cpp`.
- Event processing runs in a non-blocking `AInputQueue` loop during `APP_CMD_INPUT_CHANGED`.
- Touch points are tracked with unique finger pointer IDs to support multi-touch (simultaneous joystick movement, jump, spin attack, and screen pointer aiming).
- Gamepad events map `AMOTION_EVENT_AXIS_X/Y` and key codes (`AKEYCODE_BUTTON_A`, etc.) to Wii Remote / Nunchuk state registers.

---

## 7. Comparative Performance

| Metric | Emulation (Dolphin on Android) | Galaxy Android (Native Port) |
|---|---|---|
| **CPU Execution** | JIT Dynamic Recompiler | Native ARM64 Machine Code |
| **GPU Execution** | State-tracking overhead | Direct OpenGL ES 3.2 calls |
| **Memory Footprint** | ~1.8 GB RAM | ~380 MB RAM |
| **Battery Draw** | High (~12-15W) | Low (~4-6W) |
| **Frame Rate** | Variable 30-50 FPS on mid-range | Solid 60 / 120 FPS |

---

## References

- [Controls Reference](CONTROLS.md) - Input and controller guide
- [Installation Guide](INSTALLATION.md) - How to setup and play
- [Decompilation Reference](decomp/getting_started.md) - Petari decompilation notes
- [Petari Repository](https://github.com/SMGCommunity/Petari)
- [GalaxyQuest Repository](https://github.com/bigmak94/GalaxyQuest)
