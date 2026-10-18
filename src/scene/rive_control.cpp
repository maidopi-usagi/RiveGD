#include "rive_control.h"
#include "../renderer/rive_renderer.h"
#include "../rive_constants.h"
#include "../rive_variant_utils.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <godot_cpp/classes/input_event_pan_gesture.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include "rive/layout.hpp"
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_artboard.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_blob.hpp>
#include <rive/viewmodel/viewmodel_instance_symbol_list_index.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace godot;

namespace {
bool is_persistent_override(const Variant &p_value, int p_depth = 0) {
    if (p_depth > 32) return false;
    if (p_value.get_type() == Variant::OBJECT) {
        Object *object = p_value;
        return !object || Object::cast_to<Resource>(object);
    }
    if (p_value.get_type() == Variant::ARRAY) {
        const Array items = p_value;
        if (items.size() > 1000) return false;
        for (int i = 0; i < items.size(); ++i) {
            if (items[i].get_type() != Variant::OBJECT || Ref<RiveViewModelPreset>(items[i]).is_null()) return false;
        }
        return true;
    }
    if (p_value.get_type() == Variant::DICTIONARY) {
        const Dictionary values = p_value;
        const Array keys = values.keys();
        for (int i = 0; i < keys.size(); ++i) {
            if (!is_persistent_override(keys[i], p_depth + 1) ||
                    !is_persistent_override(values[keys[i]], p_depth + 1)) return false;
        }
    }
    return true;
}

Variant copy_override_containers(const Variant &p_value) {
    if (p_value.get_type() == Variant::ARRAY) return Array(p_value).duplicate(true);
    if (p_value.get_type() == Variant::DICTIONARY) return Dictionary(p_value).duplicate(true);
    return p_value;
}

bool instantiate_list_items(const Array &p_presets, const Ref<RiveFile> &p_file, Array &r_items) {
    if (p_presets.size() > 1000) return false;
    for (int i = 0; i < p_presets.size(); ++i) {
        const Ref<RiveViewModelPreset> preset = p_presets[i];
        if (preset.is_null()) return false;
        const auto item = preset->instantiate(p_file);
        if (item.is_null()) return false;
        r_items.push_back(item);
    }
    return true;
}

bool apply_list_presets(const Ref<RiveViewModelInstance> &p_vm, const Dictionary &p_values, const Ref<RiveFile> &p_file) {
    Array paths = p_values.keys();
    paths.sort();
    for (int i = 0; i < paths.size(); ++i) {
        const Variant value = p_values[paths[i]];
        if (value.get_type() != Variant::ARRAY) continue;
        Array items;
        if (p_vm.is_null() || !instantiate_list_items(value, p_file, items) || !p_vm->set_property_value(paths[i], items)) return false;
    }
    return true;
}

void collect_presets(const Variant &p_value, std::vector<Ref<RiveViewModelPreset>> &p_presets,
        int p_depth, int &p_count) {
    if (p_depth >= 32 || p_count >= 1000) return;
    ++p_count;
    if (p_value.get_type() == Variant::OBJECT) {
        const Ref<RiveViewModelPreset> preset = p_value;
        if (preset.is_null() || std::find(p_presets.begin(), p_presets.end(), preset) != p_presets.end()) return;
        p_presets.push_back(preset);
        collect_presets(preset->get_values(), p_presets, p_depth + 1, p_count);
    } else if (p_value.get_type() == Variant::DICTIONARY) {
        const Dictionary values = p_value;
        const Array keys = values.keys();
        for (int i = 0; i < keys.size() && p_count < 1000; ++i) {
            collect_presets(values[keys[i]], p_presets, p_depth + 1, p_count);
        }
    } else if (p_value.get_type() == Variant::ARRAY) {
        const Array values = p_value;
        for (int i = 0; i < values.size() && p_count < 1000; ++i) {
            collect_presets(values[i], p_presets, p_depth + 1, p_count);
        }
    }
}

template <typename T> rive::ViewModelInstanceValue *vm_value(T *p_value) { return p_value; }
template <typename T> rive::ViewModelInstanceValue *vm_value(const rive::rcp<T> &p_value) { return p_value.get(); }

void collect_properties(const Ref<RiveViewModelInstance> &p_root, rive::ViewModelInstance *p_vm,
        const String &p_prefix, List<PropertyInfo> *p_list, Dictionary *p_defaults,
        std::vector<rive::ViewModelInstance *> &p_ancestors, int &p_count) {
    if (!p_vm || p_ancestors.size() >= 32 || p_count >= 1000 ||
            std::find(p_ancestors.begin(), p_ancestors.end(), p_vm) != p_ancestors.end()) return;
    p_ancestors.push_back(p_vm);
    for (const auto &entry : p_vm->propertyValues()) {
        if (p_count >= 1000) break;
        rive::ViewModelInstanceValue *value = vm_value(entry);
        if (!value) continue;
        ++p_count;
        const String name = String::utf8(value->name().c_str());
        const String path = p_prefix.is_empty() ? name : p_prefix + String(".") + name;
        PropertyInfo property(Variant::NIL, String(RiveConstants::PROPERTY_PREFIX) + path,
                PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR);
        if (value->is<rive::ViewModelInstanceNumber>()) property.type = Variant::FLOAT;
        else if (value->is<rive::ViewModelInstanceString>()) property.type = Variant::STRING;
        else if (value->is<rive::ViewModelInstanceBoolean>()) property.type = Variant::BOOL;
        else if (value->is<rive::ViewModelInstanceColor>()) property.type = Variant::COLOR;
        else if (value->is<rive::ViewModelInstanceAssetBlob>()) property.type = Variant::PACKED_BYTE_ARRAY;
        else if (value->is<rive::ViewModelInstanceSymbolListIndex>()) {
            property.type = Variant::INT;
            property.usage |= PROPERTY_USAGE_READ_ONLY;
        }
        else if (value->is<rive::ViewModelInstanceEnum>()) {
            property.type = Variant::INT;
            property.hint = PROPERTY_HINT_ENUM;
            Ref<RiveViewModelEnum> enum_value = p_root->get_enum_property(path);
            const PackedStringArray options = enum_value.is_valid() ? enum_value->get_values() : PackedStringArray();
            for (int i = 0; i < options.size(); ++i) {
                if (i) property.hint_string += ",";
                property.hint_string += options[i].replace(",", " ").replace(":", " ") + ":" + String::num_int64(i);
            }
        } else if (value->is<rive::ViewModelInstanceAssetImage>() || value->is<rive::ViewModelInstanceAssetFont>()) {
            property.type = Variant::OBJECT;
            property.hint = PROPERTY_HINT_RESOURCE_TYPE;
            property.hint_string = value->is<rive::ViewModelInstanceAssetImage>() ? "Texture2D" : "FontFile";
        } else if (value->is<rive::ViewModelInstanceArtboard>()) {
            property.type = Variant::STRING;
            property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
            Ref<RiveViewModelArtboard> artboard_value = p_root->get_artboard_property(path);
            const PackedStringArray options = artboard_value.is_valid() ? artboard_value->get_artboard_list() : PackedStringArray();
            for (int i = 0; i < options.size(); ++i) {
                if (i) property.hint_string += ",";
                property.hint_string += options[i];
            }
        } else if (value->is<rive::ViewModelInstanceViewModel>()) {
            auto child = value->as<rive::ViewModelInstanceViewModel>()->referenceViewModelInstance();
            collect_properties(p_root, child.get(), path, p_list, p_defaults, p_ancestors, p_count);
            continue;
        } else if (value->is<rive::ViewModelInstanceList>()) {
            auto items = value->as<rive::ViewModelInstanceList>()->listItems();
            for (size_t i = 0; i < items.size() && p_count < 1000; ++i) {
                if (items[i]) collect_properties(p_root, items[i]->viewModelInstance().get(),
                        path + String("[") + String::num_int64(i) + "]", p_list, p_defaults, p_ancestors, p_count);
            }
            continue;
        } else continue;
        if (p_list) p_list->push_back(property);
        if (p_defaults) (*p_defaults)[RiveViewModelInstance::canonicalize_property_path(path)] = p_root->get_property_value(path);
    }
    p_ancestors.pop_back();
}
}

