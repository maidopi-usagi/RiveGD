#ifndef RIVE_PLAYER_H
#define RIVE_PLAYER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>

#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/renderer.hpp>

using namespace godot;

class RivePlayer : public RefCounted {
    GDCLASS(RivePlayer, RefCounted);

private:
    rive::rcp<rive::File> rive_file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::LinearAnimationInstance> animation;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    rive::rcp<rive::ViewModelInstance> view_model_instance;

    String current_animation;
    String current_state_machine;

protected:
    static void _bind_methods() {}

public:
    RivePlayer();
    ~RivePlayer();

    bool load_from_bytes(const PackedByteArray &data);
    bool load(rive::rcp<rive::File> file);
    void set_artboard(std::unique_ptr<rive::ArtboardInstance> p_artboard, rive::rcp<rive::File> p_file = nullptr);
    
    void advance(float delta);
    void draw(rive::Renderer *renderer, const rive::Mat2D &transform);

    // Input handling
    bool hit_test(Vector2 position, const rive::Mat2D &transform);
    bool pointer_down(Vector2 position, const rive::Mat2D &transform);
    bool pointer_up(Vector2 position, const rive::Mat2D &transform);
    bool pointer_move(Vector2 position, const rive::Mat2D &transform);

    // Animation/State Machine control
    void play_animation(const String &p_name);
    void play_state_machine(const String &p_name);
    
    PackedStringArray get_animation_list() const;
    PackedStringArray get_state_machine_list() const;

    String get_animation_name() const { return current_animation; }
    String get_state_machine_name() const { return current_state_machine; }

    // ViewModel access
    rive::ViewModelInstance *get_view_model_instance() const { return view_model_instance.get(); }
    rive::ArtboardInstance *get_artboard() const { return artboard.get(); }
    rive::StateMachineInstance *get_state_machine() const { return state_machine.get(); }
    rive::LinearAnimationInstance *get_animation() const { return animation.get(); }
};

#endif // RIVE_PLAYER_H
