#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <memory>
#include <vector>

#if defined(VULKAN_ENABLED)

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace godot;
using namespace rive;
using namespace rive::gpu;

namespace rive_integration {

static rive::gpu::RenderContext *g_rive_context = nullptr;
static void* g_vulkan_lib = nullptr;

PFN_vkGetInstanceProcAddr load_vulkan_loader() {
    if (g_vulkan_lib) return (PFN_vkGetInstanceProcAddr)
#ifdef _WIN32
        GetProcAddress((HMODULE)g_vulkan_lib, "vkGetInstanceProcAddr");
#else
        dlsym(g_vulkan_lib, "vkGetInstanceProcAddr");
#endif

#ifdef _WIN32
    g_vulkan_lib = LoadLibraryA("vulkan-1.dll");
#elif __APPLE__
    g_vulkan_lib = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!g_vulkan_lib) g_vulkan_lib = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
#elif defined(__ANDROID__)
    // On Android the Vulkan loader is always libvulkan.so (no .1 suffix).
    g_vulkan_lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#else
    g_vulkan_lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!g_vulkan_lib) g_vulkan_lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif

    if (!g_vulkan_lib) return nullptr;

    return (PFN_vkGetInstanceProcAddr)
#ifdef _WIN32
        GetProcAddress((HMODULE)g_vulkan_lib, "vkGetInstanceProcAddr");
#else
        dlsym(g_vulkan_lib, "vkGetInstanceProcAddr");
#endif
}

struct VulkanFuncs {
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
} g_vk;

struct FrameResources {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    uint64_t frameNumber = 0;
};

struct RiveVulkanState {
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::vector<FrameResources> frames;
    uint32_t currentFrame = 0;
    uint64_t nextFrameNumber = 0;
};

static RiveVulkanState* g_vk_state = nullptr;

void cleanup_vulkan_context();

static bool has_required_vulkan_functions() {
    return g_vk.vkGetPhysicalDeviceFeatures &&
           g_vk.vkGetPhysicalDeviceQueueFamilyProperties &&
           g_vk.vkGetDeviceQueue && g_vk.vkCreateCommandPool &&
           g_vk.vkAllocateCommandBuffers && g_vk.vkResetCommandBuffer &&
           g_vk.vkBeginCommandBuffer &&
           g_vk.vkEndCommandBuffer && g_vk.vkQueueSubmit &&
           g_vk.vkDestroyCommandPool && g_vk.vkCreateFence &&
           g_vk.vkWaitForFences && g_vk.vkResetFences &&
           g_vk.vkDestroyFence;
}

static void destroy_vulkan_state(RiveVulkanState* state) {
    if (!state) return;
    if (state->device) {
        if (state->queue != VK_NULL_HANDLE && g_vk.vkQueueWaitIdle) {
            g_vk.vkQueueWaitIdle(state->queue);
        }
        for (auto& frame : state->frames) {
            if (frame.fence != VK_NULL_HANDLE && g_vk.vkDestroyFence) {
                g_vk.vkDestroyFence(state->device, frame.fence, nullptr);
            }
        }
        if (state->commandPool != VK_NULL_HANDLE && g_vk.vkDestroyCommandPool) {
            g_vk.vkDestroyCommandPool(state->device, state->commandPool, nullptr);
        }
    }
    delete state;
}

