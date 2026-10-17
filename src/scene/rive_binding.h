#ifndef RIVE_BINDING_H
#define RIVE_BINDING_H

#include <godot_cpp/classes/node.hpp>
#include "rive_view_model.h"

class RiveBinding : public godot::Node {
    GDCLASS(RiveBinding, godot::Node);
public:
    enum Direction { RIVE_TO_GODOT, GODOT_TO_RIVE, TWO_WAY, TRIGGER_TO_METHOD };
private:
    godot::NodePath rive_node;
    godot::NodePath target_node;
    godot::String property_path;
    godot::StringName target_property;
    godot::StringName target_signal;
    Direction direction = RIVE_TO_GODOT;
    godot::ObjectID source_id;
    godot::ObjectID target_id;
    godot::StringName connected_target_signal;
    godot::StringName connected_source_signal;
    godot::Ref<RiveViewModelInstance> view_model;
    godot::Ref<RiveViewModelProperty> property;
    bool updating = false;
    uint64_t binding_generation = 0;
    godot::String binding_error;
    void _disconnect_binding();
    void _on_rive_loaded();
    void _on_value_changed(const godot::Variant &p_value);
    void _on_vm_changed(const godot::String &p_path, const godot::Variant &p_value);
    void _on_target_changed(const godot::Variant &p_value);
    void _on_trigger();
    void _on_vm_trigger(const godot::String &p_path);
protected:
    static void _bind_methods();
    void _notification(int p_what);
public:
    ~RiveBinding();
    void set_rive_node(const godot::NodePath &p_path);
    godot::NodePath get_rive_node() const { return rive_node; }
    void set_target_node(const godot::NodePath &p_path);
    godot::NodePath get_target_node() const { return target_node; }
    void set_property_path(const godot::String &p_path);
    godot::String get_property_path() const { return property_path; }
    void set_target_property(const godot::StringName &p_property);
    godot::StringName get_target_property() const { return target_property; }
    void set_target_signal(const godot::StringName &p_signal);
    godot::StringName get_target_signal() const { return target_signal; }
    void set_direction(Direction p_direction);
    Direction get_direction() const { return direction; }
    bool rebind();
    void sync_to_rive();
    void sync_to_godot();
    godot::String get_binding_error() const { return binding_error; }
    godot::PackedStringArray _get_configuration_warnings() const override;
};
VARIANT_ENUM_CAST(RiveBinding::Direction);
#endif
