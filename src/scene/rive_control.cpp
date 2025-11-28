#include "rive_control.h"
#include "../renderer/rive_renderer.h"
#include "../rive_constants.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/error_macros.hpp>

#include "rive/layout.hpp"
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_enum.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_property_enum.hpp>
#include <rive/viewmodel/data_enum.hpp>
#include <rive/viewmodel/data_enum_value.hpp>

using namespace godot;

void RiveControl::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_file_path", "path"), &RiveControl::set_file_path);
    ClassDB::bind_method(D_METHOD("get_file_path"), &RiveControl::get_file_path);

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
    ClassDB::bind_method(D_METHOD("get_view_model_instance"), &RiveControl::get_view_model_instance);

    ClassDB::bind_method(D_METHOD("set_property_values", "values"), &RiveControl::set_property_values);
    ClassDB::bind_method(D_METHOD("get_property_values"), &RiveControl::get_property_values);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_path", PROPERTY_HINT_FILE, RiveConstants::EXTENSION), "set_file_path", "get_file_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "property_values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_property_values", "get_property_values");
}

RiveControl::RiveControl()
{
    set_mouse_filter(MOUSE_FILTER_STOP);
    rive_player.instantiate();
    texture_target.instantiate();
}

RiveControl::~RiveControl()
{
    if (texture_target.is_valid()) {
        texture_target->clear();
    }
}