void RiveControl::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_rive_file", "file"), &RiveControl::set_rive_file);
    ClassDB::bind_method(D_METHOD("get_rive_file"), &RiveControl::get_rive_file);
    ClassDB::bind_method(D_METHOD("_on_rive_file_changed"), &RiveControl::_on_rive_file_changed);
    ClassDB::bind_method(D_METHOD("load_file"), &RiveControl::load_file);
    ClassDB::bind_method(D_METHOD("set_view_model_preset", "preset"), &RiveControl::set_view_model_preset);
    ClassDB::bind_method(D_METHOD("get_view_model_preset"), &RiveControl::get_view_model_preset);
    ClassDB::bind_method(D_METHOD("_on_preset_changed"), &RiveControl::_on_preset_changed);
    ClassDB::bind_method(D_METHOD("_on_mouse_exited"), &RiveControl::_on_mouse_exited);
    ClassDB::bind_method(D_METHOD("_on_joy_connection_changed", "device", "connected"), &RiveControl::_on_joy_connection_changed);
    ClassDB::bind_method(D_METHOD("_invalidate_rive"), &RiveControl::_invalidate_rive);
    ClassDB::bind_method(D_METHOD("_on_player_changed"), &RiveControl::_on_player_changed);
    ClassDB::bind_method(D_METHOD("_on_player_event", "name", "payload"), &RiveControl::_on_player_event);
    ClassDB::bind_method(D_METHOD("_on_player_animation_finished", "name"), &RiveControl::_on_player_animation_finished);
    ClassDB::bind_method(D_METHOD("_on_player_animation_looped", "name"), &RiveControl::_on_player_animation_looped);
    ClassDB::bind_method(D_METHOD("_on_player_state_changed", "name"), &RiveControl::_on_player_state_changed);

    ClassDB::bind_method(D_METHOD("set_artboard_name", "name"), &RiveControl::set_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_name"), &RiveControl::get_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_list"), &RiveControl::get_artboard_list);

    ClassDB::bind_method(D_METHOD("set_fit", "fit"), &RiveControl::set_fit);
    ClassDB::bind_method(D_METHOD("get_fit"), &RiveControl::get_fit);
    ClassDB::bind_method(D_METHOD("set_alignment", "alignment"), &RiveControl::set_alignment);
    ClassDB::bind_method(D_METHOD("get_alignment"), &RiveControl::get_alignment);
    ClassDB::bind_method(D_METHOD("set_expand_mode", "mode"), &RiveControl::set_expand_mode);
    ClassDB::bind_method(D_METHOD("get_expand_mode"), &RiveControl::get_expand_mode);
    ClassDB::bind_method(D_METHOD("get_intrinsic_size"), &RiveControl::get_intrinsic_size);
    ClassDB::bind_method(D_METHOD("get_layout_size"), &RiveControl::get_layout_size);
    ClassDB::bind_method(D_METHOD("get_artboard_transform"), &RiveControl::get_artboard_transform);
    ClassDB::bind_method(D_METHOD("set_layout_scale", "scale"), &RiveControl::set_layout_scale);
    ClassDB::bind_method(D_METHOD("get_layout_scale"), &RiveControl::get_layout_scale);
    ClassDB::bind_method(D_METHOD("set_playing", "playing"), &RiveControl::set_playing);
    ClassDB::bind_method(D_METHOD("is_playing"), &RiveControl::is_playing);
    ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &RiveControl::set_speed_scale);
    ClassDB::bind_method(D_METHOD("get_speed_scale"), &RiveControl::get_speed_scale);
    ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &RiveControl::set_update_mode);
    ClassDB::bind_method(D_METHOD("get_update_mode"), &RiveControl::get_update_mode);
    ClassDB::bind_method(D_METHOD("play"), &RiveControl::play);
    ClassDB::bind_method(D_METHOD("pause"), &RiveControl::pause);
    ClassDB::bind_method(D_METHOD("stop"), &RiveControl::stop);
    ClassDB::bind_method(D_METHOD("reset"), &RiveControl::reset);
    ClassDB::bind_method(D_METHOD("advance", "delta"), &RiveControl::advance);
    ClassDB::bind_method(D_METHOD("seek", "time"), &RiveControl::seek);
    ClassDB::bind_method(D_METHOD("get_time"), &RiveControl::get_time);
    ClassDB::bind_method(D_METHOD("get_duration"), &RiveControl::get_duration);
    ClassDB::bind_method(D_METHOD("get_player"), &RiveControl::get_player);
    ClassDB::bind_method(D_METHOD("set_input_value", "name", "value", "path"), &RiveControl::set_input_value, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_input_value", "name", "path"), &RiveControl::get_input_value, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("fire_input", "name", "path"), &RiveControl::fire_input, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_input_list", "path"), &RiveControl::get_input_list, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("set_text_run", "name", "value", "path"), &RiveControl::set_text_run, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_text_run", "name", "path"), &RiveControl::get_text_run, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("set_volume", "volume"), &RiveControl::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &RiveControl::get_volume);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");

    ClassDB::bind_method(D_METHOD("play_animation", "name"), &RiveControl::play_animation);
    ClassDB::bind_method(D_METHOD("play_state_machine", "name"), &RiveControl::play_state_machine);
    ClassDB::bind_method(D_METHOD("get_animation_list"), &RiveControl::get_animation_list);
    ClassDB::bind_method(D_METHOD("get_state_machine_list"), &RiveControl::get_state_machine_list);

    ClassDB::bind_method(D_METHOD("set_animation_name", "name"), &RiveControl::set_animation_name);
    ClassDB::bind_method(D_METHOD("get_animation_name"), &RiveControl::get_animation_name);
    ClassDB::bind_method(D_METHOD("set_state_machine_name", "name"), &RiveControl::set_state_machine_name);
    ClassDB::bind_method(D_METHOD("get_state_machine_name"), &RiveControl::get_state_machine_name);

    ClassDB::bind_method(D_METHOD("set_text_value", "property_path", "value"), &RiveControl::set_text_value);
    ClassDB::bind_method(D_METHOD("set_number_value", "property_path", "value"), &RiveControl::set_number_value);
    ClassDB::bind_method(D_METHOD("set_boolean_value", "property_path", "value"), &RiveControl::set_boolean_value);
    ClassDB::bind_method(D_METHOD("fire_trigger", "property_path"), &RiveControl::fire_trigger);
    ClassDB::bind_method(D_METHOD("set_enum_value", "property_path", "value"), &RiveControl::set_enum_value);
    ClassDB::bind_method(D_METHOD("set_color_value", "property_path", "value"), &RiveControl::set_color_value);
    ClassDB::bind_method(D_METHOD("set_image_value", "property_path", "value"), &RiveControl::set_image_value);
    ClassDB::bind_method(D_METHOD("set_font_value", "property_path", "value"), &RiveControl::set_font_value);
    ClassDB::bind_method(D_METHOD("set_artboard_value", "property_path", "value"), &RiveControl::set_artboard_value);
    ClassDB::bind_method(D_METHOD("set_list_value", "property_path", "value"), &RiveControl::set_list_value);
    ClassDB::bind_method(D_METHOD("set_property_value", "property_path", "value"), &RiveControl::set_property_value);
    ClassDB::bind_method(D_METHOD("get_property_value", "property_path"), &RiveControl::get_property_value);
    ClassDB::bind_method(D_METHOD("get_property_override", "property_path"), &RiveControl::get_property_override);
    ClassDB::bind_method(D_METHOD("has_property_override", "property_path"), &RiveControl::has_property_override);
    ClassDB::bind_method(D_METHOD("clear_property_override", "property_path"), &RiveControl::clear_property_override);
    ClassDB::bind_method(D_METHOD("get_view_model_instance"), &RiveControl::get_view_model_instance);
    ClassDB::bind_method(D_METHOD("bind_view_model", "instance"), &RiveControl::bind_view_model);

    ClassDB::bind_method(D_METHOD("set_property_values", "values"), &RiveControl::set_property_values);
    ClassDB::bind_method(D_METHOD("get_property_values"), &RiveControl::get_property_values);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "view_model_preset", PROPERTY_HINT_RESOURCE_TYPE, "RiveViewModelPreset"), "set_view_model_preset", "get_view_model_preset");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing", "is_playing");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "update_mode", PROPERTY_HINT_ENUM, "Always,When Needed,Manual"), "set_update_mode", "get_update_mode");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fit", PROPERTY_HINT_ENUM, "Fill,Contain,Cover,Fit Width,Fit Height,None,Scale Down,Layout"), "set_fit", "get_fit");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "expand_mode", PROPERTY_HINT_ENUM, "Ignore Size,Keep Size"), "set_expand_mode", "get_expand_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "layout_scale", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater"), "set_layout_scale", "get_layout_scale");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Top Left,Top Center,Top Right,Center Left,Center,Center Right,Bottom Left,Bottom Center,Bottom Right"), "set_alignment", "get_alignment");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "property_values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_property_values", "get_property_values");

    ADD_SIGNAL(MethodInfo("loaded"));
    ADD_SIGNAL(MethodInfo("load_failed", PropertyInfo(Variant::INT, "error"), PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("property_values_changed"));
    ADD_SIGNAL(MethodInfo("view_model_changed"));
    ADD_SIGNAL(MethodInfo("rive_event", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "payload")));
    ADD_SIGNAL(MethodInfo("animation_finished", PropertyInfo(Variant::STRING, "name")));
    ADD_SIGNAL(MethodInfo("animation_looped", PropertyInfo(Variant::STRING, "name")));
    ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::STRING, "name")));

    BIND_ENUM_CONSTANT(FIT_FILL);
    BIND_ENUM_CONSTANT(FIT_CONTAIN);
    BIND_ENUM_CONSTANT(FIT_COVER);
    BIND_ENUM_CONSTANT(FIT_WIDTH);
    BIND_ENUM_CONSTANT(FIT_HEIGHT);
    BIND_ENUM_CONSTANT(FIT_NONE);
    BIND_ENUM_CONSTANT(FIT_SCALE_DOWN);
    BIND_ENUM_CONSTANT(FIT_LAYOUT);
    BIND_ENUM_CONSTANT(EXPAND_IGNORE_SIZE);
    BIND_ENUM_CONSTANT(EXPAND_KEEP_SIZE);
    BIND_ENUM_CONSTANT(UPDATE_ALWAYS);
    BIND_ENUM_CONSTANT(UPDATE_WHEN_NEEDED);
    BIND_ENUM_CONSTANT(UPDATE_MANUAL);

    BIND_ENUM_CONSTANT(ALIGNMENT_TOP_LEFT);
    BIND_ENUM_CONSTANT(ALIGNMENT_TOP_CENTER);
    BIND_ENUM_CONSTANT(ALIGNMENT_TOP_RIGHT);
    BIND_ENUM_CONSTANT(ALIGNMENT_CENTER_LEFT);
    BIND_ENUM_CONSTANT(ALIGNMENT_CENTER);
    BIND_ENUM_CONSTANT(ALIGNMENT_CENTER_RIGHT);
    BIND_ENUM_CONSTANT(ALIGNMENT_BOTTOM_LEFT);
    BIND_ENUM_CONSTANT(ALIGNMENT_BOTTOM_CENTER);
    BIND_ENUM_CONSTANT(ALIGNMENT_BOTTOM_RIGHT);
}

