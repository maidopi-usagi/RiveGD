#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "glad_custom.h"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
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

#ifdef _WIN32
static void* get_gl_proc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if(p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)) {
        static HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void*)GetProcAddress(module, name);
    }
    return p;
}
#else
static void* get_gl_proc(const char* name) {
    static void* libgl = nullptr;
    if (!libgl) {
        // FIXME: Try common names.
        libgl = dlopen("libGL.so", RTLD_LAZY);
        if (!libgl) libgl = dlopen("libGL.so.1", RTLD_LAZY);
        if (!libgl) libgl = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    }
    if (libgl) {
        return dlsym(libgl, name);
    }
    return nullptr;
}
#endif

bool create_opengl_context() {
    if (g_rive_context) return true;

    if (!gladLoadCustomLoader((GLADloadfunc)get_gl_proc)) {
        UtilityFunctions::printerr("Rive: Failed to load OpenGL functions.");
        return false;
    }

    g_rive_context = RenderContextGLImpl::MakeContext({}).release();

    if (!g_rive_context) {
        UtilityFunctions::printerr("Rive: Failed to create RenderContextGL.");
        return false;
    }

    RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);
    UtilityFunctions::print("Rive: OpenGL context initialized.");
    return true;
}

void render_texture_opengl(RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_context || !drawable) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    uint64_t tex_id = rs->texture_get_native_handle(texture_rid);
    if (tex_id == 0) return;

    TextureRenderTargetGL render_target(width, height);
    render_target.setTargetTexture((GLuint)tex_id);
    
    RenderContext::FrameDescriptor frame_descriptor;
    frame_descriptor.renderTargetWidth = width;
    frame_descriptor.renderTargetHeight = height;
    frame_descriptor.loadAction = LoadAction::clear;
    frame_descriptor.clearColor = 0x00000000;
    
    RenderContextGLImpl* gl_impl = g_rive_context->static_impl_cast<RenderContextGLImpl>();
    gl_impl->invalidateGLState();

    g_rive_context->beginFrame(frame_descriptor);
    
    {
        rive::RiveRenderer renderer(g_rive_context);
        renderer.save();
        renderer.transform(rive::Mat2D(1.0f, 0.0f, 0.0f, -1.0f, 0.0f, (float)height));
        drawable->draw(&renderer);
        renderer.restore();
    }
    
    g_rive_context->flush({
        .renderTarget = &render_target
    });

    gl_impl->unbindGLInternalResources();
}

} // namespace rive_integration
