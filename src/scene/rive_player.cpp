#include "rive_player.h"
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <rive/custom_property_number.hpp>
#include <rive/custom_property_boolean.hpp>
#include <rive/custom_property_string.hpp>
#include <rive/custom_property_color.hpp>
#include <rive/custom_property_enum.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/entry_state.hpp>
#include <rive/animation/exit_state.hpp>
#include <rive/animation/any_state.hpp>
#include <rive/input/focusable.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/nested_input.hpp>
#include <rive/open_url_event.hpp>
#include <rive/text/text_selection_controller.hpp>
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <cmath>
#include <limits>

void RivePlayer::_bind_methods() {
    BIND_ENUM_CONSTANT(LOOP_AUTHORED); BIND_ENUM_CONSTANT(LOOP_ONE_SHOT);
    BIND_ENUM_CONSTANT(LOOP_REPEAT); BIND_ENUM_CONSTANT(LOOP_PING_PONG);
    BIND_ENUM_CONSTANT(POINTER_DOWN); BIND_ENUM_CONSTANT(POINTER_UP); BIND_ENUM_CONSTANT(POINTER_MOVE);
    BIND_ENUM_CONSTANT(POINTER_DRAG_START); BIND_ENUM_CONSTANT(POINTER_DRAG_END); BIND_ENUM_CONSTANT(POINTER_EXIT);
    BIND_ENUM_CONSTANT(SCROLL_BEGIN); BIND_ENUM_CONSTANT(SCROLL_UPDATE); BIND_ENUM_CONSTANT(SCROLL_END);
    BIND_ENUM_CONSTANT(SCROLL_MOMENTUM); BIND_ENUM_CONSTANT(SCROLL_INERTIA_CANCEL);
    BIND_ENUM_CONSTANT(SEMANTIC_TAP); BIND_ENUM_CONSTANT(SEMANTIC_INCREASE); BIND_ENUM_CONSTANT(SEMANTIC_DECREASE);
    ClassDB::bind_method(D_METHOD("set_animation_layers", "layers"), &RivePlayer::set_animation_layers);
    ClassDB::bind_method(D_METHOD("get_animation_layers"), &RivePlayer::get_animation_layers);
    ClassDB::bind_method(D_METHOD("cross_fade_animation", "name", "seconds"), &RivePlayer::cross_fade_animation, DEFVAL(0.25));
    ClassDB::bind_method(D_METHOD("get_rive_view_model_instance"), &RivePlayer::get_rive_view_model_instance);
    ClassDB::bind_method(D_METHOD("load_from_bytes", "data"), &RivePlayer::load_from_bytes);
    ClassDB::bind_method(D_METHOD("configure_playback", "animation", "state_machine"), &RivePlayer::configure_playback);
    ClassDB::bind_method(D_METHOD("play_animation", "name"), &RivePlayer::play_animation);
    ClassDB::bind_method(D_METHOD("play_state_machine", "name"), &RivePlayer::play_state_machine);
    ClassDB::bind_method(D_METHOD("advance", "delta"), &RivePlayer::advance);
    ClassDB::bind_method(D_METHOD("flush_view_model_changes"), &RivePlayer::flush_view_model_changes);
    ClassDB::bind_method(D_METHOD("seek", "time"), &RivePlayer::seek);
    ClassDB::bind_method(D_METHOD("reset"), &RivePlayer::reset);
    ClassDB::bind_method(D_METHOD("get_time"), &RivePlayer::get_time);
    ClassDB::bind_method(D_METHOD("get_duration"), &RivePlayer::get_duration);
    ClassDB::bind_method(D_METHOD("get_animation_name"), &RivePlayer::get_animation_name);
    ClassDB::bind_method(D_METHOD("get_state_machine_name"), &RivePlayer::get_state_machine_name);
    ClassDB::bind_method(D_METHOD("get_animation_list"), &RivePlayer::get_animation_list);
    ClassDB::bind_method(D_METHOD("get_state_machine_list"), &RivePlayer::get_state_machine_list);
    ClassDB::bind_method(D_METHOD("bind_view_model", "instance"), &RivePlayer::bind_view_model);
    ClassDB::bind_method(D_METHOD("get_global_view_model", "name"), &RivePlayer::get_global_view_model);
    ClassDB::bind_method(D_METHOD("bind_global_view_model", "name", "instance"), &RivePlayer::bind_global_view_model);
    ClassDB::bind_method(D_METHOD("set_input_value", "name", "value", "path"), &RivePlayer::set_input_value, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_input_value", "name", "path"), &RivePlayer::get_input_value, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("fire_input", "name", "path"), &RivePlayer::fire_input, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_input_list", "path"), &RivePlayer::get_input_list, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("set_volume", "volume"), &RivePlayer::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &RivePlayer::get_volume);
    ClassDB::bind_method(D_METHOD("set_animation_loop", "loop"), &RivePlayer::set_animation_loop);
    ClassDB::bind_method(D_METHOD("get_animation_loop"), &RivePlayer::get_animation_loop);
    ClassDB::bind_method(D_METHOD("set_animation_direction", "direction"), &RivePlayer::set_animation_direction);
    ClassDB::bind_method(D_METHOD("get_animation_direction"), &RivePlayer::get_animation_direction);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
    ClassDB::bind_method(D_METHOD("set_text_run", "name", "value", "path"), &RivePlayer::set_text_run, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_text_run", "name", "path"), &RivePlayer::get_text_run, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("focus_next"), &RivePlayer::focus_next);
    ClassDB::bind_method(D_METHOD("focus_previous"), &RivePlayer::focus_previous);
    ClassDB::bind_method(D_METHOD("focus_direction", "direction"), &RivePlayer::focus_direction);
    ClassDB::bind_method(D_METHOD("clear_focus"), &RivePlayer::clear_focus);
    ClassDB::bind_method(D_METHOD("key_input", "event"), &RivePlayer::key_input);
    ClassDB::bind_method(D_METHOD("text_input", "text"), &RivePlayer::text_input);
    ClassDB::bind_method(D_METHOD("update_text_composition", "text", "selection"), &RivePlayer::update_text_composition, DEFVAL(Vector2i()));
    ClassDB::bind_method(D_METHOD("cancel_text_composition"), &RivePlayer::cancel_text_composition);
    ClassDB::bind_method(D_METHOD("get_selected_text"), &RivePlayer::get_selected_text);
    ClassDB::bind_method(D_METHOD("set_text_selectable", "name", "enabled", "path"), &RivePlayer::set_text_selectable, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("hit_test_selectable_text", "position"), &RivePlayer::hit_test_selectable_text);
    ClassDB::bind_method(D_METHOD("set_semantics_enabled", "enabled"), &RivePlayer::set_semantics_enabled);
    ClassDB::bind_method(D_METHOD("is_semantics_enabled"), &RivePlayer::is_semantics_enabled);
    ClassDB::bind_method(D_METHOD("take_semantics_diff"), &RivePlayer::take_semantics_diff);
    ClassDB::bind_method(D_METHOD("request_semantic_focus", "id"), &RivePlayer::request_semantic_focus);
    ClassDB::bind_method(D_METHOD("fire_semantic_action", "id", "action"), &RivePlayer::fire_semantic_action);
    ClassDB::bind_method(D_METHOD("gamepad_input", "event"), &RivePlayer::gamepad_input);
    ClassDB::bind_method(D_METHOD("set_gamepad_connected", "device", "connected"), &RivePlayer::set_gamepad_connected);
    ClassDB::bind_method(D_METHOD("get_gamepad_state", "device"), &RivePlayer::get_gamepad_state);
    ClassDB::bind_method(D_METHOD("send_pointer", "position", "phase", "pointer_id", "button", "timestamp"), &RivePlayer::send_pointer, DEFVAL(0), DEFVAL(0), DEFVAL(0.0));
    ClassDB::bind_method(D_METHOD("send_scroll", "position", "delta", "phase", "precise", "timestamp"), &RivePlayer::send_scroll, DEFVAL(1), DEFVAL(false), DEFVAL(0.0));
    ClassDB::bind_method(D_METHOD("cancel_scroll"), &RivePlayer::cancel_scroll);
    ClassDB::bind_method(D_METHOD("get_components", "path"), &RivePlayer::get_components, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_component", "name", "path"), &RivePlayer::get_component, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("set_joystick", "name", "value", "path"), &RivePlayer::set_joystick, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_selectable_texts"), &RivePlayer::get_selectable_texts);
    ClassDB::bind_method(D_METHOD("set_text_selection", "anchor_id", "start", "extent_id", "end"), &RivePlayer::set_text_selection);
    ClassDB::bind_method(D_METHOD("selection_pointer", "target_id", "position", "phase", "pointer_id", "extend"), &RivePlayer::selection_pointer, DEFVAL(0), DEFVAL(false));
    ClassDB::bind_method(D_METHOD("clear_text_selection"), &RivePlayer::clear_text_selection);
    ClassDB::bind_method(D_METHOD("get_text_input_state"), &RivePlayer::get_text_input_state);
    ClassDB::bind_static_method("RivePlayer", D_METHOD("get_capabilities"), &RivePlayer::get_capabilities);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "semantics_enabled"), "set_semantics_enabled", "is_semantics_enabled");
    ADD_SIGNAL(MethodInfo("changed"));
    ADD_SIGNAL(MethodInfo("rive_event", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "payload")));
    ADD_SIGNAL(MethodInfo("animation_finished", PropertyInfo(Variant::STRING, "name")));
    ADD_SIGNAL(MethodInfo("animation_looped", PropertyInfo(Variant::STRING, "name")));
    ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::STRING, "name")));
}

