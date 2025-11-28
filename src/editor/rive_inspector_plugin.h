#ifndef RIVE_INSPECTOR_PLUGIN_H
#define RIVE_INSPECTOR_PLUGIN_H

#include <godot_cpp/classes/editor_inspector_plugin.hpp>
#include <godot_cpp/classes/editor_property.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include "../scene/rive_control.h"

// Forward declarations
namespace rive {
    class ViewModelInstance;
}

using namespace godot;

class RiveInspectorPlugin : public EditorInspectorPlugin {
    GDCLASS(RiveInspectorPlugin, EditorInspectorPlugin);

protected:
    static void _bind_methods();

public:
    virtual bool _can_handle(Object *p_object) const override;
    virtual void _parse_begin(Object *p_object) override;
};

#endif // RIVE_INSPECTOR_PLUGIN_H
