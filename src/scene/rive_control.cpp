#include "rive_control.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
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

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_path", PROPERTY_HINT_FILE, "*.riv"), "set_file_path", "get_file_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
}

RiveControl::RiveControl()
{
    set_mouse_filter(MOUSE_FILTER_STOP);
}

RiveControl::~RiveControl()
{
    if (texture_rid.is_valid())
    {
        RenderingServer *rs = RenderingServer::get_singleton();
        if (rs)
        {
            RenderingDevice *rd = rs->get_rendering_device();
            if (texture_rd_ref.is_valid())
            {
                texture_rd_ref->set_texture_rd_rid(RID());
                if (rd)
                {
                    rd->free_rid(texture_rid);
                }
            }
            else
            {
                rs->free_rid(texture_rid);
            }
        }
    }
    texture_rd_ref.unref();
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
        if (texture_rd_ref.is_valid())
        {
            draw_texture(texture_rd_ref, Point2());
        }
        else if (texture_rid.is_valid())
        {
            RenderingServer::get_singleton()->canvas_item_add_texture_rect(get_canvas_item(), Rect2(Point2(), get_size()), texture_rid);
        }
        break;
    case NOTIFICATION_PROCESS:
        if (artboard)
        {
            float delta = get_process_delta_time();
            if (state_machine)
            {
                state_machine->advance(delta);
            }
            else if (animation)
            {
                animation->advance(delta);
                animation->apply();
            }
            artboard->advance(delta);
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

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs)
        return;
    RenderingDevice *rd = rs->get_rendering_device();
    
    String driver = rs->get_current_rendering_driver_name();
    bool is_opengl = (driver == "opengl3");

    if (!rd && !is_opengl)
        return;

    if (texture_size != size)
    {
        if (texture_rid.is_valid())
        {
            if (texture_rd_ref.is_valid())
            {
                texture_rd_ref->set_texture_rd_rid(RID());
                if (rd)
                {
                    rd->free_rid(texture_rid);
                }
            }
            else
            {
                rs->free_rid(texture_rid);
            }
            texture_rid = RID();
        }

        if (rd)
        {
            Ref<RDTextureFormat> tf;
            tf.instantiate();
            tf->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
            tf->set_width(size.width);
            tf->set_height(size.height);
            tf->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT);

            Ref<RDTextureView> tv;
            tv.instantiate();

            texture_rid = rd->texture_create(tf, tv);
            
            if (texture_rd_ref.is_null())
            {
                texture_rd_ref.instantiate();
            }
            texture_rd_ref->set_texture_rd_rid(texture_rid);
        }
        else
        {
            Ref<Image> img = Image::create(size.width, size.height, false, Image::FORMAT_RGBA8);
            texture_rid = rs->texture_2d_create(img);
            texture_rd_ref.unref();
        }
        texture_size = size;
    }

    rive_integration::render_texture(rd, texture_rid, this, size.width, size.height);
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

    rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory)
    {
        ERR_PRINT("Rive factory not available (context not created?)");
        return;
    }

    rive::Span<const uint8_t> bytes(data.ptr(), data.size());
    rive::ImportResult result;
    rive::rcp<rive::File> file = rive::File::import(bytes, factory, &result);

    if (file)
    {
        state_machine.reset();
        animation.reset();
        view_model_instance = nullptr;
        artboard.reset();
        rive_file.reset();

        rive_file = file;
        artboard = rive_file->artboardDefault();
        if (artboard)
        {
            artboard->advance(0.0f);

            int viewModelId = artboard->viewModelId();
            if (viewModelId != -1)
            {
                view_model_instance = rive_file->createViewModelInstance(viewModelId, 0);
            }

            if (!view_model_instance)
            {
                view_model_instance = rive_file->createViewModelInstance(artboard.get());
            }

            // Try to load specified state machine or animation
            if (!current_state_machine.is_empty())
            {
                state_machine = artboard->stateMachineNamed(current_state_machine.utf8().get_data());
            }

            if (!state_machine && !current_animation.is_empty())
            {
                animation = artboard->animationNamed(current_animation.utf8().get_data());
            }

            // Fallback to defaults if nothing specified or found
            if (!state_machine && !animation)
            {
                if (artboard->stateMachineCount() > 0)
                {
                    state_machine = artboard->stateMachineAt(0);
                    current_state_machine = state_machine->name().c_str();
                    current_animation = "";
                }
                else if (artboard->animationCount() > 0)
                {
                    animation = artboard->animationAt(0);
                    current_animation = animation->name().c_str();
                    current_state_machine = "";
                }
            }

            if (state_machine)
            {
                if (view_model_instance)
                {
                    state_machine->bindViewModelInstance(view_model_instance);
                }
            }
            else if (animation)
            {
                if (view_model_instance)
                {
                    artboard->bindViewModelInstance(view_model_instance);
                }
            }

            if (view_model_instance)
            {
                if (artboard)
                    artboard->advance(0.0f);
                if (state_machine)
                    state_machine->advance(0.0f);
                else if (animation)
                    animation->advance(0.0f);
            }

            _update_property_list();
            notify_property_list_changed();
            UtilityFunctions::print_verbose("Rive file loaded successfully: " + file_path);
        }
        else
        {
            ERR_PRINT("Rive file loaded but no default artboard found: " + file_path);
        }
    }
    else
    {
        ERR_PRINT("Failed to import Rive file: " + file_path);
    }
}

