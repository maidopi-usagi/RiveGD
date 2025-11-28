#ifndef RIVE_VIEW_MODEL_INSPECTOR_H
#define RIVE_VIEW_MODEL_INSPECTOR_H

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include "../scene/rive_control.h"

// Forward declarations
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

    struct PropertyEntry {
        rive::ViewModelInstanceValue* value;
        String path;
        String name;
        int indent_level;
    };

    RiveControl *_get_control() const;
    void _collect_properties(rive::ViewModelInstance *vm, String prefix, int indent, std::vector<PropertyEntry> &out_list);
    void _update_ui();
    
    void _on_prev_page();
    void _on_next_page();
    
    void _on_number_changed(double value, String path);
    void _on_string_changed(String value, String path);
    void _on_bool_changed(bool value, String path);
    void _on_color_changed(Color value, String path);
    void _on_enum_changed(int value, String path);
    void _on_trigger_fired(String path);

protected:
    static void _bind_methods();

public:
    void set_rive_control(RiveControl *p_control);
};

#endif // RIVE_VIEW_MODEL_INSPECTOR_H