void RiveControl::_notification(int p_what)
{
    switch (p_what)
    {
    case NOTIFICATION_ENTER_TREE:
        load_file();
        set_process(true);
        break;
    case NOTIFICATION_EXIT_TREE:
        break;
    case NOTIFICATION_RESIZED:
        break;
    case NOTIFICATION_DRAW:
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
        if (rive_player.is_valid())
        {
            float delta = get_process_delta_time();
            rive_player->advance(delta);
            _render_rive();
            queue_redraw();
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

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice *rd = rs->get_rendering_device();

    rive_integration::render_texture(rd, texture_target->get_texture_rid(), this, size.width, size.height);
}

void RiveControl::set_file_path(const String &p_path)
{
    file_path = p_path;
    if (is_inside_tree())
    {
        load_file();
    }
}

String RiveControl::get_file_path() const
{
    return file_path;
}

void RiveControl::load_file()
{
    if (file_path.is_empty())
        return;

    Ref<FileAccess> f = FileAccess::open(file_path, FileAccess::READ);
    if (f.is_null())
    {
        ERR_PRINT("Failed to open Rive file: " + file_path);
        return;
    }

    uint64_t len = f->get_length();
    PackedByteArray data = f->get_buffer(len);

    if (rive_player.is_valid()) {
        if (rive_player->load_from_bytes(data)) {
            _apply_property_values();
            notify_property_list_changed();
            UtilityFunctions::print_verbose("Rive file loaded successfully: " + file_path);
        } else {
            ERR_PRINT("Failed to import Rive file: " + file_path);
        }
    }
}

void RiveControl::set_property_values(const Dictionary &p_values) {
    property_values = p_values;
    _apply_property_values();
}

Dictionary RiveControl::get_property_values() const {
    return property_values;
}

void RiveControl::_apply_property_values() {
    if (!rive_player.is_valid()) return;
    
    Array keys = property_values.keys();
    for (int i = 0; i < keys.size(); i++) {
        String path = keys[i];
        Variant value = property_values[path];
        
        if (value.get_type() == Variant::FLOAT) set_number_value(path, value);
        else if (value.get_type() == Variant::STRING) set_text_value(path, value);
        else if (value.get_type() == Variant::BOOL) set_boolean_value(path, value);
        else if (value.get_type() == Variant::INT) set_enum_value(path, value);
        else if (value.get_type() == Variant::COLOR) set_color_value(path, value);
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
    if (!rive_player.is_valid()) return false;
    return rive_player->hit_test(p_point, _get_rive_transform());
}

rive::Mat2D RiveControl::_get_rive_transform() const
{
    if (!rive_player.is_valid() || !rive_player->get_artboard())
        return rive::Mat2D();

    Size2i size = get_size();
    return rive::computeAlignment(
        rive::Fit::contain,
        rive::Alignment::center,
        rive::AABB(0, 0, size.width, size.height),
        rive_player->get_artboard()->bounds());
}

void RiveControl::_gui_input(const Ref<InputEvent> &p_event)
{
    if (!rive_player.is_valid()) return;

    Ref<InputEventMouse> mouse_event = p_event;
    if (mouse_event.is_valid())
    {
        Vector2 local_pos = mouse_event->get_position();
        rive::Mat2D transform = _get_rive_transform();
        bool hit = false;

        Ref<InputEventMouseMotion> motion = p_event;
        if (motion.is_valid())
        {
            hit = rive_player->pointer_move(local_pos, transform);
        }

        Ref<InputEventMouseButton> button = p_event;
        if (button.is_valid())
        {
            if (button->get_button_index() == MOUSE_BUTTON_LEFT)
            {
                if (button->is_pressed())
                {
                    hit = rive_player->pointer_down(local_pos, transform);
                }
                else
                {
                    hit = rive_player->pointer_up(local_pos, transform);
                }
            }
        }

        if (hit)
        {
            accept_event();
        }
    }
}

void RiveControl::play_animation(const String &p_name)
{
    if (rive_player.is_valid()) rive_player->play_animation(p_name);
}

void RiveControl::play_state_machine(const String &p_name)
{
    if (rive_player.is_valid()) rive_player->play_state_machine(p_name);
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
    if (p_property.name == StringName("animation_name"))
    {
        PackedStringArray animations = get_animation_list();
        String hint_string;
        for (int i = 0; i < animations.size(); i++)
        {
            if (i > 0)
                hint_string += ",";
            hint_string += animations[i];
        }
        p_property.hint = PROPERTY_HINT_ENUM;
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
        p_property.hint = PROPERTY_HINT_ENUM;
        p_property.hint_string = hint_string;
    }
}

void RiveControl::set_animation_name(const String &p_name)
{
    if (rive_player.is_valid()) rive_player->play_animation(p_name);
}

String RiveControl::get_animation_name() const
{
    if (rive_player.is_valid()) return rive_player->get_animation_name();
    return "";
}

void RiveControl::set_state_machine_name(const String &p_name)
{
    if (rive_player.is_valid()) rive_player->play_state_machine(p_name);
}

String RiveControl::get_state_machine_name() const
{
    if (rive_player.is_valid()) return rive_player->get_state_machine_name();
    return "";
}

static rive::ViewModelInstance *resolve_view_model_instance(rive::ViewModelInstance *root, const String &path, String &out_property_name)
{
    if (!root)
        return nullptr;

    PackedStringArray parts = path.split(".");
    rive::ViewModelInstance *current = root;

    for (int i = 0; i < parts.size() - 1; ++i)
    {
        rive::ViewModelInstanceValue *prop = current->propertyValue(parts[i].utf8().get_data());
        if (!prop)
            return nullptr;

        rive::ViewModelInstanceViewModel *vm_prop = prop->as<rive::ViewModelInstanceViewModel>();
        if (!vm_prop)
            return nullptr;

        current = vm_prop->referenceViewModelInstance().get();
        if (!current)
            return nullptr;
    }

    out_property_name = parts[parts.size() - 1];
    return current;
}

void RiveControl::set_text_value(const String &p_property_path, const String &p_value)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceString *str_prop = prop->as<rive::ViewModelInstanceString>())
            {
                str_prop->propertyValue(std::string(p_value.utf8().get_data()));
            }
        }
    }
}

void RiveControl::set_number_value(const String &p_property_path, float p_value)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceNumber *num_prop = prop->as<rive::ViewModelInstanceNumber>())
            {
                num_prop->propertyValue(p_value);
            }
        }
    }
}

void RiveControl::set_boolean_value(const String &p_property_path, bool p_value)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceBoolean *bool_prop = prop->as<rive::ViewModelInstanceBoolean>())
            {
                bool_prop->propertyValue(p_value);
            }
        }
    }
}

void RiveControl::fire_trigger(const String &p_property_path)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceTrigger *trigger_prop = prop->as<rive::ViewModelInstanceTrigger>())
            {
                trigger_prop->trigger();
            }
        }
    }
}

void RiveControl::_get_property_list(List<PropertyInfo> *p_list) const
{
    // Dynamic properties are now handled by RiveInspectorPlugin
}

