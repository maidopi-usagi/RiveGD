#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rive_integration {

#if defined(VULKAN_ENABLED)
bool create_vulkan_context(RenderingDevice* rd);
void render_texture_vulkan(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
#endif

#if defined(D3D12_ENABLED)
bool create_d3d12_context(RenderingDevice* rd);
void render_texture_d3d12(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
#endif

#if defined(__APPLE__)
bool create_metal_context(RenderingDevice* rd);
void render_texture_metal(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
#endif

void initialize_rive_renderer() {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    RenderingDevice *rd = rs->get_rendering_device();
    if (!rd) {
        UtilityFunctions::printerr("Rive: RenderingDevice not available.");
        return;
    }

    String api = rs->get_current_rendering_driver_name();
    bool success = false;

    if (api == "vulkan") {
#if defined(VULKAN_ENABLED)
        success = create_vulkan_context(rd);
#else
        UtilityFunctions::printerr("Rive: Vulkan support not compiled in.");
#endif
    } else if (api == "d3d12") {
#if defined(D3D12_ENABLED)
        success = create_d3d12_context(rd);
#else
        UtilityFunctions::printerr("Rive: D3D12 support not compiled in.");
#endif
    } else if (api == "metal") {
#if defined(__APPLE__)
        success = create_metal_context(rd);
#else
        UtilityFunctions::printerr("Rive: Metal support not compiled in.");
#endif
    } else {
        UtilityFunctions::printerr("Rive: Unsupported graphics API: " + api);
    }

    if (success) {
        UtilityFunctions::print("Rive renderer initialized successfully.");
    } else {
        UtilityFunctions::printerr("Rive renderer initialization failed.");
    }
}

void render_texture(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!rd) return;
    
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    
    String api = rs->get_current_rendering_driver_name();
    
    if (api == "vulkan") {
#if defined(VULKAN_ENABLED)
        render_texture_vulkan(rd, texture_rid, drawable, width, height);
#endif
    } else if (api == "d3d12") {
#if defined(D3D12_ENABLED)
        render_texture_d3d12(rd, texture_rid, drawable, width, height);
#endif
    } else if (api == "metal") {
#if defined(__APPLE__)
        render_texture_metal(rd, texture_rid, drawable, width, height);
#endif
    }
}

} // namespace rive_integration
