#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/templates/local_vector.hpp>

#if defined(VULKAN_ENABLED)

#include <vulkan/vulkan.h>
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"

using namespace godot;
using namespace rive;
using namespace rive::gpu;

namespace rive_integration {

static rive::gpu::RenderContext *g_rive_context = nullptr;

struct VulkanFuncs {
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
} g_vk;

bool create_vulkan_context(RenderingDevice* rd) {
    if (!rd) return false;

    VkInstance instance = (VkInstance)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TOPMOST_OBJECT, RID(), 0);
    VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, RID(), 0);
    VkDevice device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    
    if (!instance || !physical_device || !device) {
        UtilityFunctions::printerr("Rive: Failed to retrieve Vulkan handles from RenderingDevice.");
        return false;
    }

    PFN_vkGetInstanceProcAddr get_instance_proc_addr = vkGetInstanceProcAddr;
    if (!get_instance_proc_addr) {
        UtilityFunctions::printerr("Rive: vkGetInstanceProcAddr not available.");
        return false;
    }

    PFN_vkGetDeviceProcAddr get_device_proc_addr = (PFN_vkGetDeviceProcAddr)get_instance_proc_addr(instance, "vkGetDeviceProcAddr");
    if (!get_device_proc_addr) {
        return false;
    }

    g_vk.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    g_vk.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)get_device_proc_addr(device, "vkGetDeviceQueue");
    g_vk.vkCreateCommandPool = (PFN_vkCreateCommandPool)get_device_proc_addr(device, "vkCreateCommandPool");
    g_vk.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)get_device_proc_addr(device, "vkAllocateCommandBuffers");
    g_vk.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)get_device_proc_addr(device, "vkBeginCommandBuffer");
    g_vk.vkEndCommandBuffer = (PFN_vkEndCommandBuffer)get_device_proc_addr(device, "vkEndCommandBuffer");
    g_vk.vkQueueSubmit = (PFN_vkQueueSubmit)get_device_proc_addr(device, "vkQueueSubmit");
    g_vk.vkQueueWaitIdle = (PFN_vkQueueWaitIdle)get_device_proc_addr(device, "vkQueueWaitIdle");
    g_vk.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)get_device_proc_addr(device, "vkDestroyCommandPool");

    rive::gpu::RenderContextVulkanImpl::ContextOptions options;
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    rive::gpu::VulkanFeatures vulkan_features;
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

    g_rive_context = ctx.release();
    RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);

    UtilityFunctions::print_verbose("RIVE: create_vulkan_context succeeded");

    return true;
}

void render_texture_vulkan(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_context || !rd || !drawable) {
        return;
    }

    VkDevice device = (VkDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    VkPhysicalDevice physical_device = (VkPhysicalDevice)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE, RID(), 0);

    if (!device || !physical_device) {
        return;
    }
    
    uint32_t queue_family_count = 0;
    if (!g_vk.vkGetPhysicalDeviceQueueFamilyProperties) {
        return;
    }
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    LocalVector<VkQueueFamilyProperties> queue_families;
    queue_families.resize(queue_family_count);
    g_vk.vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.ptr());

    uint32_t graphics_queue_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family_index = i;
            break;
        }
    }

    if (graphics_queue_family_index == UINT32_MAX) {
        return;
    }

    VkQueue queue = VK_NULL_HANDLE;
    if (g_vk.vkGetDeviceQueue) {
        g_vk.vkGetDeviceQueue(device, graphics_queue_family_index, 0, &queue);
    }

    if (!queue) {
        return;
    }

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = graphics_queue_family_index;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool command_pool;
    if (!g_vk.vkCreateCommandPool || g_vk.vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        return;
    }

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    if (!g_vk.vkAllocateCommandBuffers || g_vk.vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        if (g_vk.vkDestroyCommandPool) {
            g_vk.vkDestroyCommandPool(device, command_pool, nullptr);
        }
        return;
    }

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (!g_vk.vkBeginCommandBuffer || g_vk.vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        if (g_vk.vkDestroyCommandPool) {
            g_vk.vkDestroyCommandPool(device, command_pool, nullptr);
        }
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
                drawable->draw(&renderer);
            }

            static uint64_t frame_idx = 0;
            frame_idx++;

            rive::gpu::RenderContext::FlushResources fr;
            fr.renderTarget = rtarget.get();
            fr.externalCommandBuffer = command_buffer;
            fr.currentFrameNumber = frame_idx;
            fr.safeFrameNumber = (frame_idx > 2) ? frame_idx - 2 : 0;

            g_rive_context->flush(fr);
        }
    }

    if (g_vk.vkEndCommandBuffer) {
        g_vk.vkEndCommandBuffer(command_buffer);
    }

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    if (g_vk.vkQueueSubmit) {
        g_vk.vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    }
    if (g_vk.vkQueueWaitIdle) {
        g_vk.vkQueueWaitIdle(queue);
    }

    if (g_vk.vkDestroyCommandPool) {
        g_vk.vkDestroyCommandPool(device, command_pool, nullptr);
    }
}

} // namespace rive_integration
#endif