RivePlayer::RivePlayer() {
    // MSVC member-pointer Callables may compare unequal when reconstructed.
    property_changed_callback = callable_mp(this, &RivePlayer::_on_property_changed);
    trigger_fired_callback = callable_mp(this, &RivePlayer::_on_trigger_fired);
}

void RivePlayer::connect_view_model_signals(const Ref<RiveViewModelInstance> &p_model, bool p_connect) {
    if (p_model.is_null()) return;
    const Callable callbacks[] = {property_changed_callback, trigger_fired_callback};
    const char *signals[] = {"property_changed", "trigger_fired"};
    for (int i = 0; i < 2; ++i) {
        const bool connected = p_model->is_connected(signals[i], callbacks[i]);
        if (p_connect && !connected) p_model->connect(signals[i], callbacks[i]);
        else if (!p_connect && connected) p_model->disconnect(signals[i], callbacks[i]);
    }
}

RivePlayer::~RivePlayer() {
    set_artboard(nullptr);
}

bool RivePlayer::load_from_bytes(const PackedByteArray &data) {
    if (data.is_empty()) {
        ERR_PRINT("Cannot load an empty Rive file.");
        return false;
    }

    bool factory_ready = false;
    rive::ImportResult result{};
    rive::rcp<rive::File> file;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (!factory) return;
        factory_ready = true;
        rive::Span<const uint8_t> bytes(data.ptr(), data.size());
        file = rive::File::import(bytes, factory, &result);
    });

    if (!executed || !factory_ready) {
        ERR_PRINT("Rive factory not available (context not created?)");
        return false;
    }

    if (!file) {
        ERR_PRINT(vformat("Failed to import Rive file (ImportResult=%d).", (int)result));
        return false;
    }

    return load(std::move(file));
}

bool RivePlayer::load(rive::rcp<rive::File> file) {
    if (file) {
        std::unique_ptr<rive::ArtboardInstance> ab = file->artboardDefault();
        if (ab) {
            set_artboard(std::move(ab), std::move(file));
            return true;
        }
    }

    if (file) {
        auto release_file = [&]() { file.reset(); };
        if (!rive_integration::run_on_rive_render_thread(release_file)) {
            release_file();
        }
    }
    return false;
}

