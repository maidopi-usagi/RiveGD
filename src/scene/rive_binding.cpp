#include "rive_binding.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/range.hpp>
#include <godot_cpp/classes/base_button.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>

using namespace godot;

void RiveBinding::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_node", "path"), &RiveBinding::set_rive_node);
    ClassDB::bind_method(D_METHOD("get_rive_node"), &RiveBinding::get_rive_node);
    ClassDB::bind_method(D_METHOD("set_target_node", "path"), &RiveBinding::set_target_node);
    ClassDB::bind_method(D_METHOD("get_target_node"), &RiveBinding::get_target_node);
    ClassDB::bind_method(D_METHOD("set_property_path", "path"), &RiveBinding::set_property_path);
    ClassDB::bind_method(D_METHOD("get_property_path"), &RiveBinding::get_property_path);
    ClassDB::bind_method(D_METHOD("set_target_property", "property"), &RiveBinding::set_target_property);
    ClassDB::bind_method(D_METHOD("get_target_property"), &RiveBinding::get_target_property);
    ClassDB::bind_method(D_METHOD("set_target_signal", "signal"), &RiveBinding::set_target_signal);
    ClassDB::bind_method(D_METHOD("get_target_signal"), &RiveBinding::get_target_signal);
    ClassDB::bind_method(D_METHOD("set_direction", "direction"), &RiveBinding::set_direction);
    ClassDB::bind_method(D_METHOD("get_direction"), &RiveBinding::get_direction);
    ClassDB::bind_method(D_METHOD("rebind"), &RiveBinding::rebind);
    ClassDB::bind_method(D_METHOD("sync_to_rive"), &RiveBinding::sync_to_rive);
    ClassDB::bind_method(D_METHOD("sync_to_godot"), &RiveBinding::sync_to_godot);
    ClassDB::bind_method(D_METHOD("get_binding_error"), &RiveBinding::get_binding_error);
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "rive_node"), "set_rive_node", "get_rive_node");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_path"), "set_property_path", "get_property_path");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_node"), "set_target_node", "get_target_node");
    ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "target_property"), "set_target_property", "get_target_property");
    ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "target_signal"), "set_target_signal", "get_target_signal");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "direction", PROPERTY_HINT_ENUM, "Rive To Godot,Godot To Rive,Two Way,Trigger To Method"), "set_direction", "get_direction");
    ADD_SIGNAL(MethodInfo("binding_failed", PropertyInfo(Variant::STRING, "message")));
    BIND_ENUM_CONSTANT(RIVE_TO_GODOT);
    BIND_ENUM_CONSTANT(GODOT_TO_RIVE);
    BIND_ENUM_CONSTANT(TWO_WAY);
    BIND_ENUM_CONSTANT(TRIGGER_TO_METHOD);
}

RiveBinding::~RiveBinding() { _disconnect_binding(); }
void RiveBinding::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) rebind();
    if (p_what == NOTIFICATION_ENTER_TREE && is_node_ready() && !Engine::get_singleton()->is_editor_hint()) call_deferred("rebind");
    if (p_what == NOTIFICATION_EXIT_TREE) _disconnect_binding();
}

void RiveBinding::_disconnect_binding() {
    ++binding_generation;
    updating = false;
    const Callable value_callback = callable_mp(this, &RiveBinding::_on_value_changed);
    const Callable trigger_callback = callable_mp(this, &RiveBinding::_on_trigger);
    if (property.is_valid()) {
        if (property->has_signal("value_changed") && property->is_connected("value_changed", value_callback)) property->disconnect("value_changed", value_callback);
        if (property->has_signal("triggered") && property->is_connected("triggered", trigger_callback)) property->disconnect("triggered", trigger_callback);
    }
    const Callable vm_callback = callable_mp(this, &RiveBinding::_on_vm_changed);
    if (view_model.is_valid() && view_model->is_connected("property_changed", vm_callback)) view_model->disconnect("property_changed", vm_callback);
    const Callable vm_trigger = callable_mp(this, &RiveBinding::_on_vm_trigger);
    if (view_model.is_valid() && view_model->is_connected("trigger_fired", vm_trigger)) view_model->disconnect("trigger_fired", vm_trigger);
    Object *source = ObjectDB::get_instance(source_id);
    const Callable loaded_callback = callable_mp(this, &RiveBinding::_on_rive_loaded);
    if (source && !connected_source_signal.is_empty() && source->has_signal(connected_source_signal) && source->is_connected(connected_source_signal, loaded_callback)) source->disconnect(connected_source_signal, loaded_callback);
    Object *target = ObjectDB::get_instance(target_id);
    const Callable target_callback = callable_mp(this, &RiveBinding::_on_target_changed);
    if (target && !connected_target_signal.is_empty() && target->is_connected(connected_target_signal, target_callback))
        target->disconnect(connected_target_signal, target_callback);
    property.unref();
    view_model.unref();
    source_id = ObjectID();
    target_id = ObjectID();
    connected_target_signal = StringName();
    connected_source_signal = StringName();
}