bool create_vulkan_context(RenderingDevice* rd) {
    if (!rd) return false;
    if (g_rive_context || g_vk_state) {
        cleanup_vulkan_context();
    }

    VkInstance instance = (VkInstance)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TOPMOST_OBJECT, RID(), 0);
    VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, RID(), 0);
    VkDevice device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    
    if (!instance || !physical_device || !device) {
        UtilityFunctions::printerr("Rive: Failed to retrieve Vulkan handles from RenderingDevice.");
        return false;
    }

    PFN_vkGetInstanceProcAddr get_instance_proc_addr = load_vulkan_loader();
    if (!get_instance_proc_addr) {
        UtilityFunctions::printerr("Rive: Failed to load Vulkan loader.");
        return false;
    }

    PFN_vkGetDeviceProcAddr get_device_proc_addr = (PFN_vkGetDeviceProcAddr)get_instance_proc_addr(instance, "vkGetDeviceProcAddr");
    if (!get_device_proc_addr) {
        return false;
    }

    g_vk.vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures");
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    g_vk.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)get_device_proc_addr(device, "vkGetDeviceQueue");
    g_vk.vkCreateCommandPool = (PFN_vkCreateCommandPool)get_device_proc_addr(device, "vkCreateCommandPool");
    g_vk.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)get_device_proc_addr(device, "vkAllocateCommandBuffers");
    g_vk.vkResetCommandBuffer = (PFN_vkResetCommandBuffer)get_device_proc_addr(device, "vkResetCommandBuffer");
    g_vk.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)get_device_proc_addr(device, "vkBeginCommandBuffer");
    g_vk.vkEndCommandBuffer = (PFN_vkEndCommandBuffer)get_device_proc_addr(device, "vkEndCommandBuffer");
    g_vk.vkQueueSubmit = (PFN_vkQueueSubmit)get_device_proc_addr(device, "vkQueueSubmit");
    g_vk.vkQueueWaitIdle = (PFN_vkQueueWaitIdle)get_device_proc_addr(device, "vkQueueWaitIdle");
    g_vk.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)get_device_proc_addr(device, "vkDestroyCommandPool");
    g_vk.vkCreateFence = (PFN_vkCreateFence)get_device_proc_addr(device, "vkCreateFence");
    g_vk.vkWaitForFences = (PFN_vkWaitForFences)get_device_proc_addr(device, "vkWaitForFences");
    g_vk.vkResetFences = (PFN_vkResetFences)get_device_proc_addr(device, "vkResetFences");
    g_vk.vkDestroyFence = (PFN_vkDestroyFence)get_device_proc_addr(device, "vkDestroyFence");

    if (!has_required_vulkan_functions()) {
        UtilityFunctions::printerr("Rive: Vulkan device is missing required entry points.");
        return false;
    }

    rive::gpu::RenderContextVulkanImpl::ContextOptions options;
    
    VkPhysicalDeviceFeatures features{};
    g_vk.vkGetPhysicalDeviceFeatures(physical_device, &features);

    rive::gpu::VulkanFeatures vulkan_features{};
    vulkan_features.independentBlend = features.independentBlend;
    vulkan_features.fillModeNonSolid = features.fillModeNonSolid;
    vulkan_features.fragmentStoresAndAtomics = features.fragmentStoresAndAtomics;
    vulkan_features.shaderClipDistance = features.shaderClipDistance;

    std::unique_ptr<rive::gpu::RenderContext> ctx = rive::gpu::RenderContextVulkanImpl::MakeContext(
            instance,
            physical_device,
            device,
            vulkan_features,
            get_instance_proc_addr,
            options);

    if (!ctx) {
        return false;
    }

    std::unique_ptr<RiveVulkanState> state(new RiveVulkanState());
    state->device = device;

    uint32_t queue_family_count = 0;
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    if (queue_family_count == 0) {
        UtilityFunctions::printerr("Rive: Vulkan device has no queue families.");
        return false;
    }
    LocalVector<VkQueueFamilyProperties> queue_families;
    queue_families.resize(queue_family_count);
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.ptr());

    const VkQueue godot_queue = (VkQueue)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
    if (godot_queue == VK_NULL_HANDLE) {
        UtilityFunctions::printerr("Rive: Failed to retrieve Godot's Vulkan command queue.");
        return false;
    }

    const uint64_t family_resource = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_QUEUE_FAMILY, RID(), 0);
    if (family_resource >= queue_family_count) {
        UtilityFunctions::printerr("Rive: Godot reported an invalid Vulkan queue family.");
        return false;
    }
    const uint32_t graphics_queue_family_index = static_cast<uint32_t>(family_resource);
    if (!(queue_families[graphics_queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
        UtilityFunctions::printerr("Rive: Godot's Vulkan command queue family is not graphics capable.");
        return false;
    }
    state->queue = godot_queue;

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = graphics_queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (g_vk.vkCreateCommandPool(device, &pool_info, nullptr, &state->commandPool) != VK_SUCCESS) {
        UtilityFunctions::printerr("Rive: Failed to create Vulkan command pool.");
        return false;
    }

    state->frames.resize(2);
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = state->commandPool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = (uint32_t)state->frames.size();
    std::vector<VkCommandBuffer> buffers(state->frames.size());
    if (g_vk.vkAllocateCommandBuffers(device, &alloc_info, buffers.data()) != VK_SUCCESS) {
        UtilityFunctions::printerr("Rive: Failed to allocate Vulkan command buffers.");
        destroy_vulkan_state(state.release());
        return false;
    }

    for (size_t i = 0; i < state->frames.size(); ++i) {
        state->frames[i].commandBuffer = buffers[i];
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (g_vk.vkCreateFence(device, &fence_info, nullptr, &state->frames[i].fence) != VK_SUCCESS) {
            UtilityFunctions::printerr("Rive: Failed to create Vulkan frame fence.");
            destroy_vulkan_state(state.release());
            return false;
        }
    }

    // Publish only after every dependent resource was created successfully.
    g_rive_context = ctx.release();
    g_vk_state = state.release();
    RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);

    UtilityFunctions::print_verbose("RIVE: create_vulkan_context succeeded");

    return true;
}

