#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/renderer/render_context.hpp"
#include "rive_render_registry.h"
#include "rive/renderer/texture.hpp"
#include "rive/renderer/rive_render_image.hpp"

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#import <Metal/Metal.h>

using namespace godot;

rive::rcp<rive::RenderImage> RiveTextureFactoryMetal_make_image(Ref<Texture2D> texture) {
    if (texture.is_null()) return nullptr;

    RenderingDevice* rd = RenderingServer::get_singleton()->get_rendering_device();
    if (!rd) return nullptr;

    RID texture_rid = RenderingServer::get_singleton()->texture_get_rd_texture(texture->get_rid());
    if (!texture_rid.is_valid()) return nullptr;

    id<MTLTexture> mtl_texture = (id<MTLTexture>)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, texture_rid, 0);
    if (!mtl_texture) return nullptr;

    auto factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) return nullptr;

    auto ctx = static_cast<rive::gpu::RenderContext*>(factory);
    if (!ctx) return nullptr;

    auto metal_ctx = ctx->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
    if (!metal_ctx) return nullptr;

    int width = texture->get_width();
    int height = texture->get_height();
    
    auto rive_texture = metal_ctx->makeImageTexture(width, height, mtl_texture);
    return rive::make_rcp<rive::RiveRenderImage>(rive_texture);
}