RiveControl::RiveControl()
{
    set_mouse_filter(MOUSE_FILTER_STOP);
    set_focus_mode(FOCUS_ALL);
    rive_player.instantiate();
    _connect_player_signals(rive_player);
    connect("mouse_exited", Callable(this, "_on_mouse_exited"));
    texture_target.instantiate();
    RiveRenderRegistry::get_singleton()->add_drawable(this);
}

RiveControl::~RiveControl()
{
    _disconnect_preset_connections();
    _disconnect_player_signals(rive_player);
    if (rive_file.is_valid()) {
        Callable changed_callback(this, "_on_rive_file_changed");
        if (rive_file->is_connected("changed", changed_callback)) {
            rive_file->disconnect("changed", changed_callback);
        }
    }
    RiveRenderRegistry::get_singleton()->remove_drawable(this);
    if (texture_target.is_valid()) {
        texture_target->clear();
    }
}

void RiveControl::_notification(int p_what)
{
    switch (p_what)
    {
    case NOTIFICATION_ENTER_TREE:
        set_process(true);
        if (rive_file.is_valid()) {
            load_file();
        }
        break;
    case NOTIFICATION_EXIT_TREE:
        {
        const ObjectID self_id(get_instance_id());
        const Ref<RivePlayer> player = rive_player;
        if (player.is_valid()) player->clear_focus();
        if (ObjectDB::get_instance(self_id) != this) return;
        _sync_keyboard(true);
        }
        break;
    case NOTIFICATION_RESIZED:
        _update_layout();
        _invalidate_rive();
        update_configuration_warnings();
        break;
    case NOTIFICATION_FOCUS_ENTER:
        {
        const ObjectID self_id(get_instance_id());
        const Ref<RivePlayer> player = rive_player;
        if (player.is_valid() && (!player->get_state_machine() || !player->get_state_machine()->focusState().hasFocus)) {
            player->focus_next();
        }
        if (ObjectDB::get_instance(self_id) != this) return;
        _sync_keyboard();
        }
        break;
    case NOTIFICATION_FOCUS_EXIT:
        {
        const ObjectID self_id(get_instance_id());
        const Ref<RivePlayer> player = rive_player;
        if (player.is_valid()) player->clear_focus();
        if (ObjectDB::get_instance(self_id) != this) return;
        _sync_keyboard(true);
        }
        break;
    case MainLoop::NOTIFICATION_OS_IME_UPDATE:
        if (ime_active && has_focus() && rive_player.is_valid()) {
            const Ref<RivePlayer> player = rive_player;
            DisplayServer *display = DisplayServer::get_singleton();
            if (display) {
                const String preedit = display->ime_get_text();
                if (preedit.is_empty()) player->cancel_text_composition();
                else player->update_text_composition(preedit, display->ime_get_selection());
            }
        }
        break;
    case NOTIFICATION_DRAW:
        if (!is_inside_tree()) break;
        if (texture_target.is_valid())
        {
            if (texture_target->get_texture_rd().is_valid())
            {
                draw_texture(texture_target->get_texture_rd(), Point2());
            }
            else if (texture_target->get_texture_rid().is_valid())
            {
                RenderingServer::get_singleton()->canvas_item_add_texture_rect(get_canvas_item(), Rect2(Point2(), get_size()), texture_target->get_texture_rid());
            }
        }
        break;
    case NOTIFICATION_PROCESS:
        if (!is_inside_tree()) break;
        if (rive_player.is_valid() && rive_player->get_artboard())
        {
            const ObjectID self_id(get_instance_id());
            const Ref<RivePlayer> player = rive_player;
            const bool changed = player->flush_view_model_changes();
            if (ObjectDB::get_instance(self_id) != this) return;
            if (changed) {
                _invalidate_rive();
                _refresh_property_schema();
            }
            const bool automatic = update_mode != UPDATE_MANUAL &&
                    (update_mode == UPDATE_ALWAYS || needs_advance || render_dirty);
            if (automatic && (playing || render_dirty)) {
                const bool active = player->advance(playing ? get_process_delta_time() * speed_scale : 0.0);
                if (ObjectDB::get_instance(self_id) != this) return;
                needs_advance = active;
                render_dirty = true;
                player->flush_view_model_changes();
                if (ObjectDB::get_instance(self_id) != this) return;
            }
            if (render_dirty) {
                _render_rive();
                queue_redraw();
            }
            _sync_keyboard();
        }
        break;
    }
}

void RiveControl::_render_rive()
{
    Size2i size = get_size();
    if (size.width <= 0 || size.height <= 0)
        return;

    if (!texture_target.is_valid()) return;

    texture_target->resize(size);
    if (!texture_target->is_valid()) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice *rd = rs->get_rendering_device();

    rive_integration::render_texture(rd, texture_target->get_texture_rid(), this, size.width, size.height);
    render_dirty = false;
}

void RiveControl::_invalidate_rive() {
    render_dirty = true;
    needs_advance = true;
    queue_redraw();
}

void RiveControl::_on_player_changed() {
    _invalidate_rive();
    if (loading) return;
    const Ref<RiveViewModelInstance> vm = get_view_model_instance();
    const ObjectID identity = vm.is_valid() ? ObjectID(vm->get_instance_id()) : ObjectID();
    if (identity == reported_view_model_id) return;
    reported_view_model_id = identity;
    _refresh_property_schema();
    emit_signal("view_model_changed");
}

void RiveControl::_connect_player_signals(const Ref<RivePlayer> &p_player) {
    if (p_player.is_valid()) p_player->set_volume(volume);
    Input *input = Input::get_singleton();
    const Callable connection_callback(this, "_on_joy_connection_changed");
    if (input && !input->is_connected("joy_connection_changed", connection_callback)) input->connect("joy_connection_changed", connection_callback);
    if (p_player.is_null()) return;
    const char *signals[] = { "changed", "rive_event", "animation_finished", "animation_looped", "state_changed" };
    const char *methods[] = { "_on_player_changed", "_on_player_event", "_on_player_animation_finished", "_on_player_animation_looped", "_on_player_state_changed" };
    for (int i = 0; i < 5; ++i) {
        const Callable callback(this, methods[i]);
        if (!p_player->is_connected(signals[i], callback)) p_player->connect(signals[i], callback);
    }
}

