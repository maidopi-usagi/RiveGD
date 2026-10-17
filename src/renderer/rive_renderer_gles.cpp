
#ifdef __ANDROID__

#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "rive/renderer/texture.hpp"
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
    RiveRenderRegistry::get_singleton()->set_factory(nullptr);
    RiveRenderRegistry::get_singleton()->clear_renderer_states();
    delete g_rive_gles_context;
    g_rive_gles_context = nullptr;
}

void render_texture_gles(RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_gles_context || !drawable) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    uint64_t tex_id = rs->texture_get_native_handle(texture_rid);
    if (tex_id == 0) return;

    TextureRenderTargetGL render_target(width, height);
    render_target.setTargetTexture(static_cast<GLuint>(tex_id));

    RenderContext::FrameDescriptor frame_descriptor;
    frame_descriptor.renderTargetWidth = width;
    frame_descriptor.renderTargetHeight = height;
    frame_descriptor.loadAction = LoadAction::clear;
    frame_descriptor.clearColor = 0x00000000;

    RenderContextGLImpl *gl_impl = g_rive_gles_context->static_impl_cast<RenderContextGLImpl>();
    gl_impl->invalidateGLState();

    g_rive_gles_context->beginFrame(frame_descriptor);

    {
        rive::RiveRenderer renderer(g_rive_gles_context);
        renderer.save();
        renderer.transform(rive::Mat2D(1.0f, 0.0f, 0.0f, -1.0f, 0.0f, static_cast<float>(height)));
        drawable->replay_draw_commands(&renderer);
        renderer.restore();
    }

    g_rive_gles_context->flush({.renderTarget = &render_target});

    gl_impl->unbindGLInternalResources();
}

}

#endif
