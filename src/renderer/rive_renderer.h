#ifndef RIVE_RENDERER_H
#define RIVE_RENDERER_H

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include "rive_render_registry.h"

using namespace godot;

namespace rive_integration {
    void initialize_rive_renderer();
    void cleanup_rive_renderer();
    void render_texture(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);

#ifdef __ANDROID__
    bool create_gles_context();
    void cleanup_gles_context();
    void render_texture_gles(RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height);
#endif
}

#endif // RIVE_RENDERER_H