void RiveControl::_disconnect_player_signals(const Ref<RivePlayer> &p_player) {
    if (p_player.is_null()) return;
    const char *signals[] = { "changed", "rive_event", "animation_finished", "animation_looped", "state_changed" };
    const char *methods[] = { "_on_player_changed", "_on_player_event", "_on_player_animation_finished", "_on_player_animation_looped", "_on_player_state_changed" };
    for (int i = 0; i < 5; ++i) {
        const Callable callback(this, methods[i]);
        if (p_player->is_connected(signals[i], callback)) p_player->disconnect(signals[i], callback);
    }
}

void RiveControl::_sync_keyboard(bool p_release) {
    const bool text_focus = !p_release && has_focus() && rive_player.is_valid() && rive_player->get_state_machine() &&
            rive_player->get_state_machine()->focusState().expectsKeyboardInput;
    Window *window = is_inside_tree() ? get_window() : nullptr;
    if (ime_active != text_focus) {
        if (window) window->set_ime_active(text_focus);
        ime_active = text_focus;
    }
    const Dictionary text_state = text_focus ? rive_player->get_text_input_state() : Dictionary();
    Vector2 caret_position(0, get_size().y);
    if (text_state.has("caret_rect")) caret_position = get_artboard_transform().xform(Rect2(text_state["caret_rect"]).get_end());
    const Vector2 ime_position = is_inside_tree() ? get_global_transform_with_canvas().xform(caret_position) : Vector2();
    if (text_focus && window) window->set_ime_position(ime_position);
    DisplayServer *display = DisplayServer::get_singleton();
    if (display && display->has_feature(DisplayServer::FEATURE_VIRTUAL_KEYBOARD) && virtual_keyboard_active != text_focus) {
        if (text_focus) {
            display->virtual_keyboard_show(text_state.get("text", String()), Rect2(ime_position, Vector2(1, 1)),
                    text_state.get("obscured", false) ? DisplayServer::KEYBOARD_TYPE_PASSWORD :
                        text_state.get("multiline", false) ? DisplayServer::KEYBOARD_TYPE_MULTILINE : DisplayServer::KEYBOARD_TYPE_DEFAULT, -1,
                    text_state.get("selection_from", -1), text_state.get("selection_to", -1));
        } else display->virtual_keyboard_hide();
        virtual_keyboard_active = text_focus;
    }
}

Transform2D RiveControl::get_artboard_transform() const {
    const rive::Mat2D value = _get_rive_transform();
    return Transform2D(value[0], value[1], value[2], value[3], value[4], value[5]);
}

void RiveControl::_update_layout() {
    if (rive_player.is_null() || !rive_player->get_artboard()) return;
    const Ref<RivePlayer> player = rive_player;
    rive::ArtboardInstance *artboard = player->get_artboard();
    const bool layout = fit == FIT_LAYOUT;
    const Vector2 size = get_size() / layout_scale;
    rive_integration::run_on_rive_render_thread([&]() {
        if (layout) {
            if (size.x > 0.0 && size.y > 0.0) {
                artboard->width(size.x);
                artboard->height(size.y);
            }
        } else {
            artboard->resetSize();
        }
        artboard->advance(0.0f);
    });
}

void RiveControl::_refresh_property_schema() {
    List<PropertyInfo> properties;
    _get_property_list(&properties);
    String signature;
    for (const PropertyInfo &property : properties) {
        signature += String(property.name) + ":" + String::num_int64(property.type) + ":" + property.hint_string + "\n";
    }
    if (signature != property_schema_signature) {
        property_schema_signature = signature;
        notify_property_list_changed();
    }
}

void RiveControl::set_rive_file(const Ref<RiveFile> &p_file)
{
    if (rive_file == p_file)
        return;

    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    const Callable changed_callback(this, "_on_rive_file_changed");
    const Ref<RiveFile> previous_file = rive_file;
    if (previous_file.is_valid()) {
        if (previous_file->is_connected("changed", changed_callback)) {
            previous_file->disconnect("changed", changed_callback);
        }
    }

    rive_file = p_file;

    if (rive_file.is_valid()) {
        if (!rive_file->is_connected("changed", changed_callback)) {
            rive_file->connect("changed", changed_callback);
        }
    }

    notify_property_list_changed();

    if (rive_file.is_null()) {
        loading = true;
        player->set_artboard(std::unique_ptr<rive::ArtboardInstance>());
        if (ObjectDB::get_instance(self_id) != this) return;
        loading = false;
        property_defaults.clear();
        intrinsic_size = Vector2();
        load_error = "";
        if (texture_target.is_valid()) texture_target->clear();
        update_minimum_size();
        notify_property_list_changed();
        _invalidate_rive();
        reported_view_model_id = ObjectID();
        _sync_keyboard(true);
        emit_signal("view_model_changed");
    } else if (is_inside_tree()) {
        if (load_file() != OK) {
            if (ObjectDB::get_instance(self_id) != this) return;
            if (rive_file->is_connected("changed", changed_callback)) {
                rive_file->disconnect("changed", changed_callback);
            }
            rive_file = previous_file;
            if (rive_file.is_valid() && !rive_file->is_connected("changed", changed_callback)) {
                rive_file->connect("changed", changed_callback);
            }
            notify_property_list_changed();
            _invalidate_rive();
        }
    }
    if (ObjectDB::get_instance(self_id) != this) return;
    update_configuration_warnings();
}

Ref<RiveFile> RiveControl::get_rive_file() const
{
    return rive_file;
}

void RiveControl::_on_rive_file_changed() {
    const ObjectID self_id(get_instance_id());
    if (is_inside_tree()) load_file();
    if (ObjectDB::get_instance(self_id) != this) return;
    _invalidate_rive();
}

void RiveControl::set_view_model_preset(const Ref<RiveViewModelPreset> &p_preset) {
    if (view_model_preset == p_preset) return;
    const Ref<RiveViewModelPreset> previous = view_model_preset;
    view_model_preset = p_preset;
    _update_preset_connections();
    const ObjectID self_id(get_instance_id());
    if (is_inside_tree() && rive_file.is_valid() && load_file() != OK) {
        if (ObjectDB::get_instance(self_id) != this) return;
        view_model_preset = previous;
        _update_preset_connections();
    }
    if (ObjectDB::get_instance(self_id) != this) return;
    update_configuration_warnings();
}

Ref<RiveViewModelPreset> RiveControl::get_view_model_preset() const { return view_model_preset; }

void RiveControl::_on_preset_changed() {
    _update_preset_connections();
    const ObjectID self_id(get_instance_id());
    if (is_inside_tree() && rive_file.is_valid()) load_file();
    if (ObjectDB::get_instance(self_id) != this) return;
    _invalidate_rive();
}

void RiveControl::_disconnect_preset_connections() {
    const Callable callback(this, "_on_preset_changed");
    for (const Ref<RiveViewModelPreset> &preset : preset_subscriptions) {
        if (preset.is_valid() && preset->is_connected("changed", callback)) preset->disconnect("changed", callback);
    }
    preset_subscriptions.clear();
}

void RiveControl::_update_preset_connections() {
    std::vector<Ref<RiveViewModelPreset>> next;
    int count = 0;
    if (view_model_preset.is_valid()) collect_presets(view_model_preset, next, 0, count);
    collect_presets(property_values, next, 0, count);
    const Callable callback(this, "_on_preset_changed");
    for (const Ref<RiveViewModelPreset> &preset : preset_subscriptions) {
        if (std::find(next.begin(), next.end(), preset) == next.end() && preset->is_connected("changed", callback)) {
            preset->disconnect("changed", callback);
        }
    }
    for (const Ref<RiveViewModelPreset> &preset : next) {
        if (!preset->is_connected("changed", callback)) preset->connect("changed", callback);
    }
    preset_subscriptions = std::move(next);
}

Error RiveControl::_fail_load(Error p_error, const String &p_message)
{
    load_error = p_message;
    update_configuration_warnings();
    emit_signal("load_failed", p_error, p_message);
    UtilityFunctions::printerr("RiveControl: ", p_message);
    return p_error;
}

