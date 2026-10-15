#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/native_window.h>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <array>
#include <cstring>

#include "gx_record.h"
#include "gpu.h"
#include "gl_renderer.h"
#include "gx_builder.h"
#include "vk_shadergen.h"

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

struct PipelineKey {
    uint32_t uidIndex;
    uint32_t flags;
    uint32_t depthState;   // test(1) | (write << 1) | (depthFunc << 4)
    uint32_t blendState;   // blend(1) | (srcRGB << 1) | (dstRGB << 6) | (eq << 11) | (colorMask << 16) | (dstAlpha << 20)
    uint32_t rasterState;  // cull(1) | (cullFace << 1) | (prim << 4)

    bool operator==(const PipelineKey& o) const {
        return uidIndex == o.uidIndex && flags == o.flags && depthState == o.depthState &&
               blendState == o.blendState && rasterState == o.rasterState;
    }
};

struct PipelineKeyHash {
    size_t operator()(const PipelineKey& k) const {
        size_t h = 1469598103934665603ull;
        h = (h ^ k.uidIndex) * 1099511628211ull;
        h = (h ^ k.flags) * 1099511628211ull;
        h = (h ^ k.depthState) * 1099511628211ull;
        h = (h ^ k.blendState) * 1099511628211ull;
        h = (h ^ k.rasterState) * 1099511628211ull;
        return h;
    }
};

struct SamplerDescKey {
    std::array<VkImageView, 8> views;
    std::array<VkSampler, 8> samplers;

    bool operator==(const SamplerDescKey& o) const {
        return views == o.views && samplers == o.samplers;
    }
};

struct SamplerDescKeyHash {
    size_t operator()(const SamplerDescKey& k) const {
        size_t h = 1469598103934665603ull;
        for (int i = 0; i < 8; i++) {
            h = (h ^ (uintptr_t)k.views[i]) * 1099511628211ull;
            h = (h ^ (uintptr_t)k.samplers[i]) * 1099511628211ull;
        }
        return h;
    }
};

struct FrameResource {
    VkBuffer vbo = VK_NULL_HANDLE;
    VkDeviceMemory vboMem = VK_NULL_HANDLE;
    size_t vboCap = 0;

    VkBuffer ibo = VK_NULL_HANDLE;
    VkDeviceMemory iboMem = VK_NULL_HANDLE;
    size_t iboCap = 0;

    VkBuffer rowBuf = VK_NULL_HANDLE;
    VkDeviceMemory rowMem = VK_NULL_HANDLE;
    size_t rowCap = 0;

    VkBuffer recBuf = VK_NULL_HANDLE;
    VkDeviceMemory recMem = VK_NULL_HANDLE;
    size_t recCap = 0;

    VkBuffer stBuf = VK_NULL_HANDLE;
    VkDeviceMemory stMem = VK_NULL_HANDLE;
    size_t stCap = 0;

    VkBuffer vtxRecBuf = VK_NULL_HANDLE;
    VkDeviceMemory vtxRecMem = VK_NULL_HANDLE;
    size_t vtxRecCap = 0;

    VkBuffer eyeUbo = VK_NULL_HANDLE;
    VkDeviceMemory eyeMem = VK_NULL_HANDLE;
    size_t eyeCap = 0;

    VkDescriptorSet set0 = VK_NULL_HANDLE;
};

struct VkTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t levels = 1;
    uint32_t version = 0;
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

    bool update();
    bool render(VkTarget& target, const EyeView* eye, VkTarget* hud, HudMode hudMode = HudMode::Inline);
    uint64_t frameNumber() const { return mFrameNumber; }
    bool hasFrame() const { return mCurrentFrame != nullptr; }
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
    bool createPresentationPipeline();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkFormat findSupportedDepthFormat();

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void uploadDynamicBuffer(VkBuffer& buf, VkDeviceMemory& mem, size_t& currentCap,
                             VkBufferUsageFlags usage, const void* data, size_t bytes);

    VkSampler getSampler(uint32_t mode0, uint32_t mode1, uint32_t levels);
    void uploadTextures(const std::vector<std::shared_ptr<TexImage>>& textures);
    VkPipeline getPipeline(const Item& it, const ShaderUid& uid);
    VkDescriptorSet getSamplerDescriptorSet(const std::array<VkImageView, 8>& views,
                                            const std::array<VkSampler, 8>& samplers);

    VkShaderModule createShaderModule(const uint32_t* code, size_t sizeBytes);

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
    VkImageUsageFlags mSwapchainUsage = 0;
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

    // Descriptors & Pipelines
    VkDescriptorSetLayout mStorageDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mTextureDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;

    // Presentation pipeline
    VkDescriptorSetLayout mPresentDescLayout = VK_NULL_HANDLE;
    VkPipelineLayout mPresentPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mPresentPipeline = VK_NULL_HANDLE;
    VkSampler mPresentSampler = VK_NULL_HANDLE;
    std::unordered_map<VkImageView, VkDescriptorSet> mPresentDescSets;

    // Rotating frame resources
    FrameResource mFrameRes[2];
    int mFrameIndex = 0;

    // Caches
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> mPipelines;
    std::unordered_map<uint64_t, VkSampler> mSamplers;
    std::unordered_map<const TexImage*, VkTexture> mTextures;
    std::unordered_map<SamplerDescKey, VkDescriptorSet, SamplerDescKeyHash> mSamplerDescSets;

    VkTexture mDummyTexture;
    VkSampler mDummySampler = VK_NULL_HANDLE;

    // Frame command batching
    Builder mBuilder;
    std::unique_ptr<Prepared> mPrepared;
    std::shared_ptr<const Frame> mCurrentFrame;
    uint64_t mFrameNumber = 0;

    std::recursive_mutex mRenderMutex;
};

VkRenderer& vkRenderer();

}  // namespace gpu