void RiveControl::draw(rive::Renderer *renderer)
{
    if (artboard)
    {
        renderer->save();

        rive::Mat2D transform = _get_rive_transform();

        renderer->transform(transform);
        artboard->draw(renderer);
        renderer->restore();
    }
}

bool RiveControl::_has_point(const Vector2 &p_point) const
{
    if (!state_machine)
        return false;

    rive::Mat2D transform = _get_rive_transform();
    rive::Mat2D inverse;
    if (!transform.invert(&inverse))
        return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(p_point.x, p_point.y);
    return state_machine->hitTest(rive_pos);
}

rive::Mat2D RiveControl::_get_rive_transform() const
{
    if (!artboard)
        return rive::Mat2D();

    Size2i size = get_size();
    return rive::computeAlignment(
        rive::Fit::contain,
        rive::Alignment::center,
        rive::AABB(0, 0, size.width, size.height),
        artboard->bounds());
}

void RiveControl::_gui_input(const Ref<InputEvent> &p_event)
{
    if (!state_machine || !artboard)
        return;

    Ref<InputEventMouse> mouse_event = p_event;
    if (mouse_event.is_valid())
    {
        rive::Mat2D transform = _get_rive_transform();
        rive::Mat2D inverse;
        if (!transform.invert(&inverse))
            return;

        Vector2 local_pos = mouse_event->get_position();
        rive::Vec2D rive_pos = inverse * rive::Vec2D(local_pos.x, local_pos.y);

        rive::HitResult hit_result = rive::HitResult::none;

        Ref<InputEventMouseMotion> motion = p_event;
        if (motion.is_valid())
        {
            hit_result = state_machine->pointerMove(rive_pos);
        }

        Ref<InputEventMouseButton> button = p_event;
        if (button.is_valid())
        {
            if (button->get_button_index() == MOUSE_BUTTON_LEFT)
            {
                if (button->is_pressed())
                {
                    hit_result = state_machine->pointerDown(rive_pos);
                }
                else
                {
                    hit_result = state_machine->pointerUp(rive_pos);
                }
            }
        }

        if (hit_result != rive::HitResult::none)
        {
            accept_event();
        }
    }
}

void RiveControl::play_animation(const String &p_name)
{
    if (!artboard)
        return;

    state_machine.reset();

    animation = artboard->animationNamed(p_name.utf8().get_data());
    if (!animation)
    {
        ERR_PRINT("Animation not found: " + p_name);
    }
}

void RiveControl::play_state_machine(const String &p_name)
{
    if (!artboard)
        return;

    animation.reset();

    state_machine = artboard->stateMachineNamed(p_name.utf8().get_data());
    if (!state_machine)
    {
        ERR_PRINT("State machine not found: " + p_name);
    }
}

PackedStringArray RiveControl::get_animation_list() const
{
    PackedStringArray list;
    if (artboard)
    {
        for (size_t i = 0; i < artboard->animationCount(); ++i)
        {
            list.push_back(String(artboard->animation(i)->name().c_str()));
        }
    }
    return list;
}

PackedStringArray RiveControl::get_state_machine_list() const
{
    PackedStringArray list;
    if (artboard)
    {
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i)
        {
            list.push_back(String(artboard->stateMachine(i)->name().c_str()));
        }
    }
    return list;
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
    current_animation = p_name;
    if (!p_name.is_empty())
    {
        play_animation(p_name);
        current_state_machine = "";
    }
}

String RiveControl::get_animation_name() const
{
    return current_animation;
}

void RiveControl::set_state_machine_name(const String &p_name)
{
    current_state_machine = p_name;
    if (!p_name.is_empty())
    {
        play_state_machine(p_name);
        current_animation = "";
    }
}