void RivePlayer::set_artboard(std::unique_ptr<rive::ArtboardInstance> p_artboard, rive::rcp<rive::File> p_file) {
    ++semantics_generation;
    auto old_layers = std::move(animation_layers);
    auto old_selection = std::move(text_selection);
    auto old_fading = std::move(fading_animation);
    fade_duration = fade_elapsed = 0.0;
    std::unique_ptr<rive::StateMachineInstance> old_state_machine = std::move(state_machine);
    std::unique_ptr<rive::LinearAnimationInstance> old_animation = std::move(animation);
    std::unique_ptr<rive::ArtboardInstance> old_artboard = std::move(artboard);
    rive::rcp<rive::ViewModelInstance> old_view_model = std::move(view_model_instance);
    rive::rcp<rive::File> old_file = std::move(rive_file);

    connect_view_model_signals(wrapper_view_model_instance, false);
    wrapper_view_model_instance.unref();
    clear_global_view_models();
    pointer_positions.clear();
    gamepads.clear();
    pending_events.clear();
    pointer_event_cursor = 0;
    current_animation = "";
    current_state_machine = "";
    animation_completed = false;
    artboard = std::move(p_artboard);
    rive_file = std::move(p_file);

    if (old_state_machine || old_animation || old_artboard || old_view_model || old_file || old_fading || !old_layers.empty()) {
        auto release_old = [&]() {
            old_selection.reset();
            old_layers.clear();
            old_fading.reset();
            if (old_state_machine) old_state_machine->dispose();
            old_state_machine.reset();
            old_animation.reset();
            old_view_model.reset();
            old_artboard.reset();
            old_file.reset();
        };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }

    if (artboard) {
        artboard->volume(static_cast<float>(volume));
        if (rive_file) {
            int viewModelId = artboard->viewModelId();
            if (viewModelId != -1) {
                view_model_instance = rive_file->createViewModelInstance(viewModelId, 0);
            }

            if (!view_model_instance) {
                view_model_instance = rive_file->createViewModelInstance(artboard.get());
            }
        }

        if (!requested_animation.is_empty()) {
            animation = artboard->animationNamed(requested_animation.utf8().get_data());
            if (animation) current_animation = requested_animation;
        }

        if (!animation && !requested_state_machine.is_empty()) {
            state_machine = artboard->stateMachineNamed(requested_state_machine.utf8().get_data());
            if (state_machine) current_state_machine = requested_state_machine;
        }

        if (!state_machine && !animation) {
            if (artboard->stateMachineCount() > 0) {
                state_machine = artboard->stateMachineAt(0);
                current_state_machine = String::utf8(state_machine->name().c_str());
                current_animation = "";
            } else if (artboard->animationCount() > 0) {
                animation = artboard->animationAt(0);
                current_animation = String::utf8(animation->name().c_str());
                current_state_machine = "";
            }
        }

        if (state_machine) {
            if (semantics_enabled) state_machine->enableSemantics();
            if (view_model_instance) {
                state_machine->bindViewModelInstance(view_model_instance);
            }
        } else if (animation) {
            animation->loopValue(animation_loop);
            animation->reset(animation_direction);
            animation->direction(animation_direction);
            animation->addNestedEventListener(this);
            if (view_model_instance) {
                artboard->bindViewModelInstance(view_model_instance);
            }
        }
        get_rive_view_model_instance();

        if (state_machine) { state_machine->advanceAndApply(0.0f); collect_state_machine_events(); }
        else if (animation) animation->advanceAndApply(0.0f);
        else if (artboard) artboard->advance(0.0f);
    }
}

bool RivePlayer::advance(float delta) {
    if (!artboard || advancing || !std::isfinite(delta) || delta < 0) return false;
    Ref<RivePlayer> keep_alive(this);
    advancing = true;
    const String animation_name = current_animation;
    PackedStringArray states;
    bool looped = false;
    bool finished = false;
    bool active = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
    if (state_machine) {
        pointer_event_cursor = 0;
        active = state_machine->advanceAndApply(delta);
        collect_state_machine_events();
        for (size_t i = 0; i < state_machine->stateChangedCount(); ++i) {
            const rive::LayerState *state = state_machine->stateChangedByIndex(i);
            if (!state) continue;
            String name;
            if (state->is<rive::AnimationState>() && state->as<rive::AnimationState>()->animation())
                name = String::utf8(state->as<rive::AnimationState>()->animation()->name().c_str());
            else if (state->is<rive::EntryState>()) name = "Entry";
            else if (state->is<rive::ExitState>()) name = "Exit";
            else if (state->is<rive::AnyState>()) name = "Any";
            else name = "Blend";
            states.push_back(name);
        }
    } else if (animation) {
        if (!fading_animation && animation_layers.empty()) active = animation->advanceAndApply(delta);
        else {
            active = animation->advance(delta, animation.get()) || animation->keepGoing();
            float amount = 1.0f;
            if (fading_animation) {
                fade_elapsed = std::min(fade_duration, fade_elapsed + delta);
                amount = fade_duration > 0 ? static_cast<float>(fade_elapsed / fade_duration) : 1.0f;
                fading_animation->advance(delta, fading_animation.get());
                fading_animation->apply();
            }
            animation->apply(amount);
            for (auto &layer : animation_layers) {
                if (layer.weight <= 0) continue;
                active = layer.instance->advance(delta * layer.speed, layer.instance.get()) || active;
                layer.instance->apply(layer.weight);
            }
            active = artboard->advance(delta) || active || amount < 1.0f;
            if (amount >= 1.0f) fading_animation.reset();
        }
        looped = animation->didLoop() && animation->loop() != rive::Loop::oneShot;
        finished = !animation_completed && delta > 0 && !animation->keepGoing();
        if (finished) animation_completed = true;
    } else {
        active = artboard->advance(delta);
    }
    });
    if (!executed) { advancing = false; return false; }
    const Array events = pending_events;
    pending_events = Array();
    flush_view_model_changes();
    for (int i = 0; i < events.size(); ++i) {
        Dictionary event = events[i];
        emit_signal("rive_event", event["name"], event["payload"]);
    }
    for (int i = 0; i < states.size(); ++i) emit_signal("state_changed", states[i]);
    if (looped) emit_signal("animation_looped", animation_name);
    if (finished) emit_signal("animation_finished", animation_name);
    advancing = false;
    return active;
}

