#ifndef RIVE_CONTROL_H
#define RIVE_CONTROL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include "renderer/rive_render_registry.h"
#include "rive_player.h"
#include "../renderer/rive_texture_target.h"
#include "../resources/rive_file.h"
#include "../resources/rive_view_model_preset.h"
#include <vector>

using namespace godot;

class RiveControl : public Control, public RiveDrawable
{
    GDCLASS(RiveControl, Control);

public:
    enum Fit {
        FIT_FILL,
        FIT_CONTAIN,
        FIT_COVER,
        FIT_WIDTH,
        FIT_HEIGHT,
        FIT_NONE,
        FIT_SCALE_DOWN,
        FIT_LAYOUT,
    };

    enum ExpandMode { EXPAND_IGNORE_SIZE, EXPAND_KEEP_SIZE };
    enum UpdateMode { UPDATE_ALWAYS, UPDATE_WHEN_NEEDED, UPDATE_MANUAL };

    enum Alignment {
        ALIGNMENT_TOP_LEFT,
        ALIGNMENT_TOP_CENTER,
        ALIGNMENT_TOP_RIGHT,
        ALIGNMENT_CENTER_LEFT,
        ALIGNMENT_CENTER,
        ALIGNMENT_CENTER_RIGHT,
        ALIGNMENT_BOTTOM_LEFT,
        ALIGNMENT_BOTTOM_CENTER,
        ALIGNMENT_BOTTOM_RIGHT,
    };

private:

    Ref<RiveFile> rive_file;
    Ref<RivePlayer> rive_player;
    Ref<RiveTextureTarget> texture_target;
    Dictionary property_values;
    Dictionary property_defaults;
    String artboard_name;
    String animation_name;
    String state_machine_name;
    Ref<RiveViewModelPreset> view_model_preset;
    std::vector<Ref<RiveViewModelPreset>> preset_subscriptions;
    Fit fit = FIT_CONTAIN;
    Alignment alignment = ALIGNMENT_CENTER;
    ExpandMode expand_mode = EXPAND_IGNORE_SIZE;
    UpdateMode update_mode = UPDATE_ALWAYS;
    bool playing = true;
    double speed_scale = 1.0;
    double layout_scale = 1.0;
    double volume = 1.0;
    bool render_dirty = true;
    bool needs_advance = true;
    bool ime_active = false;
    bool virtual_keyboard_active = false;
    bool loading = false;
    ObjectID reported_view_model_id;
    String load_error;
    String property_schema_signature;
    Vector2 intrinsic_size;

protected:
    static void _bind_methods();
    void _notification(int p_what);
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(List<PropertyInfo> *p_list) const;
    void _validate_property(PropertyInfo &p_property) const;
    bool _property_can_revert(const StringName &p_name) const;
    bool _property_get_revert(const StringName &p_name, Variant &r_ret) const;

    void _render_rive();
    rive::Mat2D _get_rive_transform() const;
    void _apply_property_values();
    bool _apply_property_value(const String &p_path, const Variant &p_value, bool p_replay = false);
    void _on_rive_file_changed();
    Error _fail_load(Error p_error, const String &p_message);
    void _invalidate_rive();
    void _update_layout();
    void _on_preset_changed();
    void _update_preset_connections();
    void _disconnect_preset_connections();
    void _on_mouse_exited();
    void _on_joy_connection_changed(int p_device, bool p_connected);
    void _refresh_property_schema();
    void _sync_keyboard(bool p_release = false);
    void _on_player_changed();
    void _connect_player_signals(const Ref<RivePlayer> &p_player);
    void _disconnect_player_signals(const Ref<RivePlayer> &p_player);
    void _on_player_event(const String &p_name, const Dictionary &p_payload);
    void _on_player_animation_finished(const String &p_name);
    void _on_player_animation_looped(const String &p_name);
    void _on_player_state_changed(const String &p_name);

public:
    RiveControl();
    ~RiveControl();

    void set_rive_file(const Ref<RiveFile> &p_file);
    Ref<RiveFile> get_rive_file() const;
    void set_view_model_preset(const Ref<RiveViewModelPreset> &p_preset);
    Ref<RiveViewModelPreset> get_view_model_preset() const;

    void set_artboard_name(const String &p_name);
    String get_artboard_name() const;
    PackedStringArray get_artboard_list() const;

    void set_fit(Fit p_fit);
    Fit get_fit() const;
    void set_alignment(Alignment p_alignment);
    Alignment get_alignment() const;
    void set_expand_mode(ExpandMode p_mode);
    ExpandMode get_expand_mode() const;
    Vector2 get_intrinsic_size() const;
    Vector2 get_layout_size() const;
    Transform2D get_artboard_transform() const;
    Vector2 _get_minimum_size() const override;
    void set_layout_scale(double p_scale);
    double get_layout_scale() const;
    PackedStringArray _get_configuration_warnings() const override;

    void set_playing(bool p_playing);
    bool is_playing() const;
    void set_speed_scale(double p_scale);
    double get_speed_scale() const;
    void set_update_mode(UpdateMode p_mode);
    UpdateMode get_update_mode() const;
    void play();
    void pause();
    void stop();
    void reset();
    void advance(double p_delta);
    bool seek(double p_time);
    double get_time() const;
    double get_duration() const;
    void set_volume(double p_volume);
    double get_volume() const { return volume; }

    void set_property_values(const Dictionary &p_values);
    Dictionary get_property_values() const;

    Error load_file();

    void draw(rive::Renderer *renderer) override;

    virtual void _gui_input(const Ref<InputEvent> &p_event) override;
    virtual bool _has_point(const Vector2 &p_point) const override;

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
    void set_image_value(const String &p_property_path, const Ref<Texture2D> &p_value);
    void set_font_value(const String &p_property_path, const Ref<FontFile> &p_value);
    void set_artboard_value(const String &p_property_path, const String &p_value);
    void set_list_value(const String &p_property_path, const Array &p_value);
    bool set_property_value(const String &p_property_path, const Variant &p_value);
    Variant get_property_value(const String &p_property_path) const;
    Variant get_property_override(const String &p_property_path) const;
    bool has_property_override(const String &p_property_path) const;
    void clear_property_override(const String &p_property_path);
    bool set_input_value(const String &p_name, const Variant &p_value, const String &p_path = "");
    Variant get_input_value(const String &p_name, const String &p_path = "") const;
    void fire_input(const String &p_name, const String &p_path = "");
    Array get_input_list(const String &p_path = "") const;
    bool set_text_run(const String &p_name, const String &p_value, const String &p_path = "");
    String get_text_run(const String &p_name, const String &p_path = "") const;

    Ref<RiveViewModelInstance> get_view_model_instance() const;
    bool bind_view_model(const Ref<RiveViewModelInstance> &p_instance);
    Ref<RivePlayer> get_rive_player() const { return rive_player; }
    Ref<RivePlayer> get_player() const { return rive_player; }
};

VARIANT_ENUM_CAST(RiveControl::Fit);
VARIANT_ENUM_CAST(RiveControl::Alignment);
VARIANT_ENUM_CAST(RiveControl::ExpandMode);
VARIANT_ENUM_CAST(RiveControl::UpdateMode);

#endif
