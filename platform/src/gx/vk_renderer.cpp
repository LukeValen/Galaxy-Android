#include "vk_renderer.h"

#include <android/log.h>
#include <algorithm>
#include <array>
#include <cstring>

#define TAG "PetariVK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

namespace gpu {

static VkRenderer sVkRenderer;

VkRenderer& VkRenderer::get() {
    return sVkRenderer;
}

VkRenderer& vkRenderer() {
    return sVkRenderer;
}

bool VkRenderer::init(ANativeWindow* window) {
    std::lock_guard<std::recursive_mutex> lock(mRenderMutex);
    if (mInitialized) return true;
    mWindow = window;
    mWidth = ANativeWindow_getWidth(window);
    mHeight = ANativeWindow_getHeight(window);

    LOGI("Initializing Vulkan 1.1 native renderer (%dx%d)...", mWidth, mHeight);

    if (!createInstance()) {
        LOGE("Failed to create Vulkan 1.1 instance");
        return false;
    }

    if (!createSurface(window)) {
        LOGE("Failed to create Android Vulkan surface");
        return false;
    }

    if (!selectPhysicalDevice()) {
        LOGE("Failed to select Vulkan physical device");
        return false;
    }

    if (!createLogicalDevice()) {
        LOGE("Failed to create Vulkan logical device");
        return false;
    }

    if (!createSwapchain()) {
        LOGE("Failed to create Vulkan swapchain");
        return false;
    }

    if (!createRenderPasses()) {
        LOGE("Failed to create Vulkan render passes");
        return false;
    }

    if (!createCommandPool()) {
        LOGE("Failed to create Vulkan command pool");
        return false;
    }

    if (!createSyncObjects()) {
        LOGE("Failed to create Vulkan sync objects");
        return false;
    }

    if (!createDescriptorLayouts()) {
        LOGE("Failed to create Vulkan descriptor layouts");
        return false;
    }

    mInitialized = true;
    LOGI("Vulkan 1.1 native renderer initialized successfully on %s", mDeviceProperties.deviceName);
    return true;
}

void VkRenderer::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(mRenderMutex);
    if (!mInitialized) return;

    if (mDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(mDevice);

        if (mFlightFence != VK_NULL_HANDLE) vkDestroyFence(mDevice, mFlightFence, nullptr);
        if (mImageAvailableSem != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, mImageAvailableSem, nullptr);
        if (mRenderFinishedSem != VK_NULL_HANDLE) vkDestroySemaphore(mDevice, mRenderFinishedSem, nullptr);

        if (mDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
        if (mStorageDescLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(mDevice, mStorageDescLayout, nullptr);

        if (mCommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

        for (auto fb : mSwapchainFramebuffers) vkDestroyFramebuffer(mDevice, fb, nullptr);
        mSwapchainFramebuffers.clear();

        for (auto iv : mSwapchainImageViews) vkDestroyImageView(mDevice, iv, nullptr);
        mSwapchainImageViews.clear();

        if (mEfbRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(mDevice, mEfbRenderPass, nullptr);
        if (mPresentRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(mDevice, mPresentRenderPass, nullptr);

        if (mSwapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        vkDestroyDevice(mDevice, nullptr);
    }

    if (mInstance != VK_NULL_HANDLE) {
        if (mSurface != VK_NULL_HANDLE) vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        vkDestroyInstance(mInstance, nullptr);
    }

    mInstance = VK_NULL_HANDLE;
    mDevice = VK_NULL_HANDLE;
    mSurface = VK_NULL_HANDLE;
    mSwapchain = VK_NULL_HANDLE;
    mInitialized = false;
    LOGI("Vulkan 1.1 renderer shut down cleanly.");
}

bool VkRenderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GalaxyAndroid";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "PetariVK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &mInstance);
    return res == VK_SUCCESS;
}

bool VkRenderer::createSurface(ANativeWindow* window) {
    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = window;
    return vkCreateAndroidSurfaceKHR(mInstance, &sci, nullptr, &mSurface) == VK_SUCCESS;
}

bool VkRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mSurface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                mPhysicalDevice = dev;
                mQueueFamily = i;
                vkGetPhysicalDeviceProperties(dev, &mDeviceProperties);
                vkGetPhysicalDeviceFeatures(dev, &mDeviceFeatures);
                LOGI("Selected Vulkan GPU: %s (Driver version: 0x%x, API: %d.%d)",
                     mDeviceProperties.deviceName, mDeviceProperties.driverVersion,
                     VK_VERSION_MAJOR(mDeviceProperties.apiVersion),
                     VK_VERSION_MINOR(mDeviceProperties.apiVersion));
                return true;
            }
        }
    }
    return false;
}