bool RivePlayer::flush_view_model_changes() {
    Ref<RiveViewModelInstance> vm = wrapper_view_model_instance;
    Array globals;
    for (const auto &entry : global_view_models) globals.push_back(entry.value);
    bool changed = vm.is_valid() && vm->flush_changes();
    for (int i = 0; i < globals.size(); ++i) {
        Ref<RiveViewModelInstance> global = globals[i];
        changed = global->flush_changes() || changed;
    }
    return changed;
}

void RivePlayer::clear_global_view_models() {
    for (const auto &entry : global_view_models) {
        const auto &vm = entry.value;
        if (vm != wrapper_view_model_instance) connect_view_model_signals(vm, false);
    }
    global_view_models.clear();
}

void RivePlayer::draw(rive::Renderer *renderer, const rive::Mat2D &transform) {
    if (artboard) {
        renderer->save();
        renderer->transform(transform);
        artboard->draw(renderer);
        renderer->restore();
    }
}

bool RivePlayer::hit_test(Vector2 position, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    return state_machine->hitTest(rive_pos);
}

bool RivePlayer::pointer_down(Vector2 position, const rive::Mat2D &transform, int pointer_id, int button) {
    if (!state_machine || button < 0 || button > 2 || pointer_id < 0 || !position.is_finite()) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    pointer_positions[pointer_id] = Vector2(rive_pos.x, rive_pos.y);
    bool handled = state_machine->pointerDown(rive_pos, pointer_id, static_cast<rive::PointerButton>(button)) != rive::HitResult::none;
    collect_state_machine_events();
    emit_signal("changed");
    return handled;
}

bool RivePlayer::pointer_up(Vector2 position, const rive::Mat2D &transform, int pointer_id, int button) {
    if (!state_machine || button < 0 || button > 2 || pointer_id < 0 || !position.is_finite()) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    pointer_positions[pointer_id] = Vector2(rive_pos.x, rive_pos.y);
    bool handled = state_machine->pointerUp(rive_pos, pointer_id, static_cast<rive::PointerButton>(button)) != rive::HitResult::none;
    collect_state_machine_events();
    emit_signal("changed");
    return handled;
}

bool RivePlayer::pointer_move(Vector2 position, const rive::Mat2D &transform, int pointer_id) {
    if (!state_machine) return false;
    
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;

    rive::Vec2D rive_pos = inverse * rive::Vec2D(position.x, position.y);
    pointer_positions[pointer_id] = Vector2(rive_pos.x, rive_pos.y);
    bool handled = state_machine->pointerMove(rive_pos, 0, pointer_id) != rive::HitResult::none;
    collect_state_machine_events();
    emit_signal("changed");
    return handled;
}

void RivePlayer::play_animation(const String &p_name) {
    if (!artboard) { requested_animation = p_name; requested_state_machine = ""; return; }

    const std::string name = p_name.utf8().get_data();
    if (!artboard->animation(name)) {
        ERR_PRINT("Rive animation not found: " + p_name);
        return;
    }
    clear_global_view_models();
    gamepads.clear();

    current_animation = p_name;
    current_state_machine = "";
    requested_animation = p_name;
    requested_state_machine = "";
    animation_completed = false;
    pointer_positions.clear();
    pending_events.clear();
    pointer_event_cursor = 0;
    rive_integration::run_on_rive_render_thread([&]() {
        if (state_machine) state_machine->dispose();
        state_machine.reset();
        animation_layers.clear();
        fading_animation.reset();
        animation.reset();
        animation = artboard->animationNamed(name);
        animation->loopValue(animation_loop);
        animation->reset(animation_direction);
        animation->direction(animation_direction);
        animation->addNestedEventListener(this);
        if (view_model_instance) artboard->bindViewModelInstance(view_model_instance);
        animation->advanceAndApply(0.0f);
    });
    emit_signal("changed");
}

void RivePlayer::play_state_machine(const String &p_name) {
    if (!artboard) { requested_state_machine = p_name; requested_animation = ""; return; }

    const std::string name = p_name.utf8().get_data();
    if (!artboard->stateMachine(name)) {
        ERR_PRINT("Rive state machine not found: " + p_name);
        return;
    }
    clear_global_view_models();
    gamepads.clear();
    ++semantics_generation;

    current_state_machine = p_name;
    current_animation = "";
    requested_state_machine = p_name;
    requested_animation = "";
    pointer_positions.clear();
    pending_events.clear();
    pointer_event_cursor = 0;
    rive_integration::run_on_rive_render_thread([&]() {
        if (state_machine) state_machine->dispose();
        state_machine.reset();
        animation_layers.clear();
        fading_animation.reset();
        animation.reset();
        state_machine = artboard->stateMachineNamed(name);
        if (semantics_enabled) state_machine->enableSemantics();
        if (view_model_instance) state_machine->bindViewModelInstance(view_model_instance);
        state_machine->advanceAndApply(0.0f);
        collect_state_machine_events();
    });
    emit_signal("changed");
}

PackedStringArray RivePlayer::get_animation_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            list.push_back(String::utf8(artboard->animation(i)->name().c_str()));
        }
    }
    return list;
}

PackedStringArray RivePlayer::get_state_machine_list() const {
    PackedStringArray list;
    if (artboard) {
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            list.push_back(String::utf8(artboard->stateMachine(i)->name().c_str()));
        }
    }
    return list;
}

