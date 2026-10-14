#pragma once

#include <android/input.h>
#include <stdint.h>
#include <string>
#include "port/input.h"

namespace mobile_ui {

struct Settings {
    bool showControls = true;
    bool fullscreen = true;
    float opacity = 0.65f;
    float scale = 1.0f;
    bool dynamicStick = true;
    float stickOffsetX = 0.0f;
    float stickOffsetY = 0.0f;
    float buttonOffsetX = 0.0f;
    float buttonOffsetY = 0.0f;
    bool showDpad = true;
    bool dpadOnRight = false;
    float dpadOffsetX = 0.0f;
    float dpadOffsetY = 0.0f;

    // Advanced Mods (off by default)
    bool modFly = false;
    bool modSkipCutscenes = false;
    bool modInfiniteHealth = false;

    // FPS configuration
    int targetFps = 60;  // 60, 90, or 120
    bool adaptiveFps = true;

    bool fullTouchMode = true;
    int perfHudMode = 2; // 0 = Off, 1 = FPS, 2 = Full Hardware Stats
    int graphicsBackend = 0; // 0 = OpenGL ES 3.2 (Default Active), 1 = Vulkan 1.1 (Experimental)

    void load(const std::string& path);
    void save(const std::string& path) const;
};

class UIOverlay {
public:
    static UIOverlay& get();

    bool init();
    void setSavePath(const std::string& path);

    // Per-frame tick to handle pulse buttons, auto-release, and hardware metrics
    void update(PortPadState& pad, int& spinFrames);

    // Processes an Android motion or key event.
    // Returns true if consumed (e.g., settings menu or virtual controls).
    bool handleInput(const AInputEvent* event, PortPadState& pad, int& spinFrames,
                     int screenW, int screenH, int vpX, int vpY, int vpW, int vpH);

    // Render the overlay (touch controls + settings menu + hardware perf HUD)
    void render(int screenW, int screenH);

    bool needsRedraw();
    bool isSettingsOpen() const { return mSettingsOpen || mDrawerAnim > 0.01f; }
    void closeSettings();
    void triggerSaveAnywhere();
    void showToast(const std::string& msg);
    Settings& settings() { return mSettings; }
    void setPlusEdition(bool plus) { mPlusEdition = plus; }
    bool isPlusEdition() const { return mPlusEdition; }

private:
    UIOverlay();

    Settings mSettings;
    std::string mSavePath;
    bool mPlusEdition = false;
    bool mInitialized = false;
    bool mDirty = false;

    // UI state
    bool mSettingsOpen = false;
    float mDrawerAnim = 0.0f; // 0.0 = closed offscreen, 1.0 = fully open
    int64_t mLastAnimTime = 0;
    bool mEditLayoutMode = false;
    int mDraggingCluster = 0; // 0 = none, 1 = stick cluster, 2 = button cluster
    float mDragStartX = 0.0f, mDragStartY = 0.0f;
    float mInitialStickOfsX = 0.0f, mInitialStickOfsY = 0.0f;
    float mInitialBtnOfsX = 0.0f, mInitialBtnOfsY = 0.0f;
    float mInitialDpadOfsX = 0.0f, mInitialDpadOfsY = 0.0f;

    // Settings Navigation & Scrolling
    int mSettingsTab = 0;              // 0 = General, 1 = Advanced
    float mDrawerScrollY = 0.0f;       // Current scroll offset (pixels)
    float mDrawerScrollTarget = 0.0f;
    bool mDrawerIsScrolling = false;
    int mDrawerScrollPointerId = -1;
    float mDrawerTouchStartY = 0.0f;
    float mDrawerTouchStartX = 0.0f;
    float mDrawerScrollStartOffset = 0.0f;

    // Virtual Stick State
    int mStickPointerId = -1;
    float mStickBaseX = 0.0f;
    float mStickBaseY = 0.0f;
    float mStickKnobX = 0.0f;
    float mStickKnobY = 0.0f;

    // Button press states (for visual press effect)
    bool mBtnAPressed = false;
    bool mBtnBPressed = false;
    bool mBtnZPressed = false;
    bool mBtnSpinPressed = false;
    bool mBtnPlusPressed = false;
    bool mBtnCfgPressed = false;
    bool mBtnDpadLeft = false;
    bool mBtnDpadRight = false;
    bool mBtnDpadUp = false;
    bool mBtnDpadDown = false;

    // Persistent in-game Star Pointer state (normalized -1..1, default to screen center 0, 0)
    float mPointerX = 0.0f;
    float mPointerY = 0.0f;

    // Finger active press tracking (prevents stuck buttons)
    bool mFingerDownOnA = false;
    bool mFingerDownOnB = false;
    bool mFingerDownOnZ = false;
    bool mFingerDownOnSpin = false;
    bool mFingerDownOnPlus = false;

    // Multi-touch Pointer Roles
    enum class TouchRole {
        NONE,
        STICK,
        DPAD,
        BTN_A,
        BTN_B,
        BTN_Z,
        BTN_SPIN,
        BTN_PLUS,
        STAR_POINTER
    };

    struct TouchPoint {
        int32_t id = -1;
        TouchRole role = TouchRole::NONE;
        float startX = 0.0f;
        float startY = 0.0f;
        int64_t startTime = 0;
    };
    TouchPoint mTouches[10];

    // Full Touch Gesture & Tap State
    int mTapPendingFrames = 0;
    int mTapDelayFrames = 0;
    int mTouchPointerId = -1;
    int64_t mTouchStartTime = 0;
    float mTouchStartX = 0.0f, mTouchStartY = 0.0f;
    int64_t mLastTapTime = 0;

    // Hardware & Performance Monitoring Metrics
    float mCurrentFps = 60.0f;
    float mAvgFrameTimeMs = 16.6f;
    int mFpsFrameCounter = 0;
    int64_t mFpsLastTimeNs = 0;
    float mCpuUsagePercent = 0.0f;
    float mRamUsageMB = 0.0f;
    int64_t mLastHwSampleTime = 0;
    std::string mGpuRendererName = "Mali-G68 GPU";

    // In-Game Log Viewer State
    bool mLogViewerOpen = false;
    float mLogScrollY = 0.0f;

    // Toast Notification State
    std::string mToastMessage = "Game Progress Saved Anywhere!";
    float mSaveToastTimer = 0.0f;

    // GL resources
    unsigned int mProg = 0;
    unsigned int mVao = 0;
    unsigned int mVbo = 0;
    unsigned int mFontTex = 0;
    int mAtlasW = 0;
    int mAtlasH = 0;
};

inline UIOverlay& get() {
    return UIOverlay::get();
}

}  // namespace mobile_ui