Error RiveControl::load_file()
{
    if (rive_file.is_null()) {
        return _fail_load(ERR_UNCONFIGURED, "No RiveFile is assigned.");
    }
    if (rive_player.is_null()) {
        return _fail_load(ERR_UNAVAILABLE, "The Rive player is unavailable.");
    }

    Error file_error = rive_file->load_rive_file();
    if (file_error != OK) {
        return _fail_load(file_error, "Failed to parse the assigned RiveFile.");
    }

    rive::File *file = rive_file->get_rive_file();
    std::unique_ptr<rive::ArtboardInstance> artboard;

    if (file) {
        artboard = artboard_name.is_empty()
            ? file->artboardDefault()
            : file->artboardNamed(artboard_name.utf8().get_data());

        if (!artboard && !artboard_name.is_empty()) {
            return _fail_load(ERR_DOES_NOT_EXIST, "Artboard '" + artboard_name + "' was not found.");
        }
        if (!artboard) {
            return _fail_load(ERR_INVALID_DATA, "The Rive file does not contain an artboard.");
        }

    } else {
        if (!artboard_name.is_empty()) {
            return _fail_load(ERR_DOES_NOT_EXIST, "SVG resources expose only their default artboard.");
        }

        artboard = rive_file->instantiate_artboard();
        if (!artboard) {
            return _fail_load(ERR_INVALID_DATA, "Failed to instantiate the SVG artboard.");
        }
    }
    const ObjectID self_id(get_instance_id());
    if (!animation_name.is_empty() && !artboard->animation(animation_name.utf8().get_data())) {
        return _fail_load(ERR_DOES_NOT_EXIST, "Animation '" + animation_name + "' was not found.");
    }
    if (!state_machine_name.is_empty() && animation_name.is_empty() && !artboard->stateMachine(state_machine_name.utf8().get_data())) {
        return _fail_load(ERR_DOES_NOT_EXIST, "State machine '" + state_machine_name + "' was not found.");
    }
    Ref<RiveViewModelInstance> preset_vm;
    if (view_model_preset.is_valid()) {
        preset_vm = view_model_preset->instantiate(rive_file, artboard_name);
        if (preset_vm.is_null() || preset_vm->_get_native_file().get() != file || !preset_vm->_get_native_instance()) {
            return _fail_load(ERR_INVALID_DATA, "The ViewModel preset is incompatible with the selected Rive file/artboard.");
        }
        auto expected_vm = file ? file->createViewModelInstance(artboard.get()) : nullptr;
        if (expected_vm && expected_vm->viewModelId() != preset_vm->_get_native_instance()->viewModelId()) {
            return _fail_load(ERR_INVALID_DATA, "The ViewModel preset definition does not match the selected artboard.");
        }
    }
    rive::rcp<rive::File> retained_file;
    if (file) { file->ref(); retained_file = rive::rcp<rive::File>(file); }
    Ref<RivePlayer> player;
    player.instantiate();
    player->configure_playback(animation_name, state_machine_name);
    player->set_artboard(std::move(artboard), retained_file);
    if (preset_vm.is_valid() && !player->bind_view_model(preset_vm)) {
        return _fail_load(ERR_INVALID_DATA, "Failed to bind the ViewModel preset.");
    }
    if (!apply_list_presets(player->get_rive_view_model_instance(), property_values, rive_file)) {
        return _fail_load(ERR_INVALID_DATA, "A saved ViewModel list contains invalid item presets or an incompatible property path.");
    }
    loading = true;
    _disconnect_player_signals(rive_player);
    rive_player = player;
    _connect_player_signals(rive_player);
    intrinsic_size = Vector2(rive_player->get_artboard()->originalWidth(), rive_player->get_artboard()->originalHeight());
    property_defaults.clear();
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_valid()) {
        std::vector<rive::ViewModelInstance *> ancestors;
        int count = 0;
        collect_properties(vm, vm->_get_native_instance().get(), "", nullptr, &property_defaults, ancestors, count);
    }
    _apply_property_values();
    if (ObjectDB::get_instance(self_id) != this) return OK;
    _update_layout();
    player->advance(0.0f);
    if (ObjectDB::get_instance(self_id) != this) return OK;
    load_error = "";
    _invalidate_rive();
    update_minimum_size();
    update_configuration_warnings();
    _refresh_property_schema();
    notify_property_list_changed();
    loading = false;
    const Ref<RiveViewModelInstance> current_vm = get_view_model_instance();
    reported_view_model_id = current_vm.is_valid() ? ObjectID(current_vm->get_instance_id()) : ObjectID();
    emit_signal("view_model_changed");
    if (ObjectDB::get_instance(self_id) != this) return OK;
    emit_signal("loaded");
    UtilityFunctions::print_verbose("RiveControl: Rive file loaded successfully.");
    return OK;
}

void RiveControl::set_artboard_name(const String &p_name)
{
    if (artboard_name == p_name) {
        return;
    }
    const String previous_name = artboard_name;
    artboard_name = p_name;
    const ObjectID self_id(get_instance_id());
    if (is_inside_tree() && rive_file.is_valid()) {
        if (load_file() != OK) {
            if (ObjectDB::get_instance(self_id) != this) return;
            artboard_name = previous_name;
        }
    }
}

String RiveControl::get_artboard_name() const
{
    return artboard_name;
}

PackedStringArray RiveControl::get_artboard_list() const
{
    PackedStringArray artboards;
    if (rive_file.is_null()) {
        return artboards;
    }

    rive::File *file = rive_file->get_rive_file();
    if (!file) {
        return artboards;
    }

    for (size_t i = 0; i < file->artboardCount(); ++i) {
        artboards.push_back(String::utf8(file->artboardNameAt(i).c_str()));
    }
    return artboards;
}

void RiveControl::set_property_values(const Dictionary &p_values) {
    const ObjectID self_id(get_instance_id());
    Dictionary normalized_values;
    const Array keys = p_values.keys();
    for (int i = 0; i < keys.size(); ++i) {
        const Variant key = keys[i];
        if ((key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) || String(key).is_empty()) {
            WARN_PRINT("RiveControl override keys must be nonempty property path strings.");
            return;
        }
        if (!is_persistent_override(p_values[keys[i]])) {
            WARN_PRINT("RiveControl overrides require scene-serializable values. Use item presets for saved lists and the ViewModel API for runtime handles.");
            return;
        }
        const String path = RiveViewModelInstance::canonicalize_property_path(String(key));
        if (path.is_empty()) {
            WARN_PRINT("RiveControl override paths must have valid nonempty segments and list indexes.");
            return;
        }
        if (normalized_values.has(path) && Variant(normalized_values[path]) != Variant(p_values[key])) {
            WARN_PRINT("Conflicting RiveControl override aliases: the dictionary was not changed.");
            return;
        }
        normalized_values[path] = p_values[key];
    }
    if (rive_player.is_valid() && rive_player->get_artboard()) {
        bool has_lists = false;
        for (int i = 0; i < keys.size(); ++i) has_lists |= p_values[keys[i]].get_type() == Variant::ARRAY;
        if (has_lists) {
            const auto candidate = view_model_preset.is_valid() ? view_model_preset->instantiate(rive_file, artboard_name) :
                    rive_file->instantiate_view_model_for_artboard(artboard_name);
            if (!apply_list_presets(candidate, normalized_values, rive_file)) {
                WARN_PRINT("Invalid saved list presets: overrides were not changed.");
                return;
            }
        }
    }
    const Dictionary previous_values = property_values;
    bool removed_override = false;
    const Array previous_keys = previous_values.keys();
    for (int i = 0; i < previous_keys.size(); ++i) {
        if (!normalized_values.has(previous_keys[i])) {
            removed_override = true;
            break;
        }
    }
    rive_integration::assign_deep_dictionary_copy(property_values, normalized_values);
    _update_preset_connections();
    if (removed_override && rive_file.is_valid() && rive_player.is_valid() && rive_player->get_artboard()) {
        if (load_file() != OK) {
            if (ObjectDB::get_instance(self_id) != this) return;
            property_values = previous_values;
            _update_preset_connections();
        }
    } else {
        _apply_property_values();
    }
    if (ObjectDB::get_instance(self_id) != this) return;
    emit_signal("property_values_changed");
    if (ObjectDB::get_instance(self_id) != this) return;
    _invalidate_rive();
    _refresh_property_schema();
}

Dictionary RiveControl::get_property_values() const {
    return property_values.duplicate(true);
}