Ref<RiveViewModelInstance> RivePlayer::get_rive_view_model_instance() {
    if (wrapper_view_model_instance.is_null() && view_model_instance) {
        wrapper_view_model_instance.instantiate();
        wrapper_view_model_instance->_init(view_model_instance, rive_file);
        connect_view_model_signals(wrapper_view_model_instance, true);
    }
    return wrapper_view_model_instance;
}

void RivePlayer::_on_property_changed(const String &, const Variant &) { emit_signal("changed"); }
void RivePlayer::_on_trigger_fired(const String &) { emit_signal("changed"); }

bool RivePlayer::configure_playback(const String &p_animation, const String &p_state_machine) {
    if (!artboard) {
        requested_animation = p_animation;
        requested_state_machine = p_animation.is_empty() ? p_state_machine : String();
        return true;
    }
    if (!p_animation.is_empty()) {
        if (!artboard->animation(p_animation.utf8().get_data())) return false;
        play_animation(p_animation);
    } else if (!p_state_machine.is_empty()) {
        if (!artboard->stateMachine(p_state_machine.utf8().get_data())) return false;
        play_state_machine(p_state_machine);
    } else {
        requested_animation = "";
        requested_state_machine = "";
        if (artboard->stateMachineCount()) play_state_machine(String::utf8(artboard->stateMachine(0)->name().c_str()));
        else if (artboard->animationCount()) play_animation(String::utf8(artboard->animation(0)->name().c_str()));
        requested_animation = "";
        requested_state_machine = "";
    }
    return true;
}

bool RivePlayer::bind_view_model(const Ref<RiveViewModelInstance> &p_instance) {
    if (!artboard || p_instance.is_null() || !p_instance->_get_native_instance() ||
        p_instance->_get_native_file().get() != rive_file.get()) return false;
    auto native = p_instance->_get_native_instance();
    if (view_model_instance && native->viewModelId() != view_model_instance->viewModelId()) return false;
    bool previous_is_global = false;
    for (const auto &entry : global_view_models) previous_is_global |= entry.value == wrapper_view_model_instance;
    if (!previous_is_global) connect_view_model_signals(wrapper_view_model_instance, false);
    view_model_instance = native;
    wrapper_view_model_instance = p_instance;
    connect_view_model_signals(wrapper_view_model_instance, true);
    if (state_machine) state_machine->bindViewModelInstance(view_model_instance);
    else artboard->bindViewModelInstance(view_model_instance);
    emit_signal("changed");
    return true;
}

Ref<RiveViewModelInstance> RivePlayer::get_global_view_model(const String &p_name) {
    if (!state_machine) return Ref<RiveViewModelInstance>();
    auto native = state_machine->globalViewModelInstance(p_name.utf8().get_data());
    if (!native) return Ref<RiveViewModelInstance>();
    if (const auto *cached = global_view_models.getptr(p_name)) {
        if ((*cached)->_get_native_instance() == native) return *cached;
        bool still_bound = *cached == wrapper_view_model_instance;
        for (const auto &entry : global_view_models) if (entry.key != p_name && entry.value == *cached) still_bound = true;
        if (!still_bound) connect_view_model_signals(*cached, false);
    }
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(native, rive_file);
    connect_view_model_signals(wrapper, true);
    global_view_models[p_name] = wrapper;
    return wrapper;
}

bool RivePlayer::bind_global_view_model(const String &p_name, const Ref<RiveViewModelInstance> &p_instance) {
    if (!state_machine || p_instance.is_null() || !p_instance->_get_native_instance() || p_instance->_get_native_file().get() != rive_file.get()) return false;
    if (!state_machine->setGlobalViewModelInstance(p_name.utf8().get_data(), p_instance->_get_native_instance())) return false;
    state_machine->bind();
    if (auto *previous = global_view_models.getptr(p_name)) {
        bool still_bound = *previous == wrapper_view_model_instance;
        for (const auto &entry : global_view_models) if (entry.key != p_name && entry.value == *previous) still_bound = true;
        if (!still_bound) connect_view_model_signals(*previous, false);
    }
    global_view_models[p_name] = p_instance;
    connect_view_model_signals(p_instance, true);
    emit_signal("changed");
    return true;
}

bool RivePlayer::seek(float p_time) {
    if (!animation || !std::isfinite(p_time) || p_time < 0) return false;
    rive_integration::run_on_rive_render_thread([&]() {
    fading_animation.reset();
    fade_duration = fade_elapsed = 0;
    const int previous_direction = animation->direction() < 0 ? -1 : 1;
    animation->time(p_time);
    animation->direction(previous_direction);
    animation->apply();
    artboard->advance(0);
    animation_completed = false;
    });
    emit_signal("changed");
    return true;
}

void RivePlayer::reset() {
    rive_integration::run_on_rive_render_thread([&]() {
    fading_animation.reset(); fade_duration = fade_elapsed = 0;
    if (state_machine) state_machine->reset();
    else if (animation) { animation->reset(animation_direction); animation->direction(animation_direction); animation->apply(); }
    for (auto &layer : animation_layers) {
        const int direction = layer.initial_direction;
        layer.instance->reset(direction); layer.instance->direction(direction);
        layer.instance->apply(layer.weight);
    }
    animation_completed = false;
    if (artboard) artboard->advance(0);
    });
    emit_signal("changed");
}

float RivePlayer::get_time() const { return animation ? animation->time() : 0; }
float RivePlayer::get_duration() const { return animation ? animation->durationSeconds() : -1; }

rive::SMIInput *RivePlayer::resolve_input(const String &p_name, const String &p_path) const {
    if (!artboard || p_name.is_empty()) return nullptr;
    if (!p_path.is_empty()) return artboard->input(p_name.utf8().get_data(), p_path.utf8().get_data());
    if (state_machine) for (size_t i = 0; i < state_machine->inputCount(); ++i) {
        auto *input = state_machine->input(i);
        if (input && input->name() == p_name.utf8().get_data()) return input;
    }
    return nullptr;
}