bool RiveBinding::rebind() {
    const ObjectID self_id(get_instance_id());
    _disconnect_binding();
    const uint64_t generation = binding_generation;
    binding_error = "";
    if (!is_inside_tree()) return false;
    auto fail = [&](const String &message) {
        const ObjectID waiting_source = source_id;
        _disconnect_binding();
        Object *source = ObjectDB::get_instance(waiting_source);
        if (source && (source->has_signal("view_model_changed") || source->has_signal("loaded"))) {
            source_id = waiting_source;
            connected_source_signal = source->has_signal("view_model_changed") ? "view_model_changed" : "loaded";
            source->connect(connected_source_signal, callable_mp(this, &RiveBinding::_on_rive_loaded));
        }
        binding_error = message;
        update_configuration_warnings();
        emit_signal("binding_failed", message);
        return false;
    };
    Node *source = rive_node.is_empty() ? nullptr : get_node_or_null(rive_node);
    Node *target = target_node.is_empty() ? nullptr : get_node_or_null(target_node);
    if (!source || !source->has_method("get_view_model_instance")) return fail("Rive source must expose get_view_model_instance().");
    source_id = source->get_instance_id();
    connected_source_signal = source->has_signal("view_model_changed") ? "view_model_changed" : "loaded";
    if (source->has_signal(connected_source_signal)) source->connect(connected_source_signal, callable_mp(this, &RiveBinding::_on_rive_loaded));
    if (!target) return fail("Target node was not found.");
    target_id = target->get_instance_id();
    const Ref<RiveViewModelInstance> resolved_model = source->call("get_view_model_instance");
    if (ObjectDB::get_instance(self_id) != this) return false;
    if (binding_generation != generation) return false;
    source = Object::cast_to<Node>(ObjectDB::get_instance(source_id));
    target = Object::cast_to<Node>(ObjectDB::get_instance(target_id));
    if (!source || !target) return fail("Source or target was freed while resolving its ViewModel.");
    view_model = resolved_model;
    if (view_model.is_null()) return fail("Rive source has no loaded ViewModel.");
    if (!view_model->has_property(property_path)) return fail("Unknown Rive property: " + property_path);
    property = view_model->get(property_path);
    if (property.is_null()) return fail("Rive property cannot be observed: " + property_path);
    if (direction == TRIGGER_TO_METHOD) {
        if (!Object::cast_to<RiveViewModelTrigger>(property.ptr()) || !target->has_method(target_property))
            return fail("Trigger binding requires a Trigger and a target method.");
        view_model->connect("trigger_fired", callable_mp(this, &RiveBinding::_on_vm_trigger));
    } else {
        if (!property->has_signal("value_changed")) return fail("Property does not provide value_changed.");
        bool valid = false;
        const Array properties = target->get_property_list();
        if (ObjectDB::get_instance(self_id) != this) return false;
        if (binding_generation != generation) return false;
        target = Object::cast_to<Node>(ObjectDB::get_instance(target_id));
        if (!target) return fail("Target was freed while resolving its properties.");
        for (int i = 0; i < properties.size(); ++i) {
            const Dictionary info = properties[i];
            if (StringName(info.get("name", StringName())) == target_property) { valid = true; break; }
        }
        if (!valid) return fail("Unknown target property: " + String(target_property));
        if (direction != GODOT_TO_RIVE) view_model->connect("property_changed", callable_mp(this, &RiveBinding::_on_vm_changed));
        if (direction != RIVE_TO_GODOT) {
            connected_target_signal = target_signal;
            if (connected_target_signal.is_empty()) {
                if (Object::cast_to<Range>(target) && target_property == StringName("value")) connected_target_signal = "value_changed";
                else if (Object::cast_to<BaseButton>(target) && target_property == StringName("button_pressed")) connected_target_signal = "toggled";
                else if (Object::cast_to<LineEdit>(target) && target_property == StringName("text")) connected_target_signal = "text_changed";
            }
            if (connected_target_signal.is_empty() || !target->has_signal(connected_target_signal))
                return fail("Two-way/source binding needs a one-argument change signal (or a Range, toggle Button, LineEdit adapter).");
            bool valid_signal = false;
            const Array signals = target->get_signal_list();
            for (int i = 0; i < signals.size(); ++i) {
                const Dictionary info = signals[i];
                if (StringName(info.get("name", StringName())) == connected_target_signal) {
                    const Array arguments = info.get("args", Array());
                    valid_signal = arguments.size() == 1;
                    break;
                }
            }
            if (!valid_signal) return fail("Target change signal must have exactly one argument.");
            if (auto *button = Object::cast_to<BaseButton>(target)) {
                if (connected_target_signal == StringName("toggled") && !button->is_toggle_mode())
                    return fail("Button binding requires toggle_mode to be enabled.");
            }
            target->connect(connected_target_signal, callable_mp(this, &RiveBinding::_on_target_changed));
        }
        if (direction == GODOT_TO_RIVE) sync_to_rive(); else sync_to_godot();
        if (ObjectDB::get_instance(self_id) != this) return false;
        if (binding_generation != generation) return false;
    }
    update_configuration_warnings();
    return true;
}

