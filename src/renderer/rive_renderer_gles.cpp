// Android OpenGL ES 3.0 renderer for Rive.
// On Android, Godot manages the EGL context; we piggyback on it.
// GLAD is NOT used here — GLES3 symbols come directly from the Android NDK.

#ifdef __ANDROID__

#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// Android NDK OpenGL ES 3.0 headers (no GLAD needed).
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/rive_renderer.hpp"

using namespace godot;
using namespace rive;
using namespace rive::gpu;

namespace rive_integration {

static rive::gpu::RenderContext *g_rive_gles_context = nullptr;

bool create_gles_context() {
    if (g_rive_gles_context) return true;

    // Godot's EGL/GLES3 context is already current on this thread.
    // RenderContextGLImpl::MakeContext() reads the capabilities of whatever
    // context is current; on Android with RIVE_ANDROID defined it uses the NDK
    // GLES3 function pointers rather than GLAD.
    g_rive_gles_context = RenderContextGLImpl::MakeContext({}).release();

    if (!g_rive_gles_context) {
        UtilityFunctions::printerr("Rive: Failed to create RenderContextGLImpl for Android GLES3.");
        return false;
    }

    RiveRenderRegistry::get_singleton()->set_factory(g_rive_gles_context);
    UtilityFunctions::print_verbose("Rive: Android GLES3 context initialized.");
    return true;
}

void cleanup_gles_context() {
    delete g_rive_gles_context;
    g_rive_gles_context = nullptr;
}

void render_texture_gles(RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_gles_context || !drawable) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    // texture_get_native_handle() returns the underlying GLES texture object ID.
    uint64_t tex_id = rs->texture_get_native_handle(texture_rid);
    if (tex_id == 0) return;

    // TextureRenderTargetGL creates an FBO and attaches the given texture to it,
    // so Rive renders into the Godot-managed GLES texture rather than FBO 0.
    TextureRenderTargetGL render_target(width, height);
    render_target.setTargetTexture(static_cast<GLuint>(tex_id));

    RenderContext::FrameDescriptor frame_descriptor;
    frame_descriptor.renderTargetWidth = width;
    frame_descriptor.renderTargetHeight = height;
    frame_descriptor.loadAction = LoadAction::clear;
    frame_descriptor.clearColor = 0x00000000;

    RenderContextGLImpl *gl_impl = g_rive_gles_context->static_impl_cast<RenderContextGLImpl>();
    // Tell Rive that GL state may have been dirtied by Godot.
    gl_impl->invalidateGLState();

    g_rive_gles_context->beginFrame(frame_descriptor);

    {
        rive::RiveRenderer renderer(g_rive_gles_context);
        renderer.save();
        // OpenGL ES origin is bottom-left; Godot textures expect top-left.
        renderer.transform(rive::Mat2D(1.0f, 0.0f, 0.0f, -1.0f, 0.0f, static_cast<float>(height)));
        drawable->draw(&renderer);
        renderer.restore();
    }

    g_rive_gles_context->flush({.renderTarget = &render_target});

    // Restore GL state so Godot's renderer is unaffected.
    gl_impl->unbindGLInternalResources();
}

} // namespace rive_integration

#endif // __ANDROID__