bool VkRenderer::createLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = mQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = mDeviceFeatures.samplerAnisotropy;
    createInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(mDevice, mQueueFamily, 0, &mQueue);
    return true;
}

bool VkRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = fmt;
            break;
        }
    }
    mSwapchainFormat = surfaceFormat.format;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = mSurface;
    sci.minImageCount = imageCount;
    sci.imageFormat = surfaceFormat.format;
    sci.imageColorSpace = surfaceFormat.colorSpace;
    sci.imageExtent = caps.currentExtent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mDevice, &sci, nullptr, &mSwapchain) != VK_SUCCESS) {
        return false;
    }

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actualImageCount, nullptr);
    mSwapchainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &actualImageCount, mSwapchainImages.data());

    mSwapchainImageViews.resize(actualImageCount);
    for (size_t i = 0; i < actualImageCount; i++) {
        VkImageViewCreateInfo ivci{};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = mSwapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = mSwapchainFormat;
        ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(mDevice, &ivci, nullptr, &mSwapchainImageViews[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool VkRenderer::createRenderPasses() {
    // Present render pass (drawing to swapchain)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = mSwapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mPresentRenderPass) != VK_SUCCESS) {
        return false;
    }

    // EFB offscreen render pass (Color + Depth/Stencil attachments)
    VkAttachmentDescription efbAttachments[2]{};
    efbAttachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    efbAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    efbAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    efbAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    efbAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    efbAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    efbAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    efbAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    efbAttachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
    efbAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    efbAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    efbAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    efbAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    efbAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    efbAttachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    efbAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription efbSubpass{};
    efbSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    efbSubpass.colorAttachmentCount = 1;
    efbSubpass.pColorAttachments = &colorRef;
    efbSubpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo efbRpInfo{};
    efbRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    efbRpInfo.attachmentCount = 2;
    efbRpInfo.pAttachments = efbAttachments;
    efbRpInfo.subpassCount = 1;
    efbRpInfo.pSubpasses = &efbSubpass;

    if (vkCreateRenderPass(mDevice, &efbRpInfo, nullptr, &mEfbRenderPass) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan EFB render pass");
        return false;
    }

    mSwapchainFramebuffers.resize(mSwapchainImageViews.size());
    for (size_t i = 0; i < mSwapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = mPresentRenderPass;
        fci.attachmentCount = 1;
        fci.pAttachments = &mSwapchainImageViews[i];
        fci.width = mWidth;
        fci.height = mHeight;
        fci.layers = 1;
        if (vkCreateFramebuffer(mDevice, &fci, nullptr, &mSwapchainFramebuffers[i]) != VK_SUCCESS) {
            return false;
        }
    }

    return true;
}

bool VkRenderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    return vkAllocateCommandBuffers(mDevice, &allocInfo, &mCommandBuffer) == VK_SUCCESS;
}

bool VkRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateSemaphore(mDevice, &semInfo, nullptr, &mImageAvailableSem) == VK_SUCCESS &&
           vkCreateSemaphore(mDevice, &semInfo, nullptr, &mRenderFinishedSem) == VK_SUCCESS &&
           vkCreateFence(mDevice, &fenceInfo, nullptr, &mFlightFence) == VK_SUCCESS;
}

bool VkRenderer::createDescriptorLayouts() {
    // Descriptor layout for native SSBO storage buffers in Vertex & Fragment stages
    // Binding 0: rows (matrix rows)
    // Binding 1: recs (draw records)
    // Binding 2: states (lights/proj/pixel)
    // Binding 3: vtxRec (vertex ordinal map)
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    for (uint32_t i = 0; i < 4; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = (uint32_t)bindings.size();
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mStorageDescLayout) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 64;

    return vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) == VK_SUCCESS;
}

uint32_t VkRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

