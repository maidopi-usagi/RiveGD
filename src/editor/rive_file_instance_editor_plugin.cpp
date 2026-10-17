#include "rive_file_instance_editor_plugin.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>

struct ResizeHandle {
    float fraction_x, fraction_y;
    bool resizes_x, resizes_y;
};
static const ResizeHandle RESIZE_HANDLES[8] = {
    {0.0f, 0.0f, true,  true },
    {0.5f, 0.0f, false, true },
    {1.0f, 0.0f, true,  true },
    {1.0f, 0.5f, true,  false},
    {1.0f, 1.0f, true,  true },
    {0.5f, 1.0f, false, true },
    {0.0f, 1.0f, true,  true },
    {0.0f, 0.5f, true,  false},
};

void RiveFileInstanceEditor::_bind_methods() {}
void RiveFileInstanceEditor::_notification(int p_what) {}

RiveFileInstanceEditor::RiveFileInstanceEditor() {
    toolbar = memnew(HBoxContainer);
    toolbar_id = toolbar->get_instance_id();
}

RiveFileInstanceEditor::~RiveFileInstanceEditor() {
    auto *remaining_toolbar = Object::cast_to<HBoxContainer>(ObjectDB::get_instance(toolbar_id));
    if (remaining_toolbar && !remaining_toolbar->get_parent()) memdelete(remaining_toolbar);
}

void RiveFileInstanceEditor::edit(RiveFileInstance *p_node) {
    node = p_node;
}

void RiveFileInstanceEditorPlugin::_bind_methods() {}

void RiveFileInstanceEditorPlugin::_enter_tree() {
    rive_editor = memnew(RiveFileInstanceEditor);
    EditorInterface::get_singleton()->get_base_control()->add_child(rive_editor);
    add_control_to_container(CONTAINER_CANVAS_EDITOR_MENU, rive_editor->get_toolbar());
    rive_editor->get_toolbar()->hide();
}

void RiveFileInstanceEditorPlugin::_exit_tree() {
    if (rive_editor) {
        remove_control_from_container(CONTAINER_CANVAS_EDITOR_MENU, rive_editor->get_toolbar());
        if (rive_editor->get_parent()) {
            rive_editor->get_parent()->remove_child(rive_editor);
        }
        memdelete(rive_editor);
        rive_editor = nullptr;
    }
}

bool RiveFileInstanceEditorPlugin::_handles(Object *p_object) const {
    return Object::cast_to<RiveFileInstance>(p_object) != nullptr;
}

void RiveFileInstanceEditorPlugin::_make_visible(bool p_visible) {
    if (p_visible) {
        rive_editor->get_toolbar()->show();
    } else {
        rive_editor->get_toolbar()->hide();
        rive_editor->edit(nullptr);
        dragging_handle = -1;
        hovering_handle = -1;
    }
}

void RiveFileInstanceEditorPlugin::_edit(Object *p_object) {
    rive_editor->edit(Object::cast_to<RiveFileInstance>(p_object));
    dragging_handle = -1;
    hovering_handle = -1;
    update_overlays();
}

Transform2D RiveFileInstanceEditorPlugin::get_viewport_to_parent_xform(RiveFileInstance *node) const {
    Transform2D parent_global = node->get_global_transform() * node->get_transform().affine_inverse();
    return (node->get_viewport_transform() * parent_global).affine_inverse();
}

Vector2 RiveFileInstanceEditorPlugin::get_handle_viewport_pos(int idx, RiveFileInstance *node) const {
    Rect2 rect = node->get_rive_bounds();
    Vector2 local_pos = rect.position + Vector2(rect.size.x * RESIZE_HANDLES[idx].fraction_x,
                                                rect.size.y * RESIZE_HANDLES[idx].fraction_y);
    Transform2D xform = node->get_viewport_transform() * node->get_global_transform();
    return xform.xform(local_pos);
}

int RiveFileInstanceEditorPlugin::get_hovered_handle(Vector2 vp_mouse, RiveFileInstance *node) const {
    for (int i = 0; i < 8; i++) {
        if (vp_mouse.distance_to(get_handle_viewport_pos(i, node)) <= HANDLE_HOVER_RADIUS) {
            return i;
        }
    }
    return -1;
}

