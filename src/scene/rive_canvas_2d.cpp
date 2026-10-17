#include "rive_canvas_2d.h"
#include "rive_node.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveCanvas2D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_size", "size"), &RiveCanvas2D::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &RiveCanvas2D::get_size);
    ClassDB::bind_method(D_METHOD("set_cull_rect", "rect"), &RiveCanvas2D::set_cull_rect);
    ClassDB::bind_method(D_METHOD("get_cull_rect"), &RiveCanvas2D::get_cull_rect);
    ClassDB::bind_method(D_METHOD("set_cull_enabled", "enabled"), &RiveCanvas2D::set_cull_enabled);
    ClassDB::bind_method(D_METHOD("is_cull_enabled"), &RiveCanvas2D::is_cull_enabled);
    ClassDB::bind_method(D_METHOD("get_texture"), &RiveCanvas2D::get_texture);
    ClassDB::bind_method(D_METHOD("read_image"), &RiveCanvas2D::read_image);
    ClassDB::bind_method(D_METHOD("_advance_node", "index"), &RiveCanvas2D::_advance_node);

    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size", "get_size");
    ADD_GROUP("Culling", "cull_");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cull_enabled"), "set_cull_enabled", "is_cull_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::RECT2, "cull_rect"), "set_cull_rect", "get_cull_rect");
}

RiveCanvas2D::RiveCanvas2D() {
    texture_target.instantiate();
    output_texture.instantiate();
    output_texture->set_target(texture_target);
    RiveRenderRegistry::get_singleton()->add_drawable(this);
}

RiveCanvas2D::~RiveCanvas2D() {
    RiveRenderRegistry::get_singleton()->remove_drawable(this);
    if (texture_target.is_valid()) {
        texture_target->clear();
    }
}

void RiveCanvas2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            set_process(true);
            render_dirty = true;
            queue_redraw();
            break;
        case NOTIFICATION_VISIBILITY_CHANGED:
            if (is_visible_in_tree()) {
                render_dirty = true;
                queue_redraw();
            }
            break;
    }
}

void RiveCanvas2D::set_size(const Vector2i &p_size) {
    if (size != p_size) {
        size = p_size;
        render_dirty = true;
        update_configuration_warnings();
        queue_redraw();
    }
}

Vector2i RiveCanvas2D::get_size() const {
    return size;
}

void RiveCanvas2D::set_cull_rect(const Rect2 &p_rect) {
    if (cull_rect != p_rect) {
        cull_rect = p_rect;
        render_dirty = true;
        queue_redraw();
    }
}

Rect2 RiveCanvas2D::get_cull_rect() const {
    return cull_rect;
}

void RiveCanvas2D::set_cull_enabled(bool p_enabled) {
    if (cull_enabled != p_enabled) {
        cull_enabled = p_enabled;
        render_dirty = true;
        queue_redraw();
    }
}

bool RiveCanvas2D::is_cull_enabled() const {
    return cull_enabled;
}

Ref<Texture2D> RiveCanvas2D::get_texture() const {
    return output_texture;
}

Ref<Image> RiveCanvas2D::read_image() const {
    return output_texture.is_valid() ? output_texture->get_image() : Ref<Image>();
}

void RiveCanvas2D::_collect_descendants(Node *parent, const Transform2D &parent_xform_in_canvas) {
    int n = parent->get_child_count();
    for (int i = 0; i < n; i++) {
        Node *child = parent->get_child(i);
        if (Object::cast_to<RiveCanvas2D>(child)) {
            continue;
        }
        RiveNode *rive_node = Object::cast_to<RiveNode>(child);
        Transform2D child_xform_in_canvas = parent_xform_in_canvas;
        if (rive_node) {
            active_nodes.push_back(ObjectID(rive_node->get_instance_id()));
            Transform2D rive_parent = parent_xform_in_canvas;
            const Transform2D canvas_global = get_global_transform_with_canvas();
            const Transform2D node_local = rive_node->get_transform();
            if (!Math::is_zero_approx(canvas_global.determinant()) && !Math::is_zero_approx(node_local.determinant())) {
                rive_parent = canvas_global.affine_inverse() * rive_node->get_global_transform_with_canvas() * node_local.affine_inverse();
            }
            active_parent_xforms.push_back(rive_parent);
            child_xform_in_canvas = parent_xform_in_canvas * rive_node->get_transform();
        } else if (CanvasItem *item = Object::cast_to<CanvasItem>(child)) {
            child_xform_in_canvas = parent_xform_in_canvas * item->get_transform();
        }
        _collect_descendants(child, child_xform_in_canvas);
    }
}

