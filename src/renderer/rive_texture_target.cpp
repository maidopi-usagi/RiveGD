#include "rive_texture_target.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>

RiveTextureTarget::RiveTextureTarget() {
}

RiveTextureTarget::~RiveTextureTarget() {
    clear();
}

void RiveTextureTarget::clear() {
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
    if (new_size.width <= 0 || new_size.height <= 0) {
        clear();
        return false;
    }

    if (texture_size == new_size && texture_rid.is_valid()) {
        return false;
    }

    clear();

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return false;

    RenderingDevice *rd = rs->get_rendering_device();
    
    // Check if we are using OpenGL (no RD)
    String driver = rs->get_current_rendering_driver_name();
    bool is_opengl = (driver == "opengl3");

    if (!rd && !is_opengl) return false;

    if (rd) {
        Ref<RDTextureFormat> tf;
        tf.instantiate();
        tf->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
        tf->set_width(new_size.width);
        tf->set_height(new_size.height);
        tf->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT);

        Ref<RDTextureView> tv;
        tv.instantiate();

        texture_rid = rd->texture_create(tf, tv);
        
        if (texture_rd_ref.is_null()) {
            texture_rd_ref.instantiate();
        }
        texture_rd_ref->set_texture_rd_rid(texture_rid);
    } else {
        Ref<Image> img = Image::create(new_size.width, new_size.height, false, Image::FORMAT_RGBA8);
        texture_rid = rs->texture_2d_create(img);
        texture_rd_ref.unref();
    }
    
    texture_size = new_size;
    return true;
}