void render_texture_vulkan(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_context || !rd || !drawable || !g_vk_state || !g_vk_state->queue ||
        g_vk_state->frames.empty()) {
        return;
    }

    VkDevice device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    if (!device) return;

    // Cycle frames
    FrameResources& frame = g_vk_state->frames[g_vk_state->currentFrame];
    g_vk_state->currentFrame = (g_vk_state->currentFrame + 1) % g_vk_state->frames.size();
    VkCommandBuffer command_buffer = frame.commandBuffer;
    if (command_buffer == VK_NULL_HANDLE || frame.fence == VK_NULL_HANDLE) return;

    // Wait for fence
    constexpr uint64_t kFenceTimeoutNs = 30ULL * 1000ULL * 1000ULL * 1000ULL;
    if (g_vk.vkWaitForFences(device, 1, &frame.fence, VK_TRUE, kFenceTimeoutNs) != VK_SUCCESS) {
        UtilityFunctions::printerr("Rive: Timed out or failed waiting for a Vulkan frame fence.");
        return;
    }
    if (g_vk.vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS) {
        UtilityFunctions::printerr("Rive: Failed to reset Vulkan command buffer.");
        return;
    }
    
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (!g_vk.vkBeginCommandBuffer || g_vk.vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        return;
    }

    VkImage image = (VkImage)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, texture_rid, 0);
    VkImageView image_view = (VkImageView)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE_VIEW, texture_rid, 0);
    VkFormat format = (VkFormat)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE_DATA_FORMAT, texture_rid, 0);

    if (image && image_view) {
        rive::gpu::RenderContext::FrameDescriptor fd;
        fd.renderTargetWidth = width;
        fd.renderTargetHeight = height;
        fd.loadAction = rive::gpu::LoadAction::clear;
        fd.clearColor = 0x00000000;

        g_rive_context->beginFrame(fd);

        rive::gpu::RenderContextVulkanImpl *impl = g_rive_context->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        rive::rcp<rive::gpu::RenderTargetVulkan> rtarget = impl->makeRenderTarget(width, height, format, usage);

        if (rtarget) {
            rive::gpu::vkutil::ImageAccess access;
            access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access.accessMask = VK_ACCESS_SHADER_READ_BIT;
            access.pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            static_cast<rive::gpu::RenderTargetVulkanImpl *>(rtarget.get())->setTargetImageView(image_view, image, access);

            {
                rive::RiveRenderer renderer(g_rive_context);
                drawable->replay_draw_commands(&renderer);
            }

            const uint64_t frame_idx = ++g_vk_state->nextFrameNumber;

            rive::gpu::RenderContext::FlushResources fr;
            fr.renderTarget = rtarget.get();
            fr.externalCommandBuffer = command_buffer;
            fr.currentFrameNumber = frame_idx;
            fr.safeFrameNumber = frame.frameNumber;

            g_rive_context->flush(fr);

            // Rive may leave the external target in a color-attachment or
            // general layout. Restore the state Godot expects before the
            // canvas samples this texture.
            rive::gpu::vkutil::ImageAccess sampled_access;
            sampled_access.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            sampled_access.accessMask = VK_ACCESS_SHADER_READ_BIT;
            sampled_access.pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            static_cast<rive::gpu::RenderTargetVulkanImpl *>(rtarget.get())
                    ->accessTargetImage(command_buffer, sampled_access);
        }
    }

    if (g_vk.vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        UtilityFunctions::printerr("Rive: Failed to finish Vulkan command buffer.");
        return;
    }

    // Do not reset the reusable fence until the command buffer is known-good.
    if (g_vk.vkResetFences(device, 1, &frame.fence) != VK_SUCCESS) return;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    if (g_vk.vkQueueSubmit(g_vk_state->queue, 1, &submit_info, frame.fence) != VK_SUCCESS) {
        // A failed submit can never signal the fence. Recreate it signaled so
        // the next frame cannot block forever.
        g_vk.vkDestroyFence(device, frame.fence, nullptr);
        frame.fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        g_vk.vkCreateFence(device, &fence_info, nullptr, &frame.fence);
        UtilityFunctions::printerr("Rive: Failed to submit Vulkan command buffer.");
        return;
    }
    frame.frameNumber = g_vk_state->nextFrameNumber;
}

void cleanup_vulkan_context() {
    if (g_vk_state && g_vk_state->queue && g_vk.vkQueueWaitIdle) {
        g_vk.vkQueueWaitIdle(g_vk_state->queue);
    }

    RiveRenderRegistry::get_singleton()->clear_renderer_states();
    if (g_rive_context) {
        delete g_rive_context;
        g_rive_context = nullptr;
    }
    RiveRenderRegistry::get_singleton()->set_factory(nullptr);

    if (g_vk_state) {
        destroy_vulkan_state(g_vk_state);
        g_vk_state = nullptr;
    }

    if (g_vulkan_lib) {
#ifdef _WIN32
        FreeLibrary((HMODULE)g_vulkan_lib);
#else
        dlclose(g_vulkan_lib);
#endif
        g_vulkan_lib = nullptr;
    }
}

} // namespace rive_integration
#endif
