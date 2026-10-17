#include "rive_texture_target.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

RiveTextureTarget::RiveTextureTarget() {
}

RiveTextureTarget::~RiveTextureTarget() {
    clear();
}

void RiveTextureTarget::clear() {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (rs && !rs->is_on_render_thread()) {
        rs->call_on_render_thread(callable_mp(this, &RiveTextureTarget::clear_on_render_thread));
        rs->force_sync();
        return;
    }
    clear_on_render_thread();
}

void RiveTextureTarget::clear_on_render_thread() {
    if (texture_rid.is_valid()) {
        RenderingServer *rs = RenderingServer::get_singleton();
        if (rs) {
            RenderingDevice *rd = rs->get_rendering_device();
            if (texture_rd_ref.is_valid()) {
                texture_rd_ref->set_texture_rd_rid(RID());
                if (rd) {
                    rd->free_rid(texture_rid);
                }
            } else {
                rs->free_rid(texture_rid);
            }
        }
    }
    texture_rid = RID();
    texture_rd_ref.unref();
    texture_size = Size2i();
}

bool RiveTextureTarget::resize(Size2i new_size) {
    RenderingServer *rs = RenderingServer::get_singleton();
    if (rs && !rs->is_on_render_thread()) {
        resize_result = false;
        rs->call_on_render_thread(callable_mp(this, &RiveTextureTarget::resize_on_render_thread).bind(new_size));
        rs->force_sync();
        return resize_result;
    }
    resize_on_render_thread(new_size);
    return resize_result;
}

void RiveTextureTarget::resize_on_render_thread(Size2i new_size) {
    resize_result = false;
    if (new_size.width <= 0 || new_size.height <= 0) {
        clear_on_render_thread();
        return;
    }

    if (texture_size == new_size && texture_rid.is_valid()) {
        resize_result = true;
        return;
    }

    clear_on_render_thread();

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;

    RenderingDevice *rd = rs->get_rendering_device();
    
    String driver = rs->get_current_rendering_driver_name();
    bool is_opengl = (driver == "opengl3");

    if (!rd && !is_opengl) return;

    if (rd) {
        Ref<RDTextureFormat> tf;
        tf.instantiate();
        tf->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
        tf->set_width(new_size.width);
        tf->set_height(new_size.height);
        tf->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
                           RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                           RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT |
                           RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT);

        Ref<RDTextureView> tv;
        tv.instantiate();

        texture_rid = rd->texture_create(tf, tv);
        if (!texture_rid.is_valid()) {
            texture_rd_ref.unref();
            texture_size = Size2i();
            return;
        }
        
        if (texture_rd_ref.is_null()) {
            texture_rd_ref.instantiate();
        }
        texture_rd_ref->set_texture_rd_rid(texture_rid);
    } else {
        Ref<Image> img = Image::create(new_size.width, new_size.height, false, Image::FORMAT_RGBA8);
        texture_rid = rs->texture_2d_create(img);
        texture_rd_ref.unref();
        if (!texture_rid.is_valid()) {
            texture_size = Size2i();
            return;
        }
    }
    
    texture_size = new_size;
    resize_result = true;
}
