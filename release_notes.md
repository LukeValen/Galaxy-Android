# GalaxyAndroid v0.0.1 — Initial Native Android Release

The first native ARM64 Android port of **Super Mario Galaxy**, executing compiled C++ game logic directly on Android hardware with zero emulation overhead.

---

### Highlights & Fixes in v0.0.1

#### 1. Overhauled Mobile Controls & Multi-Touch Input Engine
- **Exclusive Multi-Touch Pointer Tracking**: Rebuilt the touch input pipeline with a stateful pointer role tracking engine (`TouchRole`) supporting up to 10 concurrent fingers.
- **Mutually-Exclusive Nearest-Distance Matching**: Replaced overlapping hitboxes with exclusive Euclidean distance selection. A single touch can never trigger two buttons simultaneously (e.g. A + Spin or B + Z).
- **Eliminated Ghost Tap Injections**: Removed accidental `WPAD_BUTTON_A` press injections upon screen release during gameplay. Mario now jumps strictly when the A button is pressed.
- **Ergonomic Arc Layout**: Spaced out primary action buttons (A: Jump, B: Star Bit Shoot, Z: Crouch/Pound, SPIN: Shake/Cyclone, +: Pause) with comfortable thumb clearance matching rendering sub-pixels.
- **Directional Camera D-Pad**: Smooth camera orbit (◀ ▶) and recenter (▲ ▼) controls with customizable left/right dock positioning.

#### 2. Shading & Graphics Pipeline Fixes (Mali GPU & GLES 3.2)
- **Actor Lighting Fix**: Solved the actor over-saturation glitch where Mario and NPCs rendered glowing cyan/white. Added division-by-zero protection against `(0,0,0)` camera-origin rim lights (`GX_LIGHT2`) that caused `NaN`/`INF` light vectors on ARM Mali-G68 GPUs.
- **Water Refraction & EFB Distortion Mapping**: Corrected Y-axis screen coordinate mapping in `gl_shadergen.cpp`, fixing flipped texture distortion on water surfaces and heat shimmers.
- **GX Ambient & Material Registers**: Properly initialized default hardware state registers (`0x0A..0x0D`) matching Nintendo's `GXInit.c` specification.

#### 3. ⚡ Quick Save Anywhere
- **Save Anywhere In-Game**: Integrated native save routine into the engine sequence (`GameSequenceFunction::quickSaveAnywhere()`) saving Mario's progress, star bits, coin count, comet scheduler, and unlocked galaxies to NAND storage immediately without needing to return to the observatory.
- **Settings Drawer Quick Save**: Dedicated row in the Material 3 drawer (`⚙` -> "⚡ Quick Save Anywhere") triggering the save sequence.
- **Floating Toast HUD Banner**: Animated top pill toast notifying player: `"Game Progress Saved Anywhere!"`.

#### 4. Vulkan 1.1 Renderer & Frame-Replay Hardening
- **GX Frame Replay Hook**: Connected `update()` and `render()` to `gpu::frameQueue().latest()` to actively poll and execute game frame draw calls into offscreen Vulkan EFB targets.
- **Blit Format Capabilities & Filter Validation**: Added `vkGetPhysicalDeviceFormatProperties` checks for `VK_FORMAT_FEATURE_BLIT_SRC_BIT` / `VK_FORMAT_FEATURE_BLIT_DST_BIT` and linear filter support, falling back gracefully to `VK_FILTER_NEAREST` or clear pass.
- **Swapchain Transfer Support Verification**: Explicitly verifies `VK_IMAGE_USAGE_TRANSFER_DST_BIT` in surface capabilities before configuring swapchain images.
- **Comprehensive VkResult Error Checking**: Added status checks to `vkEndCommandBuffer`, `vkQueueSubmit`, and `vkQueuePresentKHR`, with robust swapchain recreation on `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR`.

#### 5. CI Build & Android Compatibility Fixes
- **Linux Case-Sensitivity Fixes**: Fixed `XanimeCore.hpp` and standardized all 12 `KariKariDirector.hpp` include paths across the codebase, resolving CI compiler failure on Ubuntu runners.
- **Gamepad & TV / Handheld Support**: Configured `android.hardware.touchscreen` to `android:required="false"` and enabled `android.hardware.gamepad`, bringing native support to Odin, Retroid Pocket, Logitech G Cloud, and Android TV.

#### 6. In-App Disc Extraction & SAF Storage Fixes
- **Resolved `pythonhome.zip` Extraction Blocker**: Whitelisted `pythonhome.zip` in `.gitignore` to prevent embedded Python standard library assets from being omitted in release builds. Added packaging assertions guaranteeing Python runtime presence.
- **Integrated Native Cooker (`libinstaller.so`)**: Added CMake build target for `installer` library with full JNI aliases for `com.openport.galaxyandroid`, enabling zero-setup in-app asset extraction.
- **SAF Document & Content URI Handling**: Resolved `content://` URIs when selecting `.rvz`, `.iso`, or `.wbfs` files via Android's file picker, resolving clean filenames and streaming images for extraction without skipping the disc unpack step.

---

### Package Contents
- `GalaxyAndroid.apk`: Unified APK containing native ARM64 `libgame.so`, `libmain.so`, `libreserve.so`, `libinstaller.so`, and `libnodtool.so` signed for Android 11+ (API 30+).