String RiveControl::get_state_machine_name() const
{
    return current_state_machine;
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
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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

void RiveControl::_collect_view_model_properties(rive::ViewModelInstance *vm, String prefix)
{
    if (!vm)
        return;

    std::vector<rive::ViewModelInstanceValue *> props = vm->propertyValues();

    for (size_t i = 0; i < props.size(); ++i)
    {
        rive::ViewModelInstanceValue *value = props[i];
        if (!value)
            continue;

        String name = value->name().c_str();
        String path = prefix.is_empty() ? name : prefix + "." + name;

        if (value->is<rive::ViewModelInstanceNumber>())
        {
            rive_properties.push_back({path, Variant::FLOAT});
        }
        else if (value->is<rive::ViewModelInstanceString>())
        {
            rive_properties.push_back({path, Variant::STRING});
        }
        else if (value->is<rive::ViewModelInstanceBoolean>())
        {
            rive_properties.push_back({path, Variant::BOOL});
        }
        else if (value->is<rive::ViewModelInstanceTrigger>())
        {
            rive_properties.push_back({path, Variant::BOOL, true});
        }
        else if (value->is<rive::ViewModelInstanceColor>())
        {
            rive_properties.push_back({path, Variant::COLOR});
        }
        else if (rive::ViewModelInstanceViewModel *child_vm_prop = value->as<rive::ViewModelInstanceViewModel>())
        {
            rive::rcp<rive::ViewModelInstance> child_vm = child_vm_prop->referenceViewModelInstance();
            if (child_vm)
            {
                _collect_view_model_properties(child_vm.get(), path);
            }
        }
        else if (rive::ViewModelInstanceEnum *enum_val = value->as<rive::ViewModelInstanceEnum>())
        {
            String hint;
            rive::ViewModelProperty *prop_base = enum_val->viewModelProperty();
            if (prop_base && prop_base->is<rive::ViewModelPropertyEnum>())
            {
                rive::ViewModelPropertyEnum *vm_prop = prop_base->as<rive::ViewModelPropertyEnum>();
                if (vm_prop)
                {
                    rive::DataEnum *data_enum = vm_prop->dataEnum();
                    if (data_enum)
                    {
                        std::vector<rive::DataEnumValue *> values = data_enum->values();
                        for (size_t i = 0; i < values.size(); ++i)
                        {
                            if (values[i])
                            {
                                if (i > 0)
                                    hint += ",";
                                hint += values[i]->key().c_str();
                            }
                        }
                    }
                }
            }
            rive_properties.push_back({path, Variant::INT, false, hint});
        }
    }
}

void RiveControl::_update_property_list()
{
    rive_properties.clear();
    if (view_model_instance)
    {
        _collect_view_model_properties(view_model_instance.get(), "");
    }
    notify_property_list_changed();
}

void RiveControl::_get_property_list(List<PropertyInfo> *p_list) const
{
    for (const RiveProperty &prop : rive_properties)
    {
        if (!prop.enum_hint.is_empty())
        {
            p_list->push_back(PropertyInfo(prop.type, "rive/" + prop.path, PROPERTY_HINT_ENUM, prop.enum_hint));
        }
        else
        {
            p_list->push_back(PropertyInfo(prop.type, "rive/" + prop.path));
        }
    }
}

bool RiveControl::_get(const StringName &p_name, Variant &r_ret) const
{
    String name = p_name;
    if (name.begins_with("rive/"))
    {
        String path = name.substr(5);

        if (!view_model_instance)
            return false;

        String prop_name;
        rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), path, prop_name);

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
    if (name.begins_with("rive/"))
    {
        String path = name.substr(5);

        for (const RiveProperty &prop : rive_properties)
        {
            if (prop.path == path)
            {
                if (prop.is_trigger)
                {
                    if (p_value)
                    {
                        const_cast<RiveControl *>(this)->fire_trigger(path);
                    }
                    return true;
                }
                if (prop.type == Variant::FLOAT)
                {
                    const_cast<RiveControl *>(this)->set_number_value(path, p_value);
                    return true;
                }
                if (prop.type == Variant::STRING)
                {
                    const_cast<RiveControl *>(this)->set_text_value(path, p_value);
                    return true;
                }
                if (prop.type == Variant::BOOL)
                {
                    const_cast<RiveControl *>(this)->set_boolean_value(path, p_value);
                    return true;
                }
                if (prop.type == Variant::INT)
                {
                    const_cast<RiveControl *>(this)->set_enum_value(path, p_value);
                    return true;
                }
                if (prop.type == Variant::COLOR)
                {
                    const_cast<RiveControl *>(this)->set_color_value(path, p_value);
                    return true;
                }
            }
        }
    }
    return false;
}

void RiveControl::set_enum_value(const String &p_property_path, int p_value)
{
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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
    if (!view_model_instance)
        return;

    String prop_name;
    rive::ViewModelInstance *target_vm = resolve_view_model_instance(view_model_instance.get(), p_property_path, prop_name);

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