bool RiveControl::_get(const StringName &p_name, Variant &r_ret) const
{
    String name = p_name;
    if (name.begins_with(RiveConstants::PROPERTY_PREFIX))
    {
        String path = name.substr(String(RiveConstants::PROPERTY_PREFIX).length());
        
        // Check overrides first
        if (property_values.has(path)) {
            r_ret = property_values[path];
            return true;
        }

        if (!rive_player.is_valid()) return false;
        rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
        if (!vm) return false;

        String prop_name;
        rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, path, prop_name);

        if (target_vm)
        {
            rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
            if (prop)
            {
                if (rive::ViewModelInstanceNumber *num = prop->as<rive::ViewModelInstanceNumber>())
                {
                    r_ret = num->propertyValue();
                    return true;
                }
                if (rive::ViewModelInstanceString *str = prop->as<rive::ViewModelInstanceString>())
                {
                    r_ret = String(str->propertyValue().c_str());
                    return true;
                }
                if (rive::ViewModelInstanceBoolean *b = prop->as<rive::ViewModelInstanceBoolean>())
                {
                    r_ret = b->propertyValue();
                    return true;
                }
                if (prop->is<rive::ViewModelInstanceTrigger>())
                {
                    r_ret = false;
                    return true;
                }
                if (rive::ViewModelInstanceEnum *enm = prop->as<rive::ViewModelInstanceEnum>())
                {
                    r_ret = (int)enm->propertyValue();
                    return true;
                }
                if (rive::ViewModelInstanceColor *col = prop->as<rive::ViewModelInstanceColor>())
                {
                    uint32_t argb = col->propertyValue();
                    float a = ((argb >> 24) & 0xFF) / 255.0f;
                    float r = ((argb >> 16) & 0xFF) / 255.0f;
                    float g = ((argb >> 8) & 0xFF) / 255.0f;
                    float b = (argb & 0xFF) / 255.0f;
                    r_ret = Color(r, g, b, a);
                    return true;
                }
            }
        }
    }
    return false;
}

bool RiveControl::_set(const StringName &p_name, const Variant &p_value)
{
    String name = p_name;
    if (name.begins_with(RiveConstants::PROPERTY_PREFIX))
    {
        String path = name.substr(String(RiveConstants::PROPERTY_PREFIX).length());
        
        // Store in dictionary for persistence
        property_values[path] = p_value;
        
        // Apply immediately
        if (p_value.get_type() == Variant::FLOAT) set_number_value(path, p_value);
        else if (p_value.get_type() == Variant::STRING) set_text_value(path, p_value);
        else if (p_value.get_type() == Variant::BOOL) set_boolean_value(path, p_value);
        else if (p_value.get_type() == Variant::INT) set_enum_value(path, p_value);
        else if (p_value.get_type() == Variant::COLOR) set_color_value(path, p_value);
        
        return true;
    }
    return false;
}

void RiveControl::set_enum_value(const String &p_property_path, int p_value)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceEnum *enum_prop = prop->as<rive::ViewModelInstanceEnum>())
            {
                enum_prop->value((uint32_t)p_value);
            }
        }
    }
}

void RiveControl::set_color_value(const String &p_property_path, Color p_value)
{
    if (!rive_player.is_valid()) return;
    rive::ViewModelInstance *vm = rive_player->get_view_model_instance();
    if (!vm) return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(vm, p_property_path, prop_name);

    if (target_vm)
    {
        rive::ViewModelInstanceValue *prop = target_vm->propertyValue(prop_name.utf8().get_data());
        if (prop)
        {
            if (rive::ViewModelInstanceColor *color_prop = prop->as<rive::ViewModelInstanceColor>())
            {
                uint32_t a = (uint32_t)(p_value.a * 255.0f);
                uint32_t r = (uint32_t)(p_value.r * 255.0f);
                uint32_t g = (uint32_t)(p_value.g * 255.0f);
                uint32_t b = (uint32_t)(p_value.b * 255.0f);
                uint32_t argb = (a << 24) | (r << 16) | (g << 8) | b;
                color_prop->propertyValue(argb);
            }
        }
    }
}

Ref<RiveViewModelInstance> RiveControl::get_view_model_instance() const {
    if (rive_player.is_valid()) {
        return rive_player->get_rive_view_model_instance();
    }
    return nullptr;
}
