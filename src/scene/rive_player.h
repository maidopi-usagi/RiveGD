#ifndef RIVE_PLAYER_H
#define RIVE_PLAYER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/renderer.hpp>
#include "rive_view_model.h"

using namespace godot;
namespace rive { class TextSelectionController; }

class RivePlayer : public RefCounted, private rive::NestedEventListener {
    GDCLASS(RivePlayer, RefCounted);

private:
    rive::rcp<rive::File> rive_file;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::LinearAnimationInstance> animation;
    struct AnimationLayer {
        std::unique_ptr<rive::LinearAnimationInstance> instance;
        float weight = 1.0f;
        float speed = 1.0f;
        int initial_direction = 1;
    };
    std::vector<AnimationLayer> animation_layers;
    std::unique_ptr<rive::LinearAnimationInstance> fading_animation;
    double fade_duration = 0.0;
    double fade_elapsed = 0.0;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    rive::rcp<rive::ViewModelInstance> view_model_instance;
    Ref<RiveViewModelInstance> wrapper_view_model_instance;
    HashMap<String, Ref<RiveViewModelInstance>> global_view_models;
    Callable property_changed_callback;
    Callable trigger_fired_callback;
    void connect_view_model_signals(const Ref<RiveViewModelInstance> &p_model, bool p_connect);

    String current_animation;
    String current_state_machine;
    String requested_animation;
    String requested_state_machine;
    HashMap<int, Vector2> pointer_positions;
    Array pending_events;
    size_t pointer_event_cursor = 0;
    void collect_state_machine_events();
    void clear_global_view_models();
    bool advancing = false;
    bool animation_completed = false;
    bool semantics_enabled = false;
    uint64_t semantics_generation = 0;
    double volume = 1.0;
    int animation_loop = LOOP_AUTHORED;
    int animation_direction = 1;
    HashMap<int, Dictionary> gamepads;
    std::unique_ptr<rive::TextSelectionController> text_selection;
    bool update_gamepad_connection(int p_device, bool p_connected, bool p_notify);
    rive::SMIInput *resolve_input(const String &p_name, const String &p_path) const;
    void notify(const std::vector<rive::EventReport> &p_events, rive::NestedArtboard *p_context) override;
    void _on_property_changed(const String &p_path, const Variant &p_value);
    void _on_trigger_fired(const String &p_path);

protected:
    static void _bind_methods();

public:
    enum LoopMode { LOOP_AUTHORED = -1, LOOP_ONE_SHOT = 0, LOOP_REPEAT = 1, LOOP_PING_PONG = 2 };
    enum PointerPhase { POINTER_DOWN = 0, POINTER_UP = 1, POINTER_MOVE = 2, POINTER_DRAG_START = 3, POINTER_DRAG_END = 4, POINTER_EXIT = 5 };
    enum ScrollPhase { SCROLL_BEGIN = 0, SCROLL_UPDATE = 1, SCROLL_END = 2, SCROLL_MOMENTUM = 3, SCROLL_INERTIA_CANCEL = 4 };
    enum SemanticAction { SEMANTIC_TAP = 0, SEMANTIC_INCREASE = 1, SEMANTIC_DECREASE = 2 };
    RivePlayer();
    ~RivePlayer();

    bool load_from_bytes(const PackedByteArray &data);
    bool load(rive::rcp<rive::File> file);
    void set_artboard(std::unique_ptr<rive::ArtboardInstance> p_artboard, rive::rcp<rive::File> p_file = nullptr);
    
    bool advance(float delta);
    bool flush_view_model_changes();
    void draw(rive::Renderer *renderer, const rive::Mat2D &transform);

    bool hit_test(Vector2 position, const rive::Mat2D &transform);
    bool pointer_down(Vector2 position, const rive::Mat2D &transform, int pointer_id = 0, int button = 0);
    bool pointer_up(Vector2 position, const rive::Mat2D &transform, int pointer_id = 0, int button = 0);
    bool pointer_move(Vector2 position, const rive::Mat2D &transform, int pointer_id = 0);
    bool pointer_exit(int pointer_id = 0);
    bool scroll(Vector2 position, Vector2 delta, const rive::Mat2D &transform);
    bool focus_next();
    bool focus_previous();
    bool focus_direction(int direction);
    void clear_focus();
    bool key_input(const Ref<InputEventKey> &event);
    bool text_input(const String &text);
    bool update_text_composition(const String &text, Vector2i selection = Vector2i());
    void cancel_text_composition();
    String get_selected_text() const;
    bool set_text_selectable(const String &p_name, bool p_enabled, const String &p_path = "");
    Dictionary hit_test_selectable_text(Vector2 p_position);