bool RivePlayer::set_input_value(const String &p_name, const Variant &p_value, const String &p_path) {
    auto *input = resolve_input(p_name, p_path);
    if (auto *value = dynamic_cast<rive::SMIBool *>(input)) {
        if (p_value.get_type() != Variant::BOOL) return false;
        value->value(p_value);
    } else if (auto *value = dynamic_cast<rive::SMINumber *>(input)) {
        if (p_value.get_type() != Variant::INT && p_value.get_type() != Variant::FLOAT) return false;
        const double number = p_value;
        if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()) return false;
        value->value(static_cast<float>(number));
    } else return false;
    emit_signal("changed");
    return true;
}

Variant RivePlayer::get_input_value(const String &p_name, const String &p_path) const {
    auto *input = resolve_input(p_name, p_path);
    if (auto *value = dynamic_cast<rive::SMIBool *>(input)) return value->value();
    if (auto *value = dynamic_cast<rive::SMINumber *>(input)) return value->value();
    return Variant();
}

bool RivePlayer::fire_input(const String &p_name, const String &p_path) {
    auto *trigger = dynamic_cast<rive::SMITrigger *>(resolve_input(p_name, p_path));
    if (!trigger) return false;
    trigger->fire();
    emit_signal("changed");
    return true;
}

Array RivePlayer::get_input_list(const String &p_path) const {
    Array inputs;
    std::vector<rive::SMIInput *> native_inputs;
    if (p_path.is_empty()) {
        if (state_machine) for (size_t i = 0; i < state_machine->inputCount(); ++i) native_inputs.push_back(state_machine->input(i));
    } else if (artboard) {
        auto *nested = artboard->nestedArtboardAtPath(p_path.utf8().get_data());
        if (nested) for (auto *item : nested->nestedAnimations()) {
            if (!item->is<rive::NestedStateMachine>()) continue;
            auto *machine = item->as<rive::NestedStateMachine>();
            for (size_t i = 0; i < machine->inputCount(); ++i) {
                auto *input = machine->input(i);
                if (input && input->input()) native_inputs.push_back(input->input());
            }
        }
    }
    for (auto *input : native_inputs) {
        if (!input) continue;
        Dictionary info;
        const String name = String::utf8(input->name().c_str());
        info["name"] = name;
        info["type"] = dynamic_cast<rive::SMIBool *>(input) ? "bool" : dynamic_cast<rive::SMINumber *>(input) ? "number" : "trigger";
        info["value"] = get_input_value(name, p_path);
        info["path"] = p_path;
        inputs.push_back(info);
    }
    return inputs;
}

void RivePlayer::set_volume(double p_volume) {
    if (!std::isfinite(p_volume) || p_volume < 0.0 || p_volume > 1.0) return;
    volume = p_volume;
    rive_integration::run_on_rive_render_thread([&]() { if (artboard) artboard->volume(static_cast<float>(volume)); });
    emit_signal("changed");
}

bool RivePlayer::set_animation_loop(int p_loop) {
    if (p_loop < -1 || p_loop > 2) return false;
    animation_loop = p_loop;
    rive_integration::run_on_rive_render_thread([&]() { if (animation) animation->loopValue(p_loop); });
    animation_completed = false;
    emit_signal("changed");
    return true;
}

bool RivePlayer::set_animation_direction(int p_direction) {
    if (p_direction != -1 && p_direction != 1) return false;
    animation_direction = p_direction;
    rive_integration::run_on_rive_render_thread([&]() { if (animation) animation->direction(p_direction); });
    animation_completed = false;
    emit_signal("changed");
    return true;
}

bool RivePlayer::set_text_run(const String &p_name, const String &p_value, const String &p_path) {
    if (!artboard) return false;
    auto *run = artboard->getTextRun(p_name.utf8().get_data(), p_path.utf8().get_data());
    if (!run) return false;
    run->text(p_value.utf8().get_data());
    emit_signal("changed");
    return true;
}

String RivePlayer::get_text_run(const String &p_name, const String &p_path) const {
    if (!artboard) return String();
    auto *run = artboard->getTextRun(p_name.utf8().get_data(), p_path.utf8().get_data());
    return run ? String::utf8(run->text().c_str()) : String();
}

bool RivePlayer::pointer_exit(int pointer_id) {
    const Vector2 *point = pointer_positions.getptr(pointer_id);
    if (!state_machine || !point) return false;
    bool handled = state_machine->pointerExit(rive::Vec2D(point->x, point->y), pointer_id) != rive::HitResult::none;
    collect_state_machine_events();
    pointer_positions.erase(pointer_id);
    emit_signal("changed");
    return handled;
}

bool RivePlayer::scroll(Vector2 position, Vector2 delta, const rive::Mat2D &transform) {
    if (!state_machine) return false;
    rive::Mat2D inverse;
    if (!transform.invert(&inverse)) return false;
    const rive::Vec2D point = inverse * rive::Vec2D(position.x, position.y);
    const rive::Vec2D offset = inverse * rive::Vec2D(position.x + delta.x, position.y + delta.y);
    rive::ScrollEvent event;
    event.delta = offset - point;
    bool handled = state_machine->pointerScroll(point, event) != rive::HitResult::none;
    collect_state_machine_events();
    if (handled) emit_signal("changed");
    return handled;
}

bool RivePlayer::focus_next() {
    bool handled = state_machine && state_machine->focusNext();
    if (handled) emit_signal("changed");
    return handled;
}
bool RivePlayer::focus_previous() {
    bool handled = state_machine && state_machine->focusPrevious();
    if (handled) emit_signal("changed");
    return handled;
}
bool RivePlayer::focus_direction(int direction) {
    if (!state_machine) return false;
    bool handled = false;
    switch (direction) {
        case 0: handled = state_machine->focusLeft(); break;
        case 1: handled = state_machine->focusRight(); break;
        case 2: handled = state_machine->focusUp(); break;
        case 3: handled = state_machine->focusDown(); break;
    }
    if (handled) emit_signal("changed");
    return handled;
}
void RivePlayer::clear_focus() {
    if (state_machine) { state_machine->clearFocus(); emit_signal("changed"); }
}

