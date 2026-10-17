#ifndef RIVE_INSPECTOR_PLUGIN_H
#define RIVE_INSPECTOR_PLUGIN_H

#include <godot_cpp/classes/editor_inspector_plugin.hpp>
#include <godot_cpp/classes/editor_property.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/editor_resource_picker.hpp>
#include "../scene/rive_control.h"

namespace rive {
    class ViewModelInstance;
}

using namespace godot;

class RiveResourceEditorProperty : public EditorProperty {
    GDCLASS(RiveResourceEditorProperty, EditorProperty);
    EditorResourcePicker *picker = nullptr;
    bool font_property = false;
    bool updating = false;
    void _on_resource_changed(const Ref<Resource> &p_resource);
    void _on_resource_selected(const Ref<Resource> &p_resource, bool p_inspect);
protected:
    static void _bind_methods();
public:
    RiveResourceEditorProperty();
    void setup(bool p_font_property);
    void _update_property() override;
    void _set_read_only(bool p_read_only) override;
};

class RiveInspectorPlugin : public EditorInspectorPlugin {
    GDCLASS(RiveInspectorPlugin, EditorInspectorPlugin);

protected:
    static void _bind_methods();

public:
    virtual bool _can_handle(Object *p_object) const override;
    virtual void _parse_begin(Object *p_object) override;
    bool _parse_property(Object *p_object, Variant::Type p_type, const String &p_name, PropertyHint p_hint_type,
            const String &p_hint_string, BitField<PropertyUsageFlags> p_usage_flags, bool p_wide) override;
};

#endif
