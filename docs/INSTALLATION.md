---
title: Galaxy Android - Comprehensive Installation & Setup Guide
description: Step-by-step instructions for installing and running Galaxy Android on Android devices, covering 100% PC-less on-device setup and PC desktop cooking pipelines.
tags:
  - android
  - installation
  - setup
  - tutorial
  - galaxy-installer
  - adb
  - cooking
  - galaxy-android
---

# Galaxy Android - Installation & Setup Guide

This guide explains how to install and play **Galaxy Android** on your Android phone or tablet.

> **Acknowledgements & Foundation:**
> The cooking and extraction pipeline is based on the toolchain developed by [bigmak94/GalaxyQuest](https://github.com/bigmak94/GalaxyQuest) and the decompilation efforts of [Petari](https://github.com/SMGCommunity/Petari). Special thanks to both teams for making native ports possible.

> **Legal Disclaimer:**
> You must provide your own legally dumped copy of *Super Mario Galaxy* (USA or EUR optical disc image). This project does not distribute any proprietary Nintendo game assets, binaries, or copyrighted data.

---

## System Requirements

- **Android Version:** Android 11 or newer (API Level 30+).
- **Architecture:** 64-bit ARM (`arm64-v8a`).
- **RAM:** Minimum 3 GB (4 GB+ recommended).
- **Free Storage:**
  - Game data without cinematics: ~2.5 GB.
  - Full game with cinematics (movies): ~5.0 GB.

---

## Method 1: 100% PC-Less On-Device Setup (Recommended)

Using the companion **GalaxyInstaller** app, your phone extracts, decrypts, and converts your game disc directly on-device with zero PC required.

### Step 1: Place Your Disc Image on Your Phone
1. Transfer or download your legally dumped Super Mario Galaxy disc into your phone's **`Download`** folder.
2. Supported disc formats:
   - **`.rvz`** (Modern compressed Dolphin format - recommended)
   - **`.wbfs`** (Wii Backup File System format)
   - **`.iso`** (Standard uncompressed Wii disc image)
   - **`.wia`** / **`.ciso`** / **`.zip`**

### Step 2: Install the APKs
Download or copy the APKs from [`out-mobile/`](../out-mobile/) to your phone and install them:
- **`GalaxyMobile.apk`** (`com.galaxy.mobile`): The main native game engine.
- **`GalaxyInstaller.apk`** (`com.galaxy.installer`): The on-device asset cooker.

### Step 3: Run GalaxyInstaller
1. Launch **GalaxyInstaller** from your app drawer.
2. Grant requested storage access permissions.
3. The app will auto-detect your game disc in your `Download` folder.
4. Check **"Include Cutscene Movies"** if you want prologue and ending cinematics.
5. Tap **"Start Cooking Game Data"**.
6. When cooking finishes (usually 1-3 minutes), tap **"Launch GalaxyMobile"** to begin playing!

---

## Method 2: PC Desktop Workflow (USB / ADB)

If you prefer to cook game assets on your computer for faster processing:

### Prerequisites on PC
- Windows, macOS, or Linux
- [Python 3.8+](https://www.python.org/)
- [Android Platform-Tools (`adb`)](https://developer.android.com/tools/releases/platform-tools)
- [Dolphin Emulator](https://dolphin-emu.org/) (for extracting optical disc partitions)

### Step 1: Enable USB Debugging on Phone
1. Go to **Settings > About phone** and tap **Build number** 7 times.
2. Navigate to **Developer options** and turn on **USB debugging**.
3. Connect your phone via USB cable and tap **Always allow from this computer**.

### Step 2: Extract Your Game Disc with Dolphin
Using the command-line tool in [`tools/dolphin/`](../tools/dolphin/):
```bash
tools/dolphin/DolphinTool.exe extract -i "path/to/Super Mario Galaxy.rvz" -o extracted
```
*(This creates an `extracted/DATA` folder containing `files` and `sys`.)*

### Step 3: Cook Assets to Little-Endian
From the project root:
- **Windows:**
  ```cmd
  py tools\cook\cook.py extracted cooked --with-movies
  ```
- **macOS / Linux:**
  ```bash
  python3 tools/cook/cook.py extracted cooked --with-movies
  ```

### Step 4: Stream Data to Device
- **Windows (1-Click):**
  Double-click [`transfer_game.bat`](../transfer_game.bat)
- **Manual Command:**
  ```cmd
  py tools\push_data.py cooked
  ```

### Step 5: Install APK and Launch
```bash
adb install -r out-mobile/GalaxyMobile.apk
adb shell am start -n com.galaxy.mobile/android.app.NativeActivity
```

---

## References

- [Technical Details](TECHNICAL.md) - Architecture and memory shims
- [Controls Guide](CONTROLS.md) - Touch and gamepad layout
- [Petari Repository](https://github.com/SMGCommunity/Petari)
- [GalaxyQuest Repository](https://github.com/bigmak94/GalaxyQuest)