bool RivePlayer::key_input(const Ref<InputEventKey> &event) {
    if ((!state_machine && !text_selection) || event.is_null()) return false;
    const godot::Key code = event->get_keycode();
    uint16_t key = 0;
    if (code >= godot::KEY_SPACE && code <= godot::KEY_QUOTELEFT) key = static_cast<uint16_t>(code);
    else if (code >= godot::KEY_F1 && code <= godot::KEY_F24) key = 290 + (code - godot::KEY_F1);
    else if (code >= godot::KEY_KP_0 && code <= godot::KEY_KP_9) key = 320 + (code - godot::KEY_KP_0);
    else switch (code) {
        case godot::KEY_ESCAPE: key = 256; break;
        case godot::KEY_ENTER: key = 257; break;
        case godot::KEY_TAB: key = 258; break;
        case godot::KEY_BACKSPACE: key = 259; break;
        case godot::KEY_INSERT: key = 260; break;
        case godot::KEY_DELETE: key = 261; break;
        case godot::KEY_RIGHT: key = 262; break;
        case godot::KEY_LEFT: key = 263; break;
        case godot::KEY_DOWN: key = 264; break;
        case godot::KEY_UP: key = 265; break;
        case godot::KEY_PAGEUP: key = 266; break;
        case godot::KEY_PAGEDOWN: key = 267; break;
        case godot::KEY_HOME: key = 268; break;
        case godot::KEY_END: key = 269; break;
        case godot::KEY_CAPSLOCK: key = 280; break;
        case godot::KEY_SCROLLLOCK: key = 281; break;
        case godot::KEY_NUMLOCK: key = 282; break;
        case godot::KEY_PRINT: key = 283; break;
        case godot::KEY_PAUSE: key = 284; break;
        case godot::KEY_KP_PERIOD: key = 330; break;
        case godot::KEY_KP_DIVIDE: key = 331; break;
        case godot::KEY_KP_MULTIPLY: key = 332; break;
        case godot::KEY_KP_SUBTRACT: key = 333; break;
        case godot::KEY_KP_ADD: key = 334; break;
        case godot::KEY_KP_ENTER: key = 335; break;
        case godot::KEY_SHIFT: key = 340; break;
        case godot::KEY_CTRL: key = 341; break;
        case godot::KEY_ALT: key = 342; break;
        case godot::KEY_META: key = 343; break;
        case godot::KEY_MENU: key = 348; break;
        default: return false;
    }
    rive::KeyModifiers mods = rive::KeyModifiers::none;
    if (event->is_shift_pressed()) mods = mods | rive::KeyModifiers::shift;
    if (event->is_ctrl_pressed()) mods = mods | rive::KeyModifiers::ctrl;
    if (event->is_alt_pressed()) mods = mods | rive::KeyModifiers::alt;
    if (event->is_meta_pressed()) mods = mods | rive::KeyModifiers::meta;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        if (text_selection && (!state_machine || !state_machine->focusState().expectsKeyboardInput))
            handled = text_selection->keyInput(static_cast<rive::Key>(key), mods, event->is_pressed());
        if (!handled && state_machine) handled = state_machine->keyInput(static_cast<rive::Key>(key), mods, event->is_pressed(), event->is_echo());
    });
    collect_state_machine_events();
    if (handled) emit_signal("changed");
    return handled;
}

bool RivePlayer::text_input(const String &text) {
    bool handled = state_machine && state_machine->textInput(text.utf8().get_data());
    collect_state_machine_events();
    if (handled) emit_signal("changed");
    return handled;
}
String RivePlayer::get_selected_text() const {
    if (state_machine && state_machine->focusState().expectsKeyboardInput)
        return String::utf8(state_machine->selectedText().c_str());
    if (text_selection && text_selection->hasSelection()) return String::utf8(text_selection->selectedText().c_str());
    return state_machine ? String::utf8(state_machine->selectedText().c_str()) : String();
}

namespace {
bool event_source_path(rive::Artboard *board, rive::Artboard *source, const String &prefix, String &out, int depth, int &budget) {
    if (!board || depth > 32 || --budget < 0) return false;
    if (board == source) { out = prefix; return true; }
    for (auto *nested : board->nestedArtboards()) {
        const String path = prefix + String(prefix.is_empty() ? "" : "/") + String::utf8(nested->name().c_str());
        if (event_source_path(nested->artboardInstance(), source, path, out, depth + 1, budget)) return true;
    }
    for (auto *list : board->artboardComponentLists()) {
        for (size_t i = 0; i < list->artboardCount(); ++i) {
            const String path = prefix + String(prefix.is_empty() ? "" : "/") + String::utf8(list->name().c_str()) + "[" + String::num_int64(i) + "]";
            if (event_source_path(list->artboardInstance(i), source, path, out, depth + 1, budget)) return true;
        }
    }
    return false;
}
}

