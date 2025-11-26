#ifndef RIVE_CONTROL_H
#define RIVE_CONTROL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>

#include "rive_render_registry.h"
#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>

using namespace godot;

class RiveControl : public Control, public RiveDrawable
{
    GDCLASS(RiveControl, Control);

    String file_path;
    rive::rcp<rive::File> rive_file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::LinearAnimationInstance> animation;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    rive::rcp<rive::ViewModelInstance> view_model_instance;

    RID texture_rid;
    Ref<Texture2DRD> texture_rd_ref; // Keep reference if using RD
    Size2i texture_size;

    String current_animation;
    String current_state_machine;

    struct RiveProperty
    {
        String path;
        Variant::Type type;
        bool is_trigger = false;
        String enum_hint;
    };
    Vector<RiveProperty> rive_properties;

protected:
    static void _bind_methods();
    void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    void _validate_property(PropertyInfo &p_property) const;

    void _update_property_list();
    void _collect_view_model_properties(rive::ViewModelInstance *vm, String prefix);

    // Internal helper
    void _render_rive();
    rive::Mat2D _get_rive_transform() const;

public:
    RiveControl();
    ~RiveControl();

    void set_file_path(const String &p_path);
    String get_file_path() const;

    void load_file();

    // RiveDrawable implementation
    void draw(rive::Renderer *renderer) override;

    // Input handling
    virtual void _gui_input(const Ref<InputEvent> &p_event) override;
    virtual bool _has_point(const Vector2 &p_point) const override;

    // API
    void play_animation(const String &p_name);
    void play_state_machine(const String &p_name);
    PackedStringArray get_animation_list() const;
    PackedStringArray get_state_machine_list() const;

    void set_animation_name(const String &p_name);
    String get_animation_name() const;
    void set_state_machine_name(const String &p_name);
    String get_state_machine_name() const;

    void set_text_value(const String &p_property_path, const String &p_value);
    void set_number_value(const String &p_property_path, float p_value);
    void set_boolean_value(const String &p_property_path, bool p_value);
    void fire_trigger(const String &p_property_path);
    void set_enum_value(const String &p_property_path, int p_value);
    void set_color_value(const String &p_property_path, Color p_value);
};

#endif // RIVE_CONTROL_H
