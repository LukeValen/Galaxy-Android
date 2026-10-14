#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/native_window.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>

#include "gx_record.h"
#include "gpu.h"
#include "gl_renderer.h"

namespace gpu {

struct VkTarget {
    int width = 0;
    int height = 0;
    VkImage colorImage = VK_NULL_HANDLE;
    VkDeviceMemory colorMemory = VK_NULL_HANDLE;
    VkImageView colorView = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

class VkRenderer {
public:
    static VkRenderer& get();

    VkRenderer() = default;
    ~VkRenderer() { shutdown(); }

    bool init(ANativeWindow* window);
    void shutdown();

    bool isReady() const { return mInitialized; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    VkTarget createTarget(int w, int h);
    void destroyTarget(VkTarget& target);

    bool update() { return false; }
    bool render(VkTarget& target, const EyeView* eye, VkTarget* hud, HudMode hudMode) { return true; }
    void present(const VkTarget& target, int vpX, int vpY, int vpW, int vpH);

    void onWindowResized(ANativeWindow* window);

private:

    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(ANativeWindow* window);
    bool createSwapchain();
    bool createRenderPasses();
    bool createCommandPool();
    bool createSyncObjects();
    bool createDescriptorLayouts();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findSupportedDepthFormat();

    bool mInitialized = false;
    ANativeWindow* mWindow = nullptr;
    int mWidth = 0;
    int mHeight = 0;

    VkInstance mInstance = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties mDeviceProperties{};
    VkPhysicalDeviceFeatures mDeviceFeatures{};
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;
    uint32_t mQueueFamily = 0;

    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    VkFormat mSwapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat mDepthFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;

    VkRenderPass mEfbRenderPass = VK_NULL_HANDLE;
    VkRenderPass mPresentRenderPass = VK_NULL_HANDLE;
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;

    VkSemaphore mImageAvailableSem = VK_NULL_HANDLE;
    VkSemaphore mRenderFinishedSem = VK_NULL_HANDLE;
    VkFence mFlightFence = VK_NULL_HANDLE;

    // Descriptor layouts for SSBO storage buffers and UBOs
    VkDescriptorSetLayout mStorageDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;

    std::recursive_mutex mRenderMutex;
};

VkRenderer& vkRenderer();

}  // namespace gpu
