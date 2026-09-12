#include "rive_canvas_2d.h"
#include "rive_node.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveCanvas2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveCanvas2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveCanvas2D::get_size);
    ClassDB::bind_method(D_METHOD("set_cull_rect", "rect"), &RiveCanvas2D::set_cull_rect);
    ClassDB::bind_method(D_METHOD("get_cull_rect"), &RiveCanvas2D::get_cull_rect);
    ClassDB::bind_method(D_METHOD("set_cull_enabled", "enabled"), &RiveCanvas2D::set_cull_enabled);
    ClassDB::bind_method(D_METHOD("is_cull_enabled"), &RiveCanvas2D::is_cull_enabled);
    ClassDB::bind_method(D_METHOD("get_texture"), &RiveCanvas2D::get_texture);
    ClassDB::bind_method(D_METHOD("_advance_node", "index"), &RiveCanvas2D::_advance_node);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size", "get_size");
    ADD_GROUP("Culling", "cull_");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cull_enabled"), "set_cull_enabled", "is_cull_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::RECT2, "cull_rect"), "set_cull_rect", "get_cull_rect");
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

void RiveCanvas2D::set_cull_rect(const Rect2 &p_rect) {
    if (cull_rect != p_rect) {
        cull_rect = p_rect;
        queue_redraw();
    }
}

Rect2 RiveCanvas2D::get_cull_rect() const {
    return cull_rect;
}

void RiveCanvas2D::set_cull_enabled(bool p_enabled) {
    if (cull_enabled != p_enabled) {
        cull_enabled = p_enabled;
        queue_redraw();
    }
}

bool RiveCanvas2D::is_cull_enabled() const {
    return cull_enabled;
}

Ref<Texture2D> RiveCanvas2D::get_texture() const {
    if (texture_target.is_valid()) {
        return texture_target->get_texture_rd();
    }
    return Ref<Texture2D>();
}

void RiveCanvas2D::_collect_descendants(Node *parent, const Transform2D &parent_xform_in_canvas) {
    int n = parent->get_child_count();
    for (int i = 0; i < n; i++) {
        Node *child = parent->get_child(i);
        // Don't descend into nested RiveCanvas2D — it manages its own subtree.
        if (Object::cast_to<RiveCanvas2D>(child)) {
            continue;
        }
        RiveNode *rive_node = Object::cast_to<RiveNode>(child);
        Node2D *node2d = Object::cast_to<Node2D>(child);
        Transform2D child_xform_in_canvas = parent_xform_in_canvas;
        if (rive_node) {
            // RiveNode draw() applies its own get_transform(); collect the
            // *parent* chain so we can push it on the renderer first.
            active_nodes.push_back(rive_node);
            active_parent_xforms.push_back(parent_xform_in_canvas);
            // Continue descending; combine this node's local xform for further
            // descendants (e.g. RiveNode under a non-RiveCanvas RiveNode).
            child_xform_in_canvas = parent_xform_in_canvas * rive_node->get_transform();
        } else if (node2d) {
            child_xform_in_canvas = parent_xform_in_canvas * node2d->get_transform();
        }
        _collect_descendants(child, child_xform_in_canvas);
    }
}

void RiveCanvas2D::_advance_node(uint32_t p_index) {
    if (p_index < active_nodes.size()) {
        active_nodes[p_index]->advance(current_delta);
    }
}

void RiveCanvas2D::_process(double delta) {
    active_nodes.clear();
    active_parent_xforms.clear();
    _collect_descendants(this, Transform2D());

    if (active_nodes.size() > 0) {
        current_delta = delta;
        // In the editor, skip WorkerThreadPool to avoid race conditions with
        // file scanning and resource reloading (node ptrs may become invalid)
        if (Engine::get_singleton()->is_editor_hint()) {
            for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); i++) {
                _advance_node(i);
            }
        } else {
            int64_t group_id = WorkerThreadPool::get_singleton()->add_group_task(Callable(this, "_advance_node"), active_nodes.size(), -1, true, "RiveCanvas2D Advance");
            WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_id);
        }
    }

    queue_redraw();
}

void RiveCanvas2D::_draw() {
    if (!is_inside_tree()) return;
    if (size.x <= 0 || size.y <= 0) return;
    if (!texture_target.is_valid()) return;

    texture_target->resize(size);
    if (!texture_target->get_texture_rid().is_valid()) return;

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
    Rect2 cull_region = (cull_rect.size.x > 0 && cull_rect.size.y > 0)
        ? cull_rect
        : Rect2(0, 0, size.x, size.y);
    for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); i++) {
        RiveNode *node = active_nodes[i];
        const Transform2D &parent_xform = active_parent_xforms[i];
        if (!node || !node->is_visible()) continue;

        // Compose canvas-relative transform for this node (parent chain * own).
        Transform2D node_xform_in_canvas = parent_xform * node->get_transform();
        Rect2 node_bounds = node->get_rive_bounds();
        Rect2 transformed_bounds = node_xform_in_canvas.xform(node_bounds);

        // Empty bounds means "no known size" (e.g. RiveRaw with custom drawing)
        // — skip culling and always draw.
        bool always_draw = !cull_enabled || node_bounds.size.x <= 0 || node_bounds.size.y <= 0;
        if (always_draw || cull_region.intersects(transformed_bounds)) {
            renderer->save();
            // Apply parent-chain transform first; node->draw will then apply
            // its own get_transform() on top.
            if (parent_xform != Transform2D()) {
                rive::Mat2D rive_parent(
                    parent_xform.columns[0].x, parent_xform.columns[0].y,
                    parent_xform.columns[1].x, parent_xform.columns[1].y,
                    parent_xform.columns[2].x, parent_xform.columns[2].y);
                renderer->transform(rive_parent);
            }
            node->draw(renderer);
            renderer->restore();
        }
    }
}