RiveNode *RiveCanvas2D::_resolve_active_node(uint32_t p_index) const {
    if (p_index >= active_nodes.size()) return nullptr;
    return Object::cast_to<RiveNode>(ObjectDB::get_instance((uint64_t)active_nodes[p_index]));
}

void RiveCanvas2D::_advance_node(uint32_t p_index) {
    if (RiveNode *node = _resolve_active_node(p_index)) {
        if (node->can_advance_rive()) node->advance_rive_automatically(current_delta);
    }
}

void RiveCanvas2D::_process(double delta) {
    if (!is_inside_tree() || !is_visible_in_tree()) return;
    active_nodes.clear();
    active_parent_xforms.clear();
    _collect_descendants(this, Transform2D());

    if (active_nodes.size() > 0) {
        current_delta = delta;
        for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); i++) {
            _advance_node(i);
        }
    }

    if (last_nodes.size() != active_nodes.size()) render_dirty = true;
    for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); ++i) {
        RiveNode *node = _resolve_active_node(i);
        if (!node) { render_dirty = true; continue; }
        const Transform2D xform = active_parent_xforms[i] * node->get_transform();
        const bool visible = node->is_visible_in_tree();
        if (i >= last_nodes.size() || last_nodes[i] != active_nodes[i] ||
                last_node_xforms[i] != xform || last_visibility[i] != visible) {
            render_dirty = true;
        }
        if (visible && (node->consume_rive_update() ||
                (node->can_advance_rive() && node->wants_continuous_redraw()))) {
            render_dirty = true;
        }
    }
    last_nodes.clear();
    last_node_xforms.clear();
    last_visibility.clear();
    for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); ++i) {
        RiveNode *node = _resolve_active_node(i);
        last_nodes.push_back(active_nodes[i]);
        last_node_xforms.push_back(node ? active_parent_xforms[i] * node->get_transform() : Transform2D());
        last_visibility.push_back(node && node->is_visible_in_tree());
    }
    if (render_dirty) queue_redraw();
}

void RiveCanvas2D::_draw() {
    if (!is_inside_tree() || !is_visible_in_tree()) return;
    if (size.x <= 0 || size.y <= 0) return;
    if (!texture_target.is_valid()) return;

    const Size2i previous_size = texture_target->get_size();
    texture_target->resize(size);
    if (!texture_target->is_valid()) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice *rd = rs->get_rendering_device();

    if (render_dirty) {
        active_nodes.clear();
        active_parent_xforms.clear();
        _collect_descendants(this, Transform2D());
        render_dirty = false;
        rive_integration::render_texture(rd, texture_target->get_texture_rid(), this, size.x, size.y);
    }

    if (texture_target->get_texture_rd().is_valid()) {
        draw_texture(texture_target->get_texture_rd(), Point2(0, 0));
    } else if (texture_target->get_texture_rid().is_valid()) {
        rs->canvas_item_add_texture_rect(get_canvas_item(), Rect2(Point2(), size), texture_target->get_texture_rid());
    }
    if (previous_size != size) {
        const Ref<RiveOutputTexture> texture = output_texture;
        if (texture.is_valid()) texture->notify_target_changed();
    }
}

void RiveCanvas2D::draw(rive::Renderer *renderer) {
    Rect2 cull_region = (cull_rect.size.x > 0 && cull_rect.size.y > 0)
        ? cull_rect
        : Rect2(0, 0, size.x, size.y);
    for (uint32_t i = 0; i < (uint32_t)active_nodes.size(); i++) {
        RiveNode *node = _resolve_active_node(i);
        const Transform2D &parent_xform = active_parent_xforms[i];
        if (!node || !node->is_visible_in_tree()) continue;

        Transform2D node_xform_in_canvas = parent_xform * node->get_transform();
        if (!node_xform_in_canvas.is_finite()) continue;
        Rect2 node_bounds = node->get_rive_bounds();
        Rect2 transformed_bounds = node_xform_in_canvas.xform(node_bounds);

        const bool has_unknown_bounds = node_bounds.size.x <= 0 || node_bounds.size.y <= 0;
        if (!cull_enabled || has_unknown_bounds || cull_region.intersects(transformed_bounds)) {
            renderer->save();
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

PackedStringArray RiveCanvas2D::_get_configuration_warnings() const {
    PackedStringArray warnings;
    if (size.x <= 0 || size.y <= 0) warnings.push_back("RiveCanvas2D size must be positive on both axes.");
    return warnings;
}
