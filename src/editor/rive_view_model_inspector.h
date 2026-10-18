#ifndef RIVE_VIEW_MODEL_INSPECTOR_H
#define RIVE_VIEW_MODEL_INSPECTOR_H

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>
#include "../scene/rive_control.h"

namespace rive {
    class ViewModelInstance;
    class ViewModelInstanceValue;
}

using namespace godot;

class RiveViewModelInspector : public VBoxContainer {
    GDCLASS(RiveViewModelInspector, VBoxContainer);

private:
    ObjectID control_id;
    int current_page = 0;
    int items_per_page = 10;
    bool applying_edit = false;
    bool refresh_queued = false;
    bool structure_only = false;
    uint64_t ui_revision = 0;
    String edit_error;

    struct PropertyEntry {
        rive::ViewModelInstanceValue* value;
        String path;
        String name;
        int indent_level;
        String notice;
    };

    RiveControl *_get_control() const;
    void _collect_properties(rive::ViewModelInstance *vm, String prefix, int indent, std::vector<PropertyEntry> &out_list, std::vector<rive::ViewModelInstance *> &ancestors);
    void _update_ui();
    void _queue_refresh();
    void _refresh();
    void _commit_value(const String &path, const Variant &value);
    void _commit_values(const Dictionary &values, const String &path, bool p_merge = true);
    bool _snapshot_list(const String &p_path, Array &r_items);
    void _commit_list(const String &p_path, const Array &p_items);
    void _on_add_list_item(int p_choice, String p_path, Array p_choices);
    void _on_list_item_button(TreeItem *p_item, int p_column, int p_button, int p_mouse_button, Tree *p_tree);
    Variant _get_list_drag_data(Vector2 p_position, Tree *p_tree);
    bool _can_drop_list_data(Vector2 p_position, Variant p_data, Tree *p_tree);
    void _drop_list_data(Vector2 p_position, Variant p_data, Tree *p_tree);
    int _get_list_drop_index(Vector2 p_position, const Variant &p_data, Tree *p_tree) const;
    void _on_reset_property(String path);
    
    void _on_prev_page();
    void _on_next_page();
    
    void _on_number_changed(double value, String path);
    void _on_string_changed(String value, String path);
    void _on_bool_changed(bool value, String path);
    void _on_color_changed(Color value, String path);
    void _on_enum_changed(int value, String path, PackedInt32Array indices);
    void _on_artboard_changed(int value, String path, PackedStringArray names);
    void _on_resource_changed(Ref<Resource> resource, String path);
    void _on_resource_selected(Ref<Resource> resource, bool inspect);
    void _on_clear_list(String path);
    void _on_trigger_fired(String path);

protected:
    static void _bind_methods();

public:
    void set_structure_only(bool p_structure_only) { structure_only = p_structure_only; }
    void set_rive_control(RiveControl *p_control);
};

#endif
