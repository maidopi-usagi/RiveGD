#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_render_image.hpp"

namespace rive_integration {

class RiveTextureFactory {
public:
    static rive::rcp<rive::RenderImage> make_image(godot::Ref<godot::Texture2D> texture);
};

}