void RiveControl::_apply_property_values() {
    if (!rive_player.is_valid()) return;
    const ObjectID self_id(get_instance_id());

    PackedStringArray parent_first_paths;
    const Array keys = property_values.keys();
    for (int i = 0; i < keys.size(); ++i) {
        const Variant key = keys[i];
        if (key.get_type() == Variant::STRING || key.get_type() == Variant::STRING_NAME) {
            parent_first_paths.push_back(String(key));
        }
    }
    parent_first_paths.sort();
    for (int i = 0; i < parent_first_paths.size(); ++i) {
        _apply_property_value(parent_first_paths[i], property_values[parent_first_paths[i]], true);
        if (ObjectDB::get_instance(self_id) != this) return;
    }
}

void RiveControl::draw(rive::Renderer *renderer)
{
    if (rive_player.is_valid())
    {
        rive_player->draw(renderer, _get_rive_transform());
    }
}

bool RiveControl::_has_point(const Vector2 &p_point) const
{
    return Rect2(Vector2(), get_size()).has_point(p_point);
}

rive::Mat2D RiveControl::_get_rive_transform() const
{
    if (!rive_player.is_valid() || !rive_player->get_artboard())
        return rive::Mat2D();

    Size2i size = get_size();
    static const rive::Fit rive_fits[] = {
        rive::Fit::fill,
        rive::Fit::contain,
        rive::Fit::cover,
        rive::Fit::fitWidth,
        rive::Fit::fitHeight,
        rive::Fit::none,
        rive::Fit::scaleDown,
        rive::Fit::layout,
    };
    static const rive::Alignment rive_alignments[] = {
        rive::Alignment::topLeft,
        rive::Alignment::topCenter,
        rive::Alignment::topRight,
        rive::Alignment::centerLeft,
        rive::Alignment::center,
        rive::Alignment::centerRight,
        rive::Alignment::bottomLeft,
        rive::Alignment::bottomCenter,
        rive::Alignment::bottomRight,
    };

    return rive::computeAlignment(
        rive_fits[fit],
        rive_alignments[alignment],
        rive::AABB(0, 0, size.width, size.height),
        rive_player->get_artboard()->bounds(), fit == FIT_LAYOUT ? layout_scale : 1.0f);
}

void RiveControl::set_fit(Fit p_fit)
{
    ERR_FAIL_INDEX(p_fit, FIT_LAYOUT + 1);
    if (fit == p_fit) {
        return;
    }
    fit = p_fit;
    _update_layout();
    _invalidate_rive();
}

RiveControl::Fit RiveControl::get_fit() const
{
    return fit;
}

void RiveControl::set_alignment(Alignment p_alignment)
{
    ERR_FAIL_INDEX(p_alignment, ALIGNMENT_BOTTOM_RIGHT + 1);
    if (alignment == p_alignment) {
        return;
    }
    alignment = p_alignment;
    _invalidate_rive();
}

RiveControl::Alignment RiveControl::get_alignment() const
{
    return alignment;
}

void RiveControl::set_expand_mode(ExpandMode p_mode) {
    ERR_FAIL_INDEX(p_mode, EXPAND_KEEP_SIZE + 1);
    if (expand_mode == p_mode) return;
    expand_mode = p_mode;
    update_minimum_size();
}
RiveControl::ExpandMode RiveControl::get_expand_mode() const { return expand_mode; }
Vector2 RiveControl::get_intrinsic_size() const { return intrinsic_size; }
Vector2 RiveControl::get_layout_size() const {
    return rive_player.is_valid() && rive_player->get_artboard() ?
            Vector2(rive_player->get_artboard()->width(), rive_player->get_artboard()->height()) : Vector2();
}
Vector2 RiveControl::_get_minimum_size() const { return expand_mode == EXPAND_KEEP_SIZE ? intrinsic_size : Vector2(); }
void RiveControl::set_layout_scale(double p_scale) {
    ERR_FAIL_COND(!std::isfinite(p_scale) || p_scale <= 0.0);
    if (layout_scale == p_scale) return;
    layout_scale = p_scale;
    _update_layout();
    _invalidate_rive();
}
double RiveControl::get_layout_scale() const { return layout_scale; }

PackedStringArray RiveControl::_get_configuration_warnings() const {
    PackedStringArray warnings;
    if (rive_file.is_null()) warnings.push_back("Assign a RiveFile to display a Rive artboard.");
    if (!load_error.is_empty()) warnings.push_back(load_error);
    if (rive_file.is_valid() && (get_size().x <= 0.0 || get_size().y <= 0.0) && expand_mode == EXPAND_IGNORE_SIZE) {
        warnings.push_back("RiveControl has zero size. Set a size, use a Container with Expand/Fill, or enable Keep Size.");
    }
    return warnings;
}

void RiveControl::set_playing(bool p_playing) {
    if (playing == p_playing) return;
    playing = p_playing;
    _invalidate_rive();
}
bool RiveControl::is_playing() const { return playing; }
void RiveControl::set_speed_scale(double p_scale) {
    ERR_FAIL_COND(!std::isfinite(p_scale) || p_scale < 0.0);
    speed_scale = p_scale;
    _invalidate_rive();
}
double RiveControl::get_speed_scale() const { return speed_scale; }
void RiveControl::set_update_mode(UpdateMode p_mode) {
    ERR_FAIL_INDEX(p_mode, UPDATE_MANUAL + 1);
    update_mode = p_mode;
    _invalidate_rive();
}
RiveControl::UpdateMode RiveControl::get_update_mode() const { return update_mode; }
void RiveControl::play() { set_playing(true); }
void RiveControl::pause() { set_playing(false); }
void RiveControl::stop() { set_playing(false); reset(); }
void RiveControl::reset() {
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (rive_player.is_valid()) {
        player->reset();
        if (ObjectDB::get_instance(self_id) != this) return;
        _apply_property_values();
        if (ObjectDB::get_instance(self_id) != this) return;
        _update_layout();
    }
    _invalidate_rive();
}
void RiveControl::advance(double p_delta) {
    ERR_FAIL_COND(!std::isfinite(p_delta) || p_delta < 0.0);
    if (rive_player.is_null()) return;
    const Ref<RivePlayer> player = rive_player;
    const ObjectID self_id(get_instance_id());
    const bool active = player->advance(p_delta * speed_scale);
    if (ObjectDB::get_instance(self_id) != this) return;
    needs_advance = active;
    player->flush_view_model_changes();
    if (ObjectDB::get_instance(self_id) != this) return;
    render_dirty = true;
    if (is_inside_tree()) _render_rive();
    queue_redraw();
}
bool RiveControl::seek(double p_time) {
    const Ref<RivePlayer> player = rive_player;
    const ObjectID self_id(get_instance_id());
    if (!std::isfinite(p_time) || p_time < 0.0 || player.is_null() || !player->seek(p_time)) return false;
    if (ObjectDB::get_instance(self_id) != this) return true;
    _invalidate_rive();
    return true;
}
double RiveControl::get_time() const { return rive_player.is_valid() ? rive_player->get_time() : 0.0; }
double RiveControl::get_duration() const { return rive_player.is_valid() ? rive_player->get_duration() : 0.0; }