void RiveBinding::_on_rive_loaded() { rebind(); }
void RiveBinding::_on_vm_changed(const String &p_path, const Variant &) {
    if (updating || view_model.is_null()) return;
    const String configured = RiveViewModelInstance::canonicalize_property_path(property_path);
    const String changed = RiveViewModelInstance::canonicalize_property_path(p_path);
    if (configured != changed && !configured.begins_with(changed + String(".")) && !configured.begins_with(changed + String("["))) return;
    if (!view_model->has_property(property_path)) { property.unref(); return; }
    property = view_model->get(property_path);
    if (property.is_valid()) sync_to_godot();
}
void RiveBinding::_on_value_changed(const Variant &p_value) {
    Object *target = ObjectDB::get_instance(target_id);
    if (updating || !target) return;
    const ObjectID self_id(get_instance_id());
    updating = true;
    target->set(target_property, p_value);
    if (auto *live = Object::cast_to<RiveBinding>(ObjectDB::get_instance(self_id))) live->updating = false;
}
void RiveBinding::_on_target_changed(const Variant &p_value) {
    if (updating || view_model.is_null()) return;
    const ObjectID self_id(get_instance_id());
    const Ref<RiveViewModelInstance> vm = view_model;
    const String path = property_path;
    updating = true;
    const bool applied = vm->set_property_value(path, p_value);
    if (auto *live = Object::cast_to<RiveBinding>(ObjectDB::get_instance(self_id))) {
        if (!applied) {
            live->binding_error = "Target value is incompatible with " + path;
            live->emit_signal("binding_failed", live->binding_error);
        }
    }
    if (auto *live = Object::cast_to<RiveBinding>(ObjectDB::get_instance(self_id))) live->updating = false;
}
void RiveBinding::_on_trigger() {
    Object *target = ObjectDB::get_instance(target_id);
    if (target && !updating) {
        const ObjectID self_id(get_instance_id());
        updating = true;
        target->call(target_property);
        if (auto *live = Object::cast_to<RiveBinding>(ObjectDB::get_instance(self_id))) live->updating = false;
    }
}
void RiveBinding::_on_vm_trigger(const String &p_path) {
    if (RiveViewModelInstance::canonicalize_property_path(property_path) == RiveViewModelInstance::canonicalize_property_path(p_path)) _on_trigger();
}
void RiveBinding::sync_to_rive() {
    const ObjectID self_id(get_instance_id());
    const uint64_t generation = binding_generation;
    Object *target = ObjectDB::get_instance(target_id);
    if (!target) return;
    const Variant current = target->get(target_property);
    if (ObjectDB::get_instance(self_id) == this && binding_generation == generation) _on_target_changed(current);
}
void RiveBinding::sync_to_godot() {
    if (view_model.is_valid()) _on_value_changed(view_model->get_property_value(property_path));
}

void RiveBinding::set_rive_node(const NodePath &p_path) { rive_node = p_path; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }
void RiveBinding::set_target_node(const NodePath &p_path) { target_node = p_path; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }
void RiveBinding::set_property_path(const String &p_path) { property_path = p_path; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }
void RiveBinding::set_target_property(const StringName &p_property) { target_property = p_property; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }
void RiveBinding::set_target_signal(const StringName &p_signal) { target_signal = p_signal; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }
void RiveBinding::set_direction(Direction p_direction) { ERR_FAIL_COND(p_direction < RIVE_TO_GODOT || p_direction > TRIGGER_TO_METHOD); direction = p_direction; if (is_node_ready() && !Engine::get_singleton()->is_editor_hint()) rebind(); }

PackedStringArray RiveBinding::_get_configuration_warnings() const {
    PackedStringArray warnings;
    if (rive_node.is_empty() || target_node.is_empty()) warnings.push_back("Set both the Rive node and target node paths.");
    if (property_path.is_empty() || target_property.is_empty()) warnings.push_back("Set the Rive path and target property (or trigger method).");
    if (!binding_error.is_empty()) warnings.push_back(binding_error);
    return warnings;
}