void RivePlayer::notify(const std::vector<rive::EventReport> &p_events, rive::NestedArtboard *p_context) {
    for (const auto &report : p_events) {
        auto *event = report.event();
        if (!event) continue;
        Dictionary properties;
        for (auto *property : event->customProperties()) {
            Variant value;
            if (property->is<rive::CustomPropertyNumber>()) value = property->as<rive::CustomPropertyNumber>()->propertyValue();
            else if (property->is<rive::CustomPropertyBoolean>()) value = property->as<rive::CustomPropertyBoolean>()->propertyValue();
            else if (property->is<rive::CustomPropertyString>()) value = String::utf8(property->as<rive::CustomPropertyString>()->propertyValue().c_str());
            else if (property->is<rive::CustomPropertyColor>()) {
                uint32_t color = property->as<rive::CustomPropertyColor>()->propertyValue();
                value = Color(((color >> 16) & 255) / 255.0, ((color >> 8) & 255) / 255.0, (color & 255) / 255.0, (color >> 24) / 255.0);
            } else if (property->is<rive::CustomPropertyEnum>()) value = static_cast<int64_t>(property->as<rive::CustomPropertyEnum>()->propertyValue());
            else continue;
            properties[String::utf8(property->name().c_str())] = value;
        }
        Dictionary payload;
        payload["properties"] = properties;
        payload["delay_seconds"] = report.secondsDelay();
        payload["type"] = event->is<rive::OpenUrlEvent>() ? "open_url" : "general";
        auto *source = event->artboard();
        String source_path; int budget = 4096;
        const bool source_known = event_source_path(artboard.get(), source, "", source_path, 0, budget);
        payload["source_artboard"] = source ? String::utf8(source->name().c_str()) : String();
        payload["source_path"] = source_path;
        payload["source_path_known"] = source_known;
        payload["nested_context"] = p_context ? String::utf8(p_context->name().c_str()) : String();
        if (event->is<rive::OpenUrlEvent>()) {
            const auto *url_event = event->as<rive::OpenUrlEvent>();
            payload["url"] = String::utf8(url_event->url().c_str());
            payload["target"] = static_cast<int64_t>(url_event->targetValue());
        }
        Dictionary entry;
        entry["name"] = String::utf8(event->name().c_str());
        entry["payload"] = payload;
        pending_events.push_back(entry);
    }
}

void RivePlayer::collect_state_machine_events() {
    if (!state_machine) return;
    const size_t count = state_machine->reportedEventCount();
    if (count < pointer_event_cursor) pointer_event_cursor = 0;
    for (size_t i = pointer_event_cursor; i < count; ++i) notify({state_machine->reportedEventAt(i)}, nullptr);
    pointer_event_cursor = count;
}

void RivePlayer::set_semantics_enabled(bool p_enabled) {
    semantics_enabled = p_enabled;
    rive_integration::run_on_rive_render_thread([&]() { if (p_enabled && state_machine) state_machine->enableSemantics(); });
}

static Array rive_semantic_nodes(const std::vector<rive::SemanticsDiffNode> &p_nodes) {
    Array nodes;
    for (const auto &node : p_nodes) {
        Dictionary data;
        data["id"] = static_cast<int64_t>(node.id);
        data["parent_id"] = node.parentId;
        data["sibling_index"] = static_cast<int64_t>(node.siblingIndex);
        data["role"] = static_cast<int64_t>(node.role);
        data["label"] = String::utf8(node.label.c_str());
        data["value"] = String::utf8(node.value.c_str());
        data["hint"] = String::utf8(node.hint.c_str());
        data["state_flags"] = static_cast<int64_t>(node.stateFlags);
        data["trait_flags"] = static_cast<int64_t>(node.traitFlags);
        data["heading_level"] = static_cast<int64_t>(node.headingLevel);
        data["bounds"] = Rect2(node.minX, node.minY, node.maxX - node.minX, node.maxY - node.minY);
        nodes.push_back(data);
    }
    return nodes;
}

Dictionary RivePlayer::take_semantics_diff() {
    Dictionary result;
    rive_integration::run_on_rive_render_thread([&]() {
    if (!semantics_enabled || !state_machine || !state_machine->semanticManager()) return;
    const auto diff = state_machine->semanticManager()->drainDiff();
    result["generation"] = static_cast<int64_t>(semantics_generation);
    result["tree_version"] = static_cast<int64_t>(diff.treeVersion);
    result["root_id"] = static_cast<int64_t>(diff.rootId);
    result["added"] = rive_semantic_nodes(diff.added);
    result["moved"] = rive_semantic_nodes(diff.moved);
    result["updated"] = rive_semantic_nodes(diff.updatedSemantic);
    PackedInt64Array removed;
    for (auto id : diff.removed) removed.push_back(id);
    result["removed"] = removed;
    Array bounds;
    for (const auto &node : diff.updatedGeometry) {
        Dictionary data;
        data["id"] = static_cast<int64_t>(node.id);
        data["bounds"] = Rect2(node.minX, node.minY, node.maxX - node.minX, node.maxY - node.minY);
        bounds.push_back(data);
    }
    result["bounds_updated"] = bounds;
    Array children;
    for (const auto &update : diff.childrenUpdated) {
        Dictionary data;
        data["parent_id"] = update.parentId;
        PackedInt64Array ids;
        for (auto id : update.childIds) ids.push_back(id);
        data["children"] = ids;
        children.push_back(data);
    }
    result["children_updated"] = children;
    });
    return result;
}

bool RivePlayer::request_semantic_focus(int64_t p_id) {
    if (!semantics_enabled || !state_machine || p_id <= 0 || p_id > UINT32_MAX || !state_machine->semanticManager()) return false;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        handled = state_machine->semanticManager()->requestFocus(static_cast<uint32_t>(p_id));
    });
    if (handled) emit_signal("changed");
    return handled;
}

Dictionary RivePlayer::get_capabilities() {
    Dictionary result;
    result["runtime_version"] = "0.1.502";
    result["layout"] = true;
    result["text"] = true;
#ifdef WITH_RIVE_AUDIO
    result["audio"] = true;
#else
    result["audio"] = false;
#endif
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    result["audio_backend"] = "godot";
#else
    result["audio_backend"] = "miniaudio_device";
#endif
#ifdef WITH_RIVE_SCRIPTING_LUAU
    result["scripting"] = true;
    result["scripting_backend"] = "luau";
#else
    result["scripting"] = false;
    result["scripting_backend"] = "disabled";
#endif
#ifdef RIVE_GODOT_SCRIPT_VERIFICATION_PRODUCTION
    result["script_verification"] = "production_signed";
#else
    result["script_verification"] = "disabled";
#endif
    result["semantic_data"] = true;
    result["platform_accessibility_bridge"] = false;
    result["accessibility_bridge_helper"] = "RiveAccessibilityBridge";
    result["ime_composition"] = true;
    result["gamepad"] = true;
    result["animation_layers"] = true;
    result["text_selection"] = true;
    return result;
}

