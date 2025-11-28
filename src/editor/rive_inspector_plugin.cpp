#include "rive_inspector_plugin.h"
#include "rive_view_model_inspector.h"
#include "../scene/rive_control.h"
#include "../scene/rive_player.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>

using namespace godot;

void RiveInspectorPlugin::_bind_methods() {}

bool RiveInspectorPlugin::_can_handle(Object *p_object) const {
    return Object::cast_to<RiveControl>(p_object) != nullptr;
}

void RiveInspectorPlugin::_parse_begin(Object *p_object) {
    RiveControl *control = Object::cast_to<RiveControl>(p_object);
    if (!control) return;

    RiveViewModelInspector *inspector = memnew(RiveViewModelInspector);
    inspector->set_rive_control(control);
    add_custom_control(inspector);
}