VkTarget VkRenderer::createTarget(int w, int h) {
    VkTarget t{};
    t.width = w;
    t.height = h;

    // 1. Color attachment image (RGBA8)
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = (uint32_t)w;
    ici.extent.height = (uint32_t)h;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(mDevice, &ici, nullptr, &t.colorImage) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mDevice, t.colorImage, &memReqs);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReqs.size;
    mai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(mDevice, &mai, nullptr, &t.colorMemory) != VK_SUCCESS ||
        vkBindImageMemory(mDevice, t.colorImage, t.colorMemory, 0) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = t.colorImage;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_R8G8B8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(mDevice, &ivci, nullptr, &t.colorView) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    // 2. Depth/stencil attachment image (D24S8)
    VkImageCreateInfo dici = ici;
    dici.format = VK_FORMAT_D24_UNORM_S8_UINT;
    dici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (vkCreateImage(mDevice, &dici, nullptr, &t.depthImage) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    vkGetImageMemoryRequirements(mDevice, t.depthImage, &memReqs);
    mai.allocationSize = memReqs.size;
    mai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(mDevice, &mai, nullptr, &t.depthMemory) != VK_SUCCESS ||
        vkBindImageMemory(mDevice, t.depthImage, t.depthMemory, 0) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    VkImageViewCreateInfo divci{};
    divci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    divci.image = t.depthImage;
    divci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    divci.format = VK_FORMAT_D24_UNORM_S8_UINT;
    divci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    divci.subresourceRange.levelCount = 1;
    divci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(mDevice, &divci, nullptr, &t.depthView) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    // 3. Target Framebuffer bound to mEfbRenderPass
    VkImageView attachments[2] = {t.colorView, t.depthView};
    VkFramebufferCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fci.renderPass = mEfbRenderPass;
    fci.attachmentCount = 2;
    fci.pAttachments = attachments;
    fci.width = (uint32_t)w;
    fci.height = (uint32_t)h;
    fci.layers = 1;
    if (vkCreateFramebuffer(mDevice, &fci, nullptr, &t.framebuffer) != VK_SUCCESS) {
        destroyTarget(t);
        return {};
    }

    return t;
}

void VkRenderer::destroyTarget(VkTarget& target) {
    if (mDevice == VK_NULL_HANDLE) return;
    if (target.framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(mDevice, target.framebuffer, nullptr);
    if (target.depthView != VK_NULL_HANDLE) vkDestroyImageView(mDevice, target.depthView, nullptr);
    if (target.depthImage != VK_NULL_HANDLE) vkDestroyImage(mDevice, target.depthImage, nullptr);
    if (target.depthMemory != VK_NULL_HANDLE) vkFreeMemory(mDevice, target.depthMemory, nullptr);
    if (target.colorView != VK_NULL_HANDLE) vkDestroyImageView(mDevice, target.colorView, nullptr);
    if (target.colorImage != VK_NULL_HANDLE) vkDestroyImage(mDevice, target.colorImage, nullptr);
    if (target.colorMemory != VK_NULL_HANDLE) vkFreeMemory(mDevice, target.colorMemory, nullptr);
    target = {};
}

void VkRenderer::onWindowResized(ANativeWindow* window) {
    std::lock_guard<std::recursive_mutex> lock(mRenderMutex);
    if (!mInitialized) return;
    mWindow = window;
    mWidth = ANativeWindow_getWidth(window);
    mHeight = ANativeWindow_getHeight(window);

    vkDeviceWaitIdle(mDevice);

    for (auto fb : mSwapchainFramebuffers) vkDestroyFramebuffer(mDevice, fb, nullptr);
    mSwapchainFramebuffers.clear();

    for (auto iv : mSwapchainImageViews) vkDestroyImageView(mDevice, iv, nullptr);
    mSwapchainImageViews.clear();

    if (mSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    createSwapchain();
    createRenderPasses();
    LOGI("Resized Vulkan swapchain to %dx%d", mWidth, mHeight);
}

void VkRenderer::present(const VkTarget& target, int vpX, int vpY, int vpW, int vpH) {
    std::lock_guard<std::recursive_mutex> lock(mRenderMutex);
    if (!mInitialized || mSwapchain == VK_NULL_HANDLE) return;

    vkWaitForFences(mDevice, 1, &mFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult acquireRes = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mImageAvailableSem, VK_NULL_HANDLE, &imageIndex);
    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
        onWindowResized(mWindow);
        return;
    }
    if (acquireRes != VK_SUCCESS) {
        return;
    }

    vkResetFences(mDevice, 1, &mFlightFence);
    vkResetCommandBuffer(mCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(mCommandBuffer, &beginInfo);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = mPresentRenderPass;
    rpInfo.framebuffer = mSwapchainFramebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {(uint32_t)mWidth, (uint32_t)mHeight};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(mCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(mCommandBuffer);

    vkEndCommandBuffer(mCommandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {mImageAvailableSem};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffer;
    VkSemaphore signalSemaphores[] = {mRenderFinishedSem};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(mQueue, 1, &submitInfo, mFlightFence);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = {mSwapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(mQueue, &presentInfo);
}

}  // namespace gpu
