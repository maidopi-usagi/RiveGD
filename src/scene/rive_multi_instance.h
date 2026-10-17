#ifndef RIVE_MULTI_INSTANCE_H
#define RIVE_MULTI_INSTANCE_H

#include "rive_node.h"
#include "../resources/rive_file.h"
#include "rive_player.h"
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

class RiveMultiInstance : public RiveNode {
    GDCLASS(RiveMultiInstance, RiveNode);

private:
    Ref<RiveFile> rive_file_resource;
    Ref<RivePlayer> rive_player;
    
    String artboard_name;
    String state_machine_name;
    String animation_name;
    bool future_active = true;
    String load_error;

    TypedArray<Transform2D> transforms;

    bool _load_artboard();
    void _advance_player(double p_delta, bool p_automatic);
    void _on_rive_file_changed();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveMultiInstance();
    ~RiveMultiInstance();

    void set_rive_file(const Ref<RiveFile> &p_file);
    Ref<RiveFile> get_rive_file() const;

    void set_artboard_name(const String &p_name);
    String get_artboard_name() const;

    void set_state_machine_name(const String &p_name);
    String get_state_machine_name() const;
    
    void set_animation_name(const String &p_name);
    String get_animation_name() const;
    
    void set_auto_play(bool p_auto);
    bool get_auto_play() const;

    void set_transforms(const Array &p_transforms);
    TypedArray<Transform2D> get_transforms() const;
    Ref<RivePlayer> get_rive_player() const override { return rive_player; }
    Ref<RiveViewModelInstance> get_view_model_instance() const;

    void advance(double delta) override;
    void advance_rive_automatically(double delta) override;
    void draw(rive::Renderer *renderer) override;
    Rect2 get_rive_bounds() const override;
    bool wants_continuous_redraw() const override { return get_update_mode() == UPDATE_ALWAYS; }
    PackedStringArray _get_configuration_warnings() const override;
};

#endif
