#include "rive_canvas_2d.h"
#include "rive_node.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveCanvas2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveCanvas2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveCanvas2D::get_size);
    ClassDB::bind_method(D_METHOD("get_texture"), &RiveCanvas2D::get_texture);
    ClassDB::bind_method(D_METHOD("_advance_node", "index"), &RiveCanvas2D::_advance_node);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size", "get_size");
}

RiveCanvas2D::RiveCanvas2D() {
    texture_target.instantiate();
}

RiveCanvas2D::~RiveCanvas2D() {
    if (texture_target.is_valid()) {
        texture_target->clear();
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

Ref<Texture2D> RiveCanvas2D::get_texture() const {
    if (texture_target.is_valid()) {
        return texture_target->get_texture_rd();
    }
    return Ref<Texture2D>();
}

void RiveCanvas2D::_advance_node(uint32_t p_index) {
    if (p_index < active_nodes.size()) {
        active_nodes[p_index]->advance(current_delta);
    }
}

void RiveCanvas2D::_process(double delta) {
    active_nodes.clear();
    for (int i = 0; i < get_child_count(); i++) {
        RiveNode *node = Object::cast_to<RiveNode>(get_child(i));
        if (node) {
            active_nodes.push_back(node);
        }
    }

    if (active_nodes.size() > 0) {
        current_delta = delta;
        int64_t group_id = WorkerThreadPool::get_singleton()->add_group_task(Callable(this, "_advance_node"), active_nodes.size(), -1, true, "RiveCanvas2D Advance");
        WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_id);
    }
    
    queue_redraw();
}

void RiveCanvas2D::_draw() {
    if (size.x <= 0 || size.y <= 0) return;
    if (!texture_target.is_valid()) return;

    texture_target->resize(size);

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice *rd = rs->get_rendering_device();

    rive_integration::render_texture(rd, texture_target->get_texture_rid(), this, size.x, size.y);

    if (texture_target->get_texture_rd().is_valid()) {
        draw_texture(texture_target->get_texture_rd(), Point2(0, 0));
    } else if (texture_target->get_texture_rid().is_valid()) {
        rs->canvas_item_add_texture_rect(get_canvas_item(), Rect2(Point2(), size), texture_target->get_texture_rid());
    }
}

void RiveCanvas2D::draw(rive::Renderer *renderer) {
    Rect2 canvas_rect(0, 0, size.x, size.y);
    for (int i = 0; i < get_child_count(); i++) {
        RiveNode *node = Object::cast_to<RiveNode>(get_child(i));
        if (node && node->is_visible()) {
            Rect2 node_bounds = node->get_rive_bounds();
            Transform2D node_xform = node->get_transform();
            Rect2 transformed_bounds = node_xform.xform(node_bounds);
            
            if (canvas_rect.intersects(transformed_bounds)) {
                renderer->save();
                node->draw(renderer);
                renderer->restore();
            }
        }
    }
}
