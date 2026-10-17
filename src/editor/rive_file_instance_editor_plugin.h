#ifndef RIVE_FILE_INSTANCE_EDITOR_PLUGIN_H
#define RIVE_FILE_INSTANCE_EDITOR_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include "../scene/rive_file_instance.h"

using namespace godot;

class RiveFileInstanceEditor : public Control {
    GDCLASS(RiveFileInstanceEditor, Control);

    HBoxContainer *toolbar = nullptr;
    ObjectID toolbar_id;
    RiveFileInstance *node = nullptr;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    RiveFileInstanceEditor();
    ~RiveFileInstanceEditor();
    void edit(RiveFileInstance *p_node);
    HBoxContainer *get_toolbar() { return toolbar; }
    RiveFileInstance *get_node() { return node; }
};

class RiveFileInstanceEditorPlugin : public EditorPlugin {
    GDCLASS(RiveFileInstanceEditorPlugin, EditorPlugin);

    RiveFileInstanceEditor *rive_editor = nullptr;

    int dragging_handle = -1;
    int hovering_handle = -1;

    Vector2 drag_anchor_parent;
    Vector2 drag_start_scale;
    Vector2 drag_start_pos;
    Rect2 drag_start_rect;

    static constexpr float HANDLE_HALF_SIZE = 5.0f;
    static constexpr float HANDLE_HOVER_RADIUS = 8.0f;

    Vector2 get_handle_viewport_pos(int idx, RiveFileInstance *node) const;
    int get_hovered_handle(Vector2 vp_mouse, RiveFileInstance *node) const;
    Transform2D get_viewport_to_parent_xform(RiveFileInstance *node) const;

protected:
    static void _bind_methods();

public:
    virtual String _get_plugin_name() const override { return "RiveFileInstance"; }
    virtual bool _handles(Object *p_object) const override;
    virtual void _make_visible(bool p_visible) override;
    virtual void _edit(Object *p_object) override;
    virtual void _enter_tree() override;
    virtual void _exit_tree() override;

    virtual bool _forward_canvas_gui_input(const Ref<InputEvent> &p_event) override;
    virtual void _forward_canvas_draw_over_viewport(Control *p_overlay) override;
};

#endif
