#include "rive_texture_factory.h"
#include "rive_render_registry.h"
#include "rive_renderer.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace rive_integration {

rive::rcp<rive::RenderImage> RiveTextureFactory::make_image(Ref<Texture2D> texture) {
    if (texture.is_null()) return nullptr;

    // Read back through Godot on the caller thread. Only the backend factory
    // decode below needs the Rive render-thread serialization boundary.
    Ref<Image> img = texture->get_image();
    if (img.is_null()) return nullptr;

    PackedByteArray png_data = img->save_png_to_buffer();
    if (png_data.is_empty()) return nullptr;

    rive::rcp<rive::RenderImage> render_image;
    const bool executed = run_on_rive_render_thread([&]() {
        RiveRenderRegistry *registry = RiveRenderRegistry::get_singleton();
        rive::Factory *factory = registry ? registry->get_factory() : nullptr;
        if (!factory) return;
        rive::Span<const uint8_t> bytes(png_data.ptr(), png_data.size());
        render_image = factory->decodeImage(bytes);
    });

    return executed ? std::move(render_image) : nullptr;
}

}
