---
title: Galaxy Android - Control Schemes and Input Mapping
description: Complete input reference guide for Galaxy Android, detailing touch controls, pointer tracking, physical Bluetooth/USB gamepads, and custom layout configuration.
tags:
  - android
  - controls
  - gamepad
  - touch-overlay
  - star-pointer
  - input-mapping
  - galaxy-android
---

# Galaxy Android - Controls & Input Guide

> **Acknowledgements & References:**
> Galaxy Android builds upon the Wii controller abstractions reconstructed in the [Petari Decompilation Project](https://github.com/SMGCommunity/Petari), the custom platform input layer pioneered by [bigmak94/GalaxyQuest](https://github.com/bigmak94/GalaxyQuest), and the mobile touch overlay implementations from [sebsies/GalaxyMobile](https://github.com/sebsies/GalaxyMobile). We extend our deepest thanks to both projects and their contributors.

Galaxy Android translates Android touchscreen interactions and standard Bluetooth/USB controller events into native Wii Remote + Nunchuk signals with zero emulation latency.

---

## 1. On-Screen Touch Controls

The on-screen overlay is engineered with a modern frosted translucent dark glass aesthetic with neon tactile micro-accents.

### Touch Layout Overview

| On-Screen Element | Wii Equivalent | Super Mario Galaxy Action |
|---|---|---|
| **Analog Thumbstick** (Left) | Nunchuk Analog Stick | Mario 360° movement, swimming, rolling |
| **A Button** (Cyan `#38BDF8`) | Wii Remote **A** | Jump, confirm in menus, talk, hold to fast-forward dialogue |
| **SPIN Button** (Emerald `#34D399`) | Wii Remote Shake | Star Spin attack, spin jump, activate Launch Stars |
| **B Button** (Amber `#FBBF24`) | Wii Remote **B** (Trigger) | Shoot collected Star Bits at enemies / obstacles |
| **Z Button** (Violet `#A78BFA`) | Nunchuk **Z** Button | Crouch, ground pound (in air), long jump / backflip (with A) |
| **Camera D-Pad** (Dial) | Wii Remote **D-Pad** | Rotate camera (Left/Right), first-person view (Up), reset view (Down) |
| **Pause Button (+)** | Wii Remote **+** | Pause menu, galaxy star checklist, options |
| **Screen Surface Drag** | Wii Remote IR Pointer | 1:1 direct finger aim: gather Star Bits, activate Pull Stars |

---

### Precision Reticle Joystick & Laser Tether

- **Dynamic Floating Mode (Default):** Tap and drag anywhere on the left third of your screen. The thumbstick anchors dynamically under your finger.
- **Fixed Mode:** Locks the joystick in a fixed physical position on screen (can be repositioned in the Layout Editor).
- **Luminous Laser Tether:** A vector laser line connects the joystick center to the thumb puck, providing immediate visual feedback of analog angle and deflection magnitude without screen clutter.

---

### Star Pointer & Interactive Touch Tracking

- **Direct Touch Gathering:** Sweep your finger over Star Bits floating in space to pull them directly into your inventory.
- **Pull Stars:** Touch and hold Pull Stars directly on screen to grapple Mario toward them.
- **Star Bit Shooting:** Tap the **B** button to fire Star Bits toward Mario's facing direction, or tap-and-hold the screen with a second finger to rapid-fire directly at specific targets.

---

### Directional Camera Dial

- Replicates the Wii Remote D-Pad camera controls:
  - **Left / Right Chevrons (◀ ▶):** Orbit the camera 45° around Mario.
  - **Top Chevron (▲):** Switch to Mario's first-person perspective.
  - **Bottom Chevron (▼):** Recenter the camera behind Mario.
  - **Center Camera Shutter Hub:** Snaps camera orientation directly behind Mario.
- Fully toggleable in settings (can be placed on either the left or right side).

---

## 2. Physical Gamepads (Bluetooth & USB)

Galaxy Android automatically detects physical gamepads connected via Bluetooth or USB-C (including Xbox Wireless, PlayStation DualShock 4 / DualSense, Nintendo Switch Pro Controller, Razer Kishi, and Backbone One).

### Standard Gamepad Mapping

| Gamepad Input | Wii Remote / Nunchuk | In-Game Action |
|---|---|---|
| **Left Analog Stick** | Nunchuk Stick | Mario 360° Movement |
| **Right Analog Stick** | Wii Remote Pointer | Analog Star Pointer aiming |
| **A / Cross (✕)** | Wii Remote **A** | Jump, talk, confirm |
| **X / Square (□)** | Wii Remote Shake | Star Spin attack |
| **Right Trigger (RT / R2)** | Wii Remote **B** | Shoot Star Bits |
| **Left Trigger (LT / L2)** | Nunchuk **Z** | Crouch, ground pound, long jump |
| **Left Bumper (LB / L1)** | Nunchuk **C** | Recenter camera behind Mario |
| **Right Bumper (RB / R1)** | Wii Remote Shake | Spin attack (alternative) |
| **D-Pad (Left / Right)** | Wii Remote D-Pad (L/R) | Rotate camera |
| **D-Pad Up** | Wii Remote D-Pad Up | First-person view |
| **D-Pad Down** | Wii Remote D-Pad Down | Recenter camera / Page down |
| **Start / Options / +** | Wii Remote **+** | Pause menu |
| **Select / Share / −** | Wii Remote **−** | Pause menu (alternative) |

---

## 3. Motion & Tilt Controls

Special stages (such as the **Star Ball** and **Surfing Ray**) originally relied on the Wii Remote's built-in accelerometer:

1. **Touch Virtual Tilt (Default):** Touch and slide on the virtual direction ring to steer the Star Ball and Ray with responsive precision.
2. **Device Gyroscope (Supported Devices):** Uses Android's internal `ASensorManager` gyroscope. Tilt your phone naturally to balance the Star Ball or carve turns on the Surfing Ray.

---

## 4. Customizing Controls & Layout Editor

Open the slide-out **Material 3 Settings Drawer** by tapping the gear icon (`⚙`) at the top-left edge of the screen:

- **Touch Controls Visibility:** Toggle on/off entirely when using a physical gamepad.
- **Button Opacity:** Adjust between 25%, 50%, 75%, and 100% transparency.
- **Button Scale:** Scale all touch buttons between 0.75x and 1.5x for phones or tablets.
- **Layout Editor:** Tap **"Edit On-Screen Layout"** to drag any button or joystick to your preferred position. Positions are saved permanently to `settings.ini`.

---

## References

- [Technical Documentation](TECHNICAL.md) - Architecture of the native port
- [Installation Guide](INSTALLATION.md) - How to install and play
- [Petari Repository](https://github.com/SMGCommunity/Petari) - Super Mario Galaxy decompilation
- [GalaxyQuest Repository](https://github.com/bigmak94/GalaxyQuest) - Standalone port architecture
