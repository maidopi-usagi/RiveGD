#include "rive_canvas_2d.h"
#include "rive_node.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveCanvas2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveCanvas2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveCanvas2D::get_size);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size", "get_size");
}

RiveCanvas2D::RiveCanvas2D() {
    texture_rd.instantiate();
}

RiveCanvas2D::~RiveCanvas2D() {
    if (texture_rid.is_valid()) {
        RenderingServer *rs = RenderingServer::get_singleton();
        if (rs) {
            RenderingDevice *rd = rs->get_rendering_device();
            if (rd) {
                if (texture_rd.is_valid()) {
                    texture_rd->set_texture_rd_rid(RID());
                }
                rd->free_rid(texture_rid);
            }
        }
    }
}

void RiveCanvas2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            set_process(true);
            break;
    }
}

void RiveCanvas2D::set_size(const Vector2i &p_size) {
    if (size != p_size) {
        size = p_size;
        queue_redraw();
    }
}

Vector2i RiveCanvas2D::get_size() const {
    return size;
}

void RiveCanvas2D::_process(double delta) {
    for (int i = 0; i < get_child_count(); i++) {
        RiveNode *node = Object::cast_to<RiveNode>(get_child(i));
        if (node) {
            node->advance(delta);
        }
    }
    
    queue_redraw();
}

void RiveCanvas2D::_draw() {
    if (size.x <= 0 || size.y <= 0) return;
    
    RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
    if (!rd) return;

    if (texture_rid.is_valid()) {
        Ref<RDTextureFormat> format = rd->texture_get_format(texture_rid);
        if (!format.is_valid()) {
            // Texture is invalid, so clear it
            texture_rid = RID();
            if (texture_rd.is_valid()) {
                texture_rd->set_texture_rd_rid(RID());
            }
        } else if (format->get_width() != size.x || format->get_height() != size.y) {
            if (texture_rd.is_valid()) {
                texture_rd->set_texture_rd_rid(RID());
            }
            rd->free_rid(texture_rid);
            texture_rid = RID();
        }
    }

    if (!texture_rid.is_valid()) {
        Ref<RDTextureFormat> format;
        format.instantiate();
        format->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
        format->set_width(size.x);
        format->set_height(size.y);
        format->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT);
        format->set_texture_type(RenderingDevice::TEXTURE_TYPE_2D);
        
        Ref<RDTextureView> view;
        view.instantiate();
        texture_rid = rd->texture_create(format, view, TypedArray<PackedByteArray>());
        texture_rd->set_texture_rd_rid(texture_rid);
    }

    rive_integration::render_texture(rd, texture_rid, this, size.x, size.y);
    
    draw_texture(texture_rd, Point2(0, 0));
}

void RiveCanvas2D::draw(rive::Renderer *renderer) {
    for (int i = 0; i < get_child_count(); i++) {
        RiveNode *node = Object::cast_to<RiveNode>(get_child(i));
        if (node && node->is_visible()) {
            renderer->save();
            node->draw(renderer);
            renderer->restore();
        }
    }
}
