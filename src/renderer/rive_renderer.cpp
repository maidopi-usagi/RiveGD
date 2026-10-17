#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <mutex>

using namespace godot;

namespace rive_integration {

namespace {

struct RenderRequest {
    RenderingDevice *rd = nullptr;
    RID texture_rid;
    RiveDrawable *drawable = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

std::recursive_mutex render_request_mutex;
thread_local uint32_t serialized_render_callback_depth = 0;

struct SerializedRenderCallbackScope {
    SerializedRenderCallbackScope() { ++serialized_render_callback_depth; }
    ~SerializedRenderCallbackScope() { --serialized_render_callback_depth; }
};

}

static void run_rive_task_on_render_thread(uint64_t p_task_address) {
    SerializedRenderCallbackScope scope;
    const std::function<void()> *task = reinterpret_cast<const std::function<void()> *>(p_task_address);
    if (task) {
        (*task)();
    }
}

bool run_on_rive_render_thread(const std::function<void()> &p_task) {
    if (!p_task) {
        return false;
    }

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) {
        return false;
    }

    if (rs->is_on_render_thread()) {
        // The synchronous caller already holds the lock on another thread.
        if (serialized_render_callback_depth != 0) {
            p_task();
            return true;
        }
        std::lock_guard<std::recursive_mutex> lock(render_request_mutex);
        p_task();
        return true;
    }

    std::lock_guard<std::recursive_mutex> lock(render_request_mutex);
    bool executed = false;
    const std::function<void()> wrapped_task = [&p_task, &executed]() {
        p_task();
        executed = true;
    };
    const uint64_t task_address = reinterpret_cast<uint64_t>(&wrapped_task);
    rs->call_on_render_thread(callable_mp_static(&run_rive_task_on_render_thread).bind(task_address));
    rs->force_sync();
    return executed;
}

#if defined(VULKAN_ENABLED)
bool create_vulkan_context(RenderingDevice* rd);
void render_texture_vulkan(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
void cleanup_vulkan_context();
#endif

#if defined(D3D12_ENABLED)
bool create_d3d12_context(RenderingDevice* rd);
void render_texture_d3d12(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
void cleanup_d3d12_context();
#endif

#if defined(__APPLE__)
bool create_metal_context(RenderingDevice* rd);
void render_texture_metal(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
void cleanup_metal_context();
#endif

#if defined(RIVE_DESKTOP_GL)
bool create_opengl_context();
void render_texture_opengl(RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
void cleanup_opengl_context();
#endif

static void initialize_rive_renderer_on_render_thread() {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    RenderingDevice *rd = rs->get_rendering_device();

    String api = rs->get_current_rendering_driver_name();
    bool success = false;

    if (api == "vulkan") {
#if defined(VULKAN_ENABLED)
        if (rd) success = create_vulkan_context(rd);
#else
        UtilityFunctions::printerr("Rive: Vulkan support not compiled in.");
#endif
    } else if (api == "d3d12") {
#if defined(D3D12_ENABLED)
        if (rd) success = create_d3d12_context(rd);
#else
        UtilityFunctions::printerr("Rive: D3D12 support not compiled in.");
#endif
    } else if (api == "metal") {
#if defined(__APPLE__)
        if (rd) success = create_metal_context(rd);
#else
        UtilityFunctions::printerr("Rive: Metal support not compiled in.");
#endif
    } else if (api == "opengl3") {
#if defined(RIVE_DESKTOP_GL)
        success = create_opengl_context();
#elif defined(__ANDROID__)
        success = create_gles_context();
#else
        UtilityFunctions::printerr("Rive: OpenGL support not compiled in.");
#endif
    } else {
        UtilityFunctions::printerr("Rive: Unsupported graphics API: " + api);
    }

    if (success) {
        UtilityFunctions::print_verbose("Rive renderer initialized.");
    } else {
        UtilityFunctions::printerr("Rive renderer initialization failed.");
    }
}

void initialize_rive_renderer() {
    run_on_rive_render_thread(&initialize_rive_renderer_on_render_thread);
}

static void cleanup_rive_renderer_on_render_thread() {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    String api = rs->get_current_rendering_driver_name();

    if (api == "d3d12") {
#if defined(D3D12_ENABLED)
        cleanup_d3d12_context();
#endif
    } else if (api == "vulkan") {
#if defined(VULKAN_ENABLED)
        cleanup_vulkan_context();
#endif
    } else if (api == "metal") {
#if defined(__APPLE__)
        cleanup_metal_context();
#endif
    } else if (api == "opengl3") {
#if defined(__ANDROID__)
        cleanup_gles_context();
#elif defined(RIVE_DESKTOP_GL)
        cleanup_opengl_context();
#endif
    }
}

void cleanup_rive_renderer() {
    run_on_rive_render_thread(&cleanup_rive_renderer_on_render_thread);
}

static void render_texture_on_render_thread(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs || !drawable || !texture_rid.is_valid() || width == 0 || height == 0) return;
    
    String api = rs->get_current_rendering_driver_name();
    
    if (api == "vulkan") {
#if defined(VULKAN_ENABLED)
        if (rd) render_texture_vulkan(rd, texture_rid, drawable, width, height);
#endif
    } else if (api == "d3d12") {
#if defined(D3D12_ENABLED)
        if (rd) render_texture_d3d12(rd, texture_rid, drawable, width, height);
#endif
    } else if (api == "metal") {
#if defined(__APPLE__)
        if (rd) render_texture_metal(rd, texture_rid, drawable, width, height);
#endif
    } else if (api == "opengl3") {
#if defined(RIVE_DESKTOP_GL)
        render_texture_opengl(texture_rid, drawable, width, height);
#elif defined(__ANDROID__)
        render_texture_gles(texture_rid, drawable, width, height);
#endif
    }
}

static void render_pending_texture_on_render_thread(uint64_t p_request_address) {
    SerializedRenderCallbackScope scope;
    const RenderRequest *request_ptr = reinterpret_cast<const RenderRequest *>(p_request_address);
    if (!request_ptr) {
        return;
    }
    const RenderRequest &request = *request_ptr;
    render_texture_on_render_thread(request.rd, request.texture_rid, request.drawable, request.width, request.height);
    if (request.drawable) {
        request.drawable->clear_recorded_commands();
    }
}

void render_texture(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs || !drawable || !texture_rid.is_valid() || width == 0 || height == 0) {
        return;
    }

    if (rs->is_on_render_thread()) {
        std::lock_guard<std::recursive_mutex> lock(render_request_mutex);
        drawable->record_draw_commands();
        render_texture_on_render_thread(rd, texture_rid, drawable, width, height);
        drawable->clear_recorded_commands();
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(render_request_mutex);
    drawable->record_draw_commands();
    const RenderRequest request = { rd, texture_rid, drawable, width, height };
    rs->call_on_render_thread(callable_mp_static(&render_pending_texture_on_render_thread).bind(reinterpret_cast<uint64_t>(&request)));
    rs->force_sync();
}

}