    void play_animation(const String &p_name);
    void play_state_machine(const String &p_name);
    bool configure_playback(const String &p_animation, const String &p_state_machine);
    bool seek(float p_time);
    void reset();
    float get_time() const;
    float get_duration() const;
    bool bind_view_model(const Ref<RiveViewModelInstance> &p_instance);
    Ref<RiveViewModelInstance> get_global_view_model(const String &p_name);
    bool bind_global_view_model(const String &p_name, const Ref<RiveViewModelInstance> &p_instance);
    bool set_input_value(const String &p_name, const Variant &p_value, const String &p_path = "");
    Variant get_input_value(const String &p_name, const String &p_path = "") const;
    bool fire_input(const String &p_name, const String &p_path = "");
    Array get_input_list(const String &p_path = "") const;
    void set_volume(double p_volume);
    double get_volume() const { return volume; }
    bool set_animation_loop(int p_loop);
    int get_animation_loop() const { return animation_loop; }
    bool set_animation_direction(int p_direction);
    int get_animation_direction() const { return animation_direction; }
    bool set_animation_layers(const Array &p_layers);
    Array get_animation_layers() const;
    bool cross_fade_animation(const String &p_name, double p_seconds = 0.25);
    bool set_text_run(const String &p_name, const String &p_value, const String &p_path = "");
    String get_text_run(const String &p_name, const String &p_path = "") const;
    void set_semantics_enabled(bool p_enabled);
    bool is_semantics_enabled() const { return semantics_enabled; }
    Dictionary take_semantics_diff();
    bool request_semantic_focus(int64_t p_id);
    bool fire_semantic_action(int64_t p_id, int p_action);
    bool gamepad_input(const Ref<InputEvent> &p_event);
    bool set_gamepad_connected(int p_device, bool p_connected);
    Dictionary get_gamepad_state(int p_device) const;
    bool send_pointer(Vector2 p_position, int p_phase, int p_pointer_id = 0, int p_button = 0, double p_timestamp = 0.0);
    bool send_scroll(Vector2 p_position, Vector2 p_delta, int p_phase = 1, bool p_precise = false, double p_timestamp = 0.0);
    void cancel_scroll();
    Array get_components(const String &p_path = "") const;
    Dictionary get_component(const String &p_name, const String &p_path = "") const;
    bool set_joystick(const String &p_name, Vector2 p_value, const String &p_path = "");
    Array get_selectable_texts();
    bool set_text_selection(int64_t p_anchor_id, int64_t p_start, int64_t p_extent_id, int64_t p_end);
    bool selection_pointer(int64_t p_target_id, Vector2 p_position, int p_phase, int p_pointer_id = 0, bool p_extend = false);
    void clear_text_selection();
    Dictionary get_text_input_state() const;
    static Dictionary get_capabilities();
    
    PackedStringArray get_animation_list() const;
    PackedStringArray get_state_machine_list() const;

    String get_animation_name() const { return current_animation; }
    String get_state_machine_name() const { return current_state_machine; }

    Ref<RiveViewModelInstance> get_rive_view_model_instance();

    rive::ViewModelInstance *get_view_model_instance() const { return view_model_instance.get(); }
    rive::ArtboardInstance *get_artboard() const { return artboard.get(); }
    rive::StateMachineInstance *get_state_machine() const { return state_machine.get(); }
    rive::LinearAnimationInstance *get_animation() const { return animation.get(); }
};

VARIANT_ENUM_CAST(RivePlayer::LoopMode);
VARIANT_ENUM_CAST(RivePlayer::PointerPhase);
VARIANT_ENUM_CAST(RivePlayer::ScrollPhase);
VARIANT_ENUM_CAST(RivePlayer::SemanticAction);

#endif