void RiveControl::_gui_input(const Ref<InputEvent> &p_event)
{
    if (rive_player.is_null() || p_event.is_null()) return;
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    const rive::Mat2D transform = _get_rive_transform();
    bool handled = false;
    Ref<InputEventMouse> mouse_event = p_event;
    if (mouse_event.is_valid() && mouse_event->get_device() == InputEvent::DEVICE_ID_EMULATION) return;
    if (mouse_event.is_valid())
    {
        Vector2 local_pos = mouse_event->get_position();
        Ref<InputEventMouseMotion> motion = p_event;
        if (motion.is_valid()) handled = player->pointer_move(local_pos, transform);
        if (ObjectDB::get_instance(self_id) != this) return;
        Ref<InputEventMouseButton> button = p_event;
        if (button.is_valid())
        {
            if (button->get_button_index() == MOUSE_BUTTON_LEFT || button->get_button_index() == MOUSE_BUTTON_RIGHT || button->get_button_index() == MOUSE_BUTTON_MIDDLE)
            {
                const int native_button = button->get_button_index() == MOUSE_BUTTON_LEFT ? 0 : button->get_button_index() == MOUSE_BUTTON_RIGHT ? 1 : 2;
                handled = button->is_pressed() ? player->pointer_down(local_pos, transform, 0, native_button)
                        : player->pointer_up(local_pos, transform, 0, native_button);
                if (ObjectDB::get_instance(self_id) != this) return;
                if (handled && button->is_pressed() && get_focus_mode() != FOCUS_NONE) {
                    grab_focus();
                    if (ObjectDB::get_instance(self_id) != this) return;
                }
            } else if (button->is_pressed()) {
                Vector2 scroll_delta;
                switch (button->get_button_index()) {
                    case MOUSE_BUTTON_WHEEL_UP: scroll_delta.y = -1.0; break;
                    case MOUSE_BUTTON_WHEEL_DOWN: scroll_delta.y = 1.0; break;
                    case MOUSE_BUTTON_WHEEL_LEFT: scroll_delta.x = -1.0; break;
                    case MOUSE_BUTTON_WHEEL_RIGHT: scroll_delta.x = 1.0; break;
                    default: break;
                }
                if (scroll_delta != Vector2()) handled = player->scroll(local_pos, scroll_delta * button->get_factor(), transform);
                if (ObjectDB::get_instance(self_id) != this) return;
            }
        }
    }
    Ref<InputEventScreenTouch> touch = p_event;
    if (touch.is_valid()) {
        const int pointer_id = touch->get_index() + 1;
        if (touch->is_canceled()) handled = player->pointer_exit(pointer_id);
        else if (touch->is_pressed()) {
            handled = player->pointer_down(touch->get_position(), transform, pointer_id);
            if (ObjectDB::get_instance(self_id) != this) return;
            if (handled && get_focus_mode() != FOCUS_NONE) grab_focus();
            if (ObjectDB::get_instance(self_id) != this) return;
        } else {
            handled = player->pointer_up(touch->get_position(), transform, pointer_id);
            if (ObjectDB::get_instance(self_id) != this) return;
            player->pointer_exit(pointer_id);
        }
        if (ObjectDB::get_instance(self_id) != this) return;
    }
    Ref<InputEventScreenDrag> drag = p_event;
    if (drag.is_valid()) handled = player->pointer_move(drag->get_position(), transform, drag->get_index() + 1);
    if (ObjectDB::get_instance(self_id) != this) return;
    Ref<InputEventKey> key = p_event;
    Ref<InputEventJoypadButton> joy_button = p_event;
    Ref<InputEventJoypadMotion> joy_motion = p_event;
    if (has_focus() && (joy_button.is_valid() || joy_motion.is_valid())) handled = player->gamepad_input(p_event);
    if (ObjectDB::get_instance(self_id) != this) return;
    Ref<InputEventPanGesture> pan = p_event;
    if (pan.is_valid()) {
        rive::Mat2D inverse;
        if (transform.invert(&inverse)) {
            const Vector2 pos = pan->get_position();
            const Vector2 delta = pan->get_delta();
            const auto native_pos = inverse * rive::Vec2D(pos.x, pos.y);
            const auto native_end = inverse * rive::Vec2D(pos.x + delta.x, pos.y + delta.y);
            handled = player->send_scroll(Vector2(native_pos.x, native_pos.y), Vector2(native_end.x - native_pos.x, native_end.y - native_pos.y), 1, true);
        }
    }
    if (ObjectDB::get_instance(self_id) != this) return;
    if (key.is_valid() && has_focus()) {
        const bool shortcut = key->is_pressed() && (key->is_ctrl_pressed() || key->is_meta_pressed());
        const String selected_text = shortcut && (key->get_keycode() == KEY_C || key->get_keycode() == KEY_X) ? player->get_selected_text() : String();
        handled = player->key_input(key);
        if (ObjectDB::get_instance(self_id) != this) return;
        if (!selected_text.is_empty() && DisplayServer::get_singleton()) {
            DisplayServer::get_singleton()->clipboard_set(selected_text);
            handled = true;
        }
        if (!handled && key->is_pressed()) {
            if (key->get_keycode() == KEY_TAB) handled = key->is_shift_pressed() ? player->focus_previous() : player->focus_next();
            else if (key->get_keycode() == KEY_LEFT) handled = player->focus_direction(0);
            else if (key->get_keycode() == KEY_RIGHT) handled = player->focus_direction(1);
            else if (key->get_keycode() == KEY_UP) handled = player->focus_direction(2);
            else if (key->get_keycode() == KEY_DOWN) handled = player->focus_direction(3);
            if (ObjectDB::get_instance(self_id) != this) return;
        }
        if (key->is_pressed() && key->get_unicode() >= 32 && !key->is_ctrl_pressed() && !key->is_meta_pressed()) {
            handled = player->text_input(String::chr(key->get_unicode())) || handled;
        } else if (key->is_pressed() && (key->is_ctrl_pressed() || key->is_meta_pressed()) && key->get_keycode() == KEY_V) {
            DisplayServer *display = DisplayServer::get_singleton();
            if (display) handled = player->text_input(display->clipboard_get()) || handled;
        }
        if (ObjectDB::get_instance(self_id) != this) return;
    }
    _invalidate_rive();
    _sync_keyboard();
    if (handled) accept_event();
}

void RiveControl::_on_mouse_exited() {
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid()) player->pointer_exit();
    if (ObjectDB::get_instance(self_id) != this) return;
    _invalidate_rive();
}

void RiveControl::_on_joy_connection_changed(int p_device, bool p_connected) {
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid() && (!p_connected || has_focus())) player->set_gamepad_connected(p_device, p_connected);
}

void RiveControl::play_animation(const String &p_name)
{
    const ObjectID self_id(get_instance_id());
    set_animation_name(p_name);
    if (ObjectDB::get_instance(self_id) != this) return;
    set_playing(true);
}

void RiveControl::play_state_machine(const String &p_name)
{
    const ObjectID self_id(get_instance_id());
    set_state_machine_name(p_name);
    if (ObjectDB::get_instance(self_id) != this) return;
    set_playing(true);
}

PackedStringArray RiveControl::get_animation_list() const
{
    if (rive_player.is_valid()) return rive_player->get_animation_list();
    return PackedStringArray();
}

PackedStringArray RiveControl::get_state_machine_list() const
{
    if (rive_player.is_valid()) return rive_player->get_state_machine_list();
    return PackedStringArray();
}

void RiveControl::_validate_property(PropertyInfo &p_property) const
{
    if (p_property.name == StringName("artboard_name"))
    {
        PackedStringArray artboards = get_artboard_list();
        String hint_string;
        for (int i = 0; i < artboards.size(); i++)
        {
            if (i > 0)
                hint_string += ",";
            hint_string += artboards[i];
        }
        p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
        p_property.hint_string = hint_string;
    }
    else if (p_property.name == StringName("animation_name"))
    {
        PackedStringArray animations = get_animation_list();
        String hint_string;
        for (int i = 0; i < animations.size(); i++)
        {
            if (i > 0)
                hint_string += ",";
            hint_string += animations[i];
        }
        p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
        p_property.hint_string = hint_string;
    }
    else if (p_property.name == StringName("state_machine_name"))
    {
        PackedStringArray state_machines = get_state_machine_list();
        String hint_string;
        for (int i = 0; i < state_machines.size(); i++)
        {
            if (i > 0)
                hint_string += ",";
            hint_string += state_machines[i];
        }
        p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
        p_property.hint_string = hint_string;
    }
}

void RiveControl::set_animation_name(const String &p_name)
{
    if (animation_name == p_name) return;
    if (!p_name.is_empty() && rive_player.is_valid() && rive_player->get_artboard() && !get_animation_list().has(p_name)) {
        _fail_load(ERR_DOES_NOT_EXIST, "Animation '" + p_name + "' was not found.");
        return;
    }
    animation_name = p_name;
    if (!p_name.is_empty()) state_machine_name = "";
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid()) player->configure_playback(animation_name, state_machine_name);
    if (ObjectDB::get_instance(self_id) != this) return;
    if (is_inside_tree()) _sync_keyboard();
    _invalidate_rive();
    update_configuration_warnings();
}

String RiveControl::get_animation_name() const
{
    return animation_name;
}

void RiveControl::set_state_machine_name(const String &p_name)
{
    if (state_machine_name == p_name) return;
    if (!p_name.is_empty() && rive_player.is_valid() && rive_player->get_artboard() && !get_state_machine_list().has(p_name)) {
        _fail_load(ERR_DOES_NOT_EXIST, "State machine '" + p_name + "' was not found.");
        return;
    }
    state_machine_name = p_name;
    if (!p_name.is_empty()) animation_name = "";
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid()) player->configure_playback(animation_name, state_machine_name);
    if (ObjectDB::get_instance(self_id) != this) return;
    if (is_inside_tree()) _sync_keyboard();
    _invalidate_rive();
    update_configuration_warnings();
}