bool RiveFileInstanceEditorPlugin::_forward_canvas_gui_input(const Ref<InputEvent> &p_event) {
    if (!rive_editor || !rive_editor->get_node()) return false;
    RiveFileInstance *node = rive_editor->get_node();

    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
        if (mb->is_pressed()) {
            int handle_index = get_hovered_handle(mb->get_position(), node);
            if (handle_index >= 0) {
                dragging_handle = handle_index;

                Rect2 rect = node->get_rive_bounds();
                const ResizeHandle &handle = RESIZE_HANDLES[handle_index];
                Vector2 anchor_fraction(1.0f - handle.fraction_x, 1.0f - handle.fraction_y);
                Vector2 anchor_local = rect.position + rect.size * anchor_fraction;

                drag_anchor_parent = node->get_transform().xform(anchor_local);
                drag_start_scale = node->get_scale();
                drag_start_pos = node->get_position();
                drag_start_rect = rect;
                return true;
            }
        } else if (dragging_handle >= 0) {
            Vector2 final_scale = node->get_scale();
            Vector2 final_pos = node->get_position();
            Vector2 start_scale = drag_start_scale;
            Vector2 start_pos = drag_start_pos;

            EditorUndoRedoManager *undo_redo = get_undo_redo();
            undo_redo->create_action("Resize RiveFileInstance");
            undo_redo->add_do_method(node, "set_scale", final_scale);
            undo_redo->add_do_method(node, "set_position", final_pos);
            undo_redo->add_undo_method(node, "set_scale", start_scale);
            undo_redo->add_undo_method(node, "set_position", start_pos);
            undo_redo->commit_action(false);

            dragging_handle = -1;
            update_overlays();
            return true;
        }
    }

    Ref<InputEventMouseMotion> mm = p_event;
    if (mm.is_valid()) {
        if (dragging_handle >= 0) {
            Transform2D vp_to_parent = get_viewport_to_parent_xform(node);
            Vector2 mouse_parent = vp_to_parent.xform(mm->get_position());

            const ResizeHandle &handle = RESIZE_HANDLES[dragging_handle];
            Rect2 rect = drag_start_rect;

            Vector2 handle_fraction(handle.fraction_x, handle.fraction_y);
            Vector2 anchor_fraction = Vector2(1, 1) - handle_fraction;
            Vector2 anchor_local = rect.position + rect.size * anchor_fraction;
            Vector2 handle_local = rect.position + rect.size * handle_fraction;

            Vector2 new_scale = drag_start_scale;
            Vector2 new_pos = drag_start_pos;

            if (handle.resizes_x) {
                float denom = handle_local.x - anchor_local.x;
                if (Math::abs(denom) > 0.001f) {
                    float sx = (mouse_parent.x - drag_anchor_parent.x) / denom;
                    if (Math::abs(sx) > 0.01f) {
                        new_scale.x = sx;
                        new_pos.x = drag_anchor_parent.x - anchor_local.x * sx;
                    }
                }
            }
            if (handle.resizes_y) {
                float denom = handle_local.y - anchor_local.y;
                if (Math::abs(denom) > 0.001f) {
                    float sy = (mouse_parent.y - drag_anchor_parent.y) / denom;
                    if (Math::abs(sy) > 0.01f) {
                        new_scale.y = sy;
                        new_pos.y = drag_anchor_parent.y - anchor_local.y * sy;
                    }
                }
            }

            node->set_scale(new_scale);
            node->set_position(new_pos);
            update_overlays();
            return true;
        }

        int handle_index = get_hovered_handle(mm->get_position(), node);
        if (handle_index != hovering_handle) {
            hovering_handle = handle_index;
            update_overlays();
        }
    }

    return false;
}

void RiveFileInstanceEditorPlugin::_forward_canvas_draw_over_viewport(Control *p_overlay) {
    if (!rive_editor) return;
    RiveFileInstance *node = rive_editor->get_node();
    if (!node || !node->is_visible_in_tree()) return;

    Transform2D xform = node->get_viewport_transform() * node->get_global_transform();
    Rect2 rect = node->get_rive_bounds();

    PackedVector2Array selection_outline;
    selection_outline.push_back(xform.xform(rect.position));
    selection_outline.push_back(xform.xform(rect.position + Vector2(rect.size.x, 0)));
    selection_outline.push_back(xform.xform(rect.position + rect.size));
    selection_outline.push_back(xform.xform(rect.position + Vector2(0, rect.size.y)));
    selection_outline.push_back(xform.xform(rect.position));
    p_overlay->draw_polyline(selection_outline, Color(1, 0.5f, 0), 2.0f);

    const Color COLOR_NORMAL(1, 1, 1);
    const Color COLOR_HOVER(1, 0.8f, 0);
    const Color COLOR_BORDER(0.2f, 0.2f, 0.2f);

    for (int i = 0; i < 8; i++) {
        Vector2 center = get_handle_viewport_pos(i, node);
        Rect2 handle_rect(center - Vector2(HANDLE_HALF_SIZE, HANDLE_HALF_SIZE),
                Vector2(HANDLE_HALF_SIZE * 2, HANDLE_HALF_SIZE * 2));
        Color fill = (i == hovering_handle || i == dragging_handle) ? COLOR_HOVER : COLOR_NORMAL;
        p_overlay->draw_rect(handle_rect, fill);
        p_overlay->draw_rect(handle_rect, COLOR_BORDER, false, 1.0f);
    }
}
