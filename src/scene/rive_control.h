#ifndef RIVE_CONTROL_H
#define RIVE_CONTROL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>

#include "renderer/rive_render_registry.h"
#include "rive_player.h"
#include "../renderer/rive_texture_target.h"
#include "../resources/rive_file.h"

using namespace godot;

class RiveControl : public Control, public RiveDrawable
{
    GDCLASS(RiveControl, Control);

    Ref<RiveFile> rive_file;
    Ref<RivePlayer> rive_player;
    Ref<RiveTextureTarget> texture_target;
    Dictionary property_values;

    struct RiveProperty
    {
        String path;
        Variant::Type type;
        bool is_trigger = false;
        String enum_hint;
    };
    // Vector<RiveProperty> rive_properties; // Removed, handled by InspectorPlugin

protected:
    static void _bind_methods();
    void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    void _validate_property(PropertyInfo &p_property) const;

    // void _update_property_list(); // Removed
    // void _collect_view_model_properties(rive::ViewModelInstance *vm, String prefix); // Removed

    // Internal helper
    void _render_rive();
    rive::Mat2D _get_rive_transform() const;
    void _apply_property_values();
    void _on_rive_file_changed();

public:
    RiveControl();
    ~RiveControl();

    void set_rive_file(const Ref<RiveFile> &p_file);
    Ref<RiveFile> get_rive_file() const;

    void set_property_values(const Dictionary &p_values);
    Dictionary get_property_values() const;

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

    Ref<RiveViewModelInstance> get_view_model_instance() const;
    Ref<RivePlayer> get_rive_player() const { return rive_player; }
};

#endif // RIVE_CONTROL_H