String RiveControl::get_state_machine_name() const
{
    return state_machine_name;
}

bool RiveControl::_apply_property_value(const String &p_path, const Variant &p_value, bool p_replay)
{
    const String path = RiveViewModelInstance::canonicalize_property_path(p_path);
    if (path.is_empty()) return false;
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_null()) return false;

    Ref<RiveViewModelTrigger> trigger = vm->get_trigger_property(path);
    if (trigger.is_valid()) {
        property_values.erase(path);
        if (p_replay) return true;
        if (p_value.get_type() != Variant::BOOL) return false;
        if (bool(p_value)) trigger->fire();
        return true;
    }

    if (p_value.get_type() == Variant::ARRAY) {
        Array items;
        return instantiate_list_items(p_value, rive_file, items) && vm->set_property_value(path, items);
    }
    return vm->set_property_value(path, p_value);
}

bool RiveControl::set_property_value(const String &p_property_path, const Variant &p_value)
{
    const String path = RiveViewModelInstance::canonicalize_property_path(p_property_path);
    if (path.is_empty() || !is_persistent_override(p_value)) return false;

    const ObjectID self_id(get_instance_id());
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_valid()) {
        if (!_apply_property_value(path, p_value)) return false;
        if (ObjectDB::get_instance(self_id) != this) return true;
        if (vm->get_trigger_property(path).is_null()) {
            property_values[path] = copy_override_containers(p_value);
        }
    } else {
        if (rive_player.is_valid() && rive_player->get_artboard()) return false;
        property_values[path] = copy_override_containers(p_value);
    }
    _invalidate_rive();
    emit_signal("property_values_changed");
    if (ObjectDB::get_instance(self_id) != this) return true;
    _update_preset_connections();
    _refresh_property_schema();
    return true;
}

Variant RiveControl::get_property_value(const String &p_property_path) const
{
    const String path = RiveViewModelInstance::canonicalize_property_path(p_property_path);
    if (path.is_empty()) return Variant();
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_valid() && vm->has_property(path)) return vm->get_property_value(path);
    return get_property_override(path);
}

Variant RiveControl::get_property_override(const String &p_property_path) const {
    const String path = RiveViewModelInstance::canonicalize_property_path(p_property_path);
    return property_values.has(path) ? copy_override_containers(property_values[path]) : Variant();
}
bool RiveControl::has_property_override(const String &p_property_path) const { return property_values.has(RiveViewModelInstance::canonicalize_property_path(p_property_path)); }
void RiveControl::clear_property_override(const String &p_property_path) {
    const String path = RiveViewModelInstance::canonicalize_property_path(p_property_path);
    if (!property_values.has(path)) return;
    Dictionary values = get_property_values();
    values.erase(path);
    set_property_values(values);
}

void RiveControl::set_text_value(const String &p_property_path, const String &p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_number_value(const String &p_property_path, float p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_boolean_value(const String &p_property_path, bool p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::fire_trigger(const String &p_property_path)
{
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_null()) return;
    Ref<RiveViewModelTrigger> trigger = vm->get_trigger_property(p_property_path);
    if (trigger.is_valid()) {
        trigger->fire();
        _invalidate_rive();
    }
}

void RiveControl::set_enum_value(const String &p_property_path, int p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_color_value(const String &p_property_path, Color p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_image_value(const String &p_property_path, const Ref<Texture2D> &p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_font_value(const String &p_property_path, const Ref<FontFile> &p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_artboard_value(const String &p_property_path, const String &p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::set_list_value(const String &p_property_path, const Array &p_value)
{
    set_property_value(p_property_path, p_value);
}

void RiveControl::_get_property_list(List<PropertyInfo> *p_list) const
{
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (vm.is_null()) return;
    std::vector<rive::ViewModelInstance *> ancestors;
    int count = 0;
    collect_properties(vm, vm->_get_native_instance().get(), "", p_list, nullptr, ancestors, count);
}

bool RiveControl::_property_can_revert(const StringName &p_name) const {
    const String name = p_name;
    return name.begins_with(RiveConstants::PROPERTY_PREFIX) &&
            property_defaults.has(RiveViewModelInstance::canonicalize_property_path(name.substr(String(RiveConstants::PROPERTY_PREFIX).length())));
}
bool RiveControl::_property_get_revert(const StringName &p_name, Variant &r_ret) const {
    if (!_property_can_revert(p_name)) return false;
    r_ret = property_defaults[RiveViewModelInstance::canonicalize_property_path(String(p_name).substr(String(RiveConstants::PROPERTY_PREFIX).length()))];
    return true;
}

bool RiveControl::_get(const StringName &p_name, Variant &r_ret) const
{
    const String name = p_name;
    if (!name.begins_with(RiveConstants::PROPERTY_PREFIX)) return false;

    const String path = RiveViewModelInstance::canonicalize_property_path(name.substr(String(RiveConstants::PROPERTY_PREFIX).length()));
    Ref<RiveViewModelInstance> vm = get_view_model_instance();
    if (!property_values.has(path) && (vm.is_null() || !vm->has_property(path))) return false;
    r_ret = get_property_value(path);
    return true;
}

bool RiveControl::_set(const StringName &p_name, const Variant &p_value)
{
    const String name = p_name;
    if (!name.begins_with(RiveConstants::PROPERTY_PREFIX)) return false;
    const String path = RiveViewModelInstance::canonicalize_property_path(name.substr(String(RiveConstants::PROPERTY_PREFIX).length()));
    if (property_defaults.has(path) && Variant(property_defaults[path]) == p_value) {
        if (p_value.get_type() == Variant::OBJECT || p_value.get_type() == Variant::NIL) {
            clear_property_override(path);
        } else {
            const ObjectID self_id(get_instance_id());
            if (!_apply_property_value(path, p_value)) return false;
            if (ObjectDB::get_instance(self_id) != this) return true;
            property_values.erase(path);
            _invalidate_rive();
            emit_signal("property_values_changed");
        }
        return true;
    }
    return set_property_value(path, p_value);
}

Ref<RiveViewModelInstance> RiveControl::get_view_model_instance() const {
    if (rive_player.is_valid()) {
        return rive_player->get_rive_view_model_instance();
    }
    return nullptr;
}

bool RiveControl::bind_view_model(const Ref<RiveViewModelInstance> &p_instance) {
    const Ref<RivePlayer> player = rive_player;
    return player.is_valid() && player->bind_view_model(p_instance);
}

bool RiveControl::set_input_value(const String &p_name, const Variant &p_value, const String &p_path) {
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_null() || !player->set_input_value(p_name, p_value, p_path)) return false;
    if (ObjectDB::get_instance(self_id) != this) return true;
    _invalidate_rive();
    return true;
}
Variant RiveControl::get_input_value(const String &p_name, const String &p_path) const { return rive_player.is_valid() ? rive_player->get_input_value(p_name, p_path) : Variant(); }
void RiveControl::fire_input(const String &p_name, const String &p_path) {
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid()) player->fire_input(p_name, p_path);
    if (ObjectDB::get_instance(self_id) != this) return;
    _invalidate_rive();
}
Array RiveControl::get_input_list(const String &p_path) const { return rive_player.is_valid() ? rive_player->get_input_list(p_path) : Array(); }
bool RiveControl::set_text_run(const String &p_name, const String &p_value, const String &p_path) {
    const ObjectID self_id(get_instance_id());
    const Ref<RivePlayer> player = rive_player;
    if (player.is_null() || !player->set_text_run(p_name, p_value, p_path)) return false;
    if (ObjectDB::get_instance(self_id) != this) return true;
    _invalidate_rive();
    return true;
}
String RiveControl::get_text_run(const String &p_name, const String &p_path) const { return rive_player.is_valid() ? rive_player->get_text_run(p_name, p_path) : String(); }

void RiveControl::set_volume(double p_volume) {
    if (!std::isfinite(p_volume) || p_volume < 0.0 || p_volume > 1.0) return;
    volume = p_volume;
    const Ref<RivePlayer> player = rive_player;
    if (player.is_valid()) player->set_volume(volume);
}
void RiveControl::_on_player_event(const String &p_name, const Dictionary &p_payload) { emit_signal("rive_event", p_name, p_payload); }
void RiveControl::_on_player_animation_finished(const String &p_name) { emit_signal("animation_finished", p_name); }
void RiveControl::_on_player_animation_looped(const String &p_name) { emit_signal("animation_looped", p_name); }
void RiveControl::_on_player_state_changed(const String &p_name) { emit_signal("state_changed", p_name); }
