#include "rive_file_instance.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/viewport.hpp>

#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>

void RiveFileInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_file", "file"), &RiveFileInstance::set_rive_file);
    ClassDB::bind_method(D_METHOD("get_rive_file"), &RiveFileInstance::get_rive_file);
    ClassDB::bind_method(D_METHOD("_on_rive_file_changed"), &RiveFileInstance::_on_rive_file_changed);
    
    ClassDB::bind_method(D_METHOD("set_artboard_name", "name"), &RiveFileInstance::set_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_name"), &RiveFileInstance::get_artboard_name);
    
    ClassDB::bind_method(D_METHOD("set_state_machine_name", "name"), &RiveFileInstance::set_state_machine_name);
    ClassDB::bind_method(D_METHOD("get_state_machine_name"), &RiveFileInstance::get_state_machine_name);
    
    ClassDB::bind_method(D_METHOD("set_animation_name", "name"), &RiveFileInstance::set_animation_name);
    ClassDB::bind_method(D_METHOD("get_animation_name"), &RiveFileInstance::get_animation_name);
    
    ClassDB::bind_method(D_METHOD("set_auto_play", "auto_play"), &RiveFileInstance::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &RiveFileInstance::get_auto_play);
    ClassDB::bind_method(D_METHOD("get_view_model_instance"), &RiveFileInstance::get_view_model_instance);
    ClassDB::bind_method(D_METHOD("get_rive_player"), &RiveFileInstance::get_rive_player);
    ClassDB::bind_method(D_METHOD("hit_test", "point"), &RiveFileInstance::hit_test);
    ClassDB::bind_method(D_METHOD("pointer_down", "position"), &RiveFileInstance::pointer_down);
    ClassDB::bind_method(D_METHOD("pointer_up", "position"), &RiveFileInstance::pointer_up);
    ClassDB::bind_method(D_METHOD("pointer_move", "position"), &RiveFileInstance::pointer_move);
    ClassDB::bind_method(D_METHOD("pointer_down_with_id", "position", "pointer_id"), &RiveFileInstance::pointer_down_with_id, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("pointer_up_with_id", "position", "pointer_id"), &RiveFileInstance::pointer_up_with_id, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("pointer_move_with_id", "position", "pointer_id"), &RiveFileInstance::pointer_move_with_id, DEFVAL(0));

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
}

RiveFileInstance::RiveFileInstance() {
    rive_player.instantiate();
    connect_rive_player_signals(rive_player.ptr());
}

RiveFileInstance::~RiveFileInstance() {
    if (rive_file_resource.is_valid()) {
        const Callable changed_callback(this, "_on_rive_file_changed");
        if (rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->disconnect("changed", changed_callback);
        }
    }
    connect_rive_player_signals(rive_player.ptr(), false);
    rive_player.unref();
    rive_file_resource.unref();
}

void RiveFileInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
            set_process_input(false);
            set_process_unhandled_input(true);
            update_configuration_warnings();
            break;
        case NOTIFICATION_VISIBILITY_CHANGED:
            request_rive_update();
            break;
    }
}

void RiveFileInstance::set_rive_file(const Ref<RiveFile> &p_file) {
    if (rive_file_resource == p_file) return;

    const Callable changed_callback(this, "_on_rive_file_changed");
    Ref<RiveFile> previous_file = rive_file_resource;
    if (previous_file.is_valid() && previous_file->is_connected("changed", changed_callback)) {
        previous_file->disconnect("changed", changed_callback);
    }

    rive_file_resource = p_file;
    if (rive_file_resource.is_valid() && !rive_file_resource->is_connected("changed", changed_callback)) {
        rive_file_resource->connect("changed", changed_callback);
    }

    if (!_load_artboard()) {
        if (rive_file_resource.is_valid() && rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->disconnect("changed", changed_callback);
        }
        rive_file_resource = previous_file;
        if (rive_file_resource.is_valid() && !rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->connect("changed", changed_callback);
        }
    }
    update_configuration_warnings();
    request_rive_update();
}

void RiveFileInstance::_on_rive_file_changed() {
    if (!_load_artboard()) {
        UtilityFunctions::printerr("RiveFileInstance: resource reload failed; keeping the previous playable instance.");
    }
    update_configuration_warnings();
    request_rive_update();
}

Ref<RiveFile> RiveFileInstance::get_rive_file() const {
    return rive_file_resource;
}

void RiveFileInstance::set_artboard_name(const String &p_name) {
    if (artboard_name == p_name) return;
    String previous_name = artboard_name;
    artboard_name = p_name;
    if (!_load_artboard()) {
        artboard_name = previous_name;
    }
    update_configuration_warnings();
    request_rive_update();
}

String RiveFileInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveFileInstance::set_state_machine_name(const String &p_name) {
    if (state_machine_name == p_name && (p_name.is_empty() || animation_name.is_empty())) return;
    String previous_name = state_machine_name;
    String previous_animation = animation_name;
    state_machine_name = p_name;
    if (!p_name.is_empty()) animation_name = String();
    if (!_load_artboard()) {
        state_machine_name = previous_name;
        animation_name = previous_animation;
    }
    update_configuration_warnings();
    request_rive_update();
}

String RiveFileInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveFileInstance::set_animation_name(const String &p_name) {
    if (animation_name == p_name && (p_name.is_empty() || state_machine_name.is_empty())) return;
    String previous_name = animation_name;
    String previous_state_machine = state_machine_name;
    animation_name = p_name;
    if (!p_name.is_empty()) state_machine_name = String();
    if (!_load_artboard()) {
        animation_name = previous_name;
        state_machine_name = previous_state_machine;
    }
    update_configuration_warnings();
    request_rive_update();
}

String RiveFileInstance::get_animation_name() const {
    return animation_name;
}

void RiveFileInstance::set_auto_play(bool p_auto) {
    set_playing(p_auto);
}

bool RiveFileInstance::get_auto_play() const {
    return is_playing();
}

bool RiveFileInstance::_load_artboard() {
    if (!rive_player.is_valid()) return false;
    if (rive_file_resource.is_null()) {
        rive_player->set_artboard(nullptr);
        load_error = String();
        pressed_pointer_ids.clear();
        emit_signal("view_model_changed");
        return true;
    }
    
    std::unique_ptr<rive::ArtboardInstance> artboard = rive_file_resource->instantiate_artboard(artboard_name);

    if (!artboard) {
        load_error = "Unable to load artboard '" + artboard_name + "'.";
        UtilityFunctions::printerr("RiveFileInstance: failed to instantiate artboard '" + artboard_name + "'.");
        return false;
    }

    rive::rcp<rive::File> file = rive::ref_rcp(rive_file_resource->get_rive_file());
    if (!animation_name.is_empty()) {
        if (!artboard->animation(animation_name.utf8().get_data())) {
            load_error = "Animation not found: " + animation_name;
            UtilityFunctions::printerr("RiveFileInstance: " + load_error);
            return false;
        }
    } else if (!state_machine_name.is_empty()) {
        if (!artboard->stateMachine(state_machine_name.utf8().get_data())) {
            load_error = "State machine not found: " + state_machine_name;
            UtilityFunctions::printerr("RiveFileInstance: " + load_error);
            return false;
        }
    }

    Ref<RivePlayer> next_player;
    next_player.instantiate();
    next_player->configure_playback(animation_name, state_machine_name);
    next_player->set_artboard(std::move(artboard), file);
    connect_rive_player_signals(rive_player.ptr(), false);
    rive_player = next_player;
    connect_rive_player_signals(rive_player.ptr());
    load_error = String();
    pressed_pointer_ids.clear();
    future_active = true;
    request_rive_update();
    emit_signal("view_model_changed");
    return true;
}

void RiveFileInstance::advance(double delta) {
    _advance_player(delta, false);
}

void RiveFileInstance::advance_rive_automatically(double delta) {
    if (get_update_mode() != UPDATE_MANUAL) _advance_player(delta, true);
}

void RiveFileInstance::_advance_player(double delta, bool p_automatic) {
    if (!rive_player.is_valid()) return;
    ERR_FAIL_COND_MSG(!Math::is_finite(delta) || delta < 0.0, "Rive advance delta must be finite and non-negative.");
    const Ref<RivePlayer> player = rive_player;
    const ObjectID self_id(get_instance_id());
    const bool was_active = future_active;
    const bool playing = !p_automatic || is_playing();
    const bool requested = is_rive_update_requested();
    bool active = false;
    bool changed = false;
    if (playing) {
        active = player->advance(delta * get_speed_scale());
        changed = active || was_active || !p_automatic;
    } else {
        changed = player->flush_view_model_changes() || requested;
        if (changed) player->advance(0.0f);
    }
    if (RiveFileInstance *instance = Object::cast_to<RiveFileInstance>(ObjectDB::get_instance((uint64_t)self_id))) {
        if (playing) instance->future_active = active;
        if (changed) instance->request_rive_update();
    }
}

void RiveFileInstance::draw(rive::Renderer *renderer) {
    if (!rive_player.is_valid()) return;
    
    renderer->save();
    
    Transform2D xform = get_transform();
    rive::Mat2D rive_transform(
        xform.columns[0].x, xform.columns[0].y,
        xform.columns[1].x, xform.columns[1].y,
        xform.columns[2].x, xform.columns[2].y
    );
    
    rive_player->draw(renderer, rive_transform);
    
    renderer->restore();
}

Rect2 RiveFileInstance::get_rive_bounds() const {
    if (rive_player.is_valid() && rive_player->get_artboard()) {
        rive::AABB aabb = rive_player->get_artboard()->bounds();
        return Rect2(aabb.minX, aabb.minY, aabb.width(), aabb.height());
    }
    const Rect2 editor_selection_bounds(-32, -32, 64, 64);
    return editor_selection_bounds;
}

void RiveFileInstance::_draw() {
    if (Engine::get_singleton()->is_editor_hint()) {
        draw_rect(get_rive_bounds(), Color(1, 1, 1, 0.01));
    }
}

void RiveFileInstance::_input(const Ref<InputEvent> &p_event) {
    if (p_event.is_null()) return;
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!can_advance_rive() || rive_player.is_null()) return;
    if (Math::is_zero_approx(get_global_transform_with_canvas().determinant())) return;
    const Ref<RivePlayer> player = rive_player;
    const ObjectID viewport_id(get_viewport()->get_instance_id());
    const Ref<InputEvent> local_event = make_input_local(p_event);
    const Transform2D xform = get_transform();
    bool handled = false;
    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid()) {
        if (mb->get_device() == InputEvent::DEVICE_ID_EMULATION) return;
        Ref<InputEventMouseButton> local_mb = local_event;
        if (mb->get_button_index() == MOUSE_BUTTON_LEFT) {
            handled = _dispatch_pointer(xform.xform(local_mb->get_position()), mb->is_pressed() ? 0 : 1, 0);
        } else if (mb->is_pressed()) {
            Vector2 scroll;
            switch (mb->get_button_index()) {
                case MOUSE_BUTTON_WHEEL_UP: scroll.y = -mb->get_factor(); break;
                case MOUSE_BUTTON_WHEEL_DOWN: scroll.y = mb->get_factor(); break;
                case MOUSE_BUTTON_WHEEL_LEFT: scroll.x = -mb->get_factor(); break;
                case MOUSE_BUTTON_WHEEL_RIGHT: scroll.x = mb->get_factor(); break;
                default: break;
            }
            if (scroll != Vector2()) handled = player->scroll(local_mb->get_position(), scroll, rive::Mat2D());
        }
    } else {
        Ref<InputEventMouseMotion> mm = p_event;
        Ref<InputEventScreenTouch> touch = p_event;
        Ref<InputEventScreenDrag> drag = p_event;
        Ref<InputEventKey> key = p_event;
        if (mm.is_valid()) {
            if (mm->get_device() == InputEvent::DEVICE_ID_EMULATION) return;
            Ref<InputEventMouseMotion> local_mm = local_event;
            handled = _dispatch_pointer(xform.xform(local_mm->get_position()), 2, 0);
        } else if (touch.is_valid()) {
            if (touch->get_index() < 0 || touch->get_index() == INT32_MAX) return;
            const int id = touch->get_index() + 1;
            Ref<InputEventScreenTouch> local_touch = local_event;
            if (touch->is_canceled()) {
                handled = pressed_pointer_ids.erase(id) != 0;
                handled = player->pointer_exit(id) || handled;
            } else {
                handled = _dispatch_pointer(xform.xform(local_touch->get_position()), touch->is_pressed() ? 0 : 1, id);
            }
        } else if (drag.is_valid()) {
            if (drag->get_index() < 0 || drag->get_index() == INT32_MAX) return;
            Ref<InputEventScreenDrag> local_drag = local_event;
            handled = _dispatch_pointer(xform.xform(local_drag->get_position()), 2, drag->get_index() + 1);
        } else if (key.is_valid()) {
            handled = player->key_input(key);
        }
    }
    if (handled) {
        if (Viewport *viewport = Object::cast_to<Viewport>(ObjectDB::get_instance((uint64_t)viewport_id))) {
            viewport->set_input_as_handled();
        }
    }
}

void RiveFileInstance::_unhandled_input(const Ref<InputEvent> &p_event) {
    _input(p_event);
}

bool RiveFileInstance::_dispatch_pointer(Vector2 p_position, int p_kind, int p_pointer_id) {
    if (rive_player.is_null() || p_pointer_id < 0) return false;
    const Ref<RivePlayer> player = rive_player;
    const Transform2D xform = get_transform();
    const rive::Mat2D transform(
        xform.columns[0].x, xform.columns[0].y,
        xform.columns[1].x, xform.columns[1].y,
        xform.columns[2].x, xform.columns[2].y);
    const bool captured = pressed_pointer_ids.count(p_pointer_id) != 0;
    if (p_kind == 1) pressed_pointer_ids.erase(p_pointer_id);
    const ObjectID self_id(get_instance_id());
    bool handled = false;
    if (p_kind == 0) handled = player->pointer_down(p_position, transform, p_pointer_id);
    else if (p_kind == 1) handled = player->pointer_up(p_position, transform, p_pointer_id);
    else handled = player->pointer_move(p_position, transform, p_pointer_id);
    if (p_kind == 0 && handled) {
        if (RiveFileInstance *instance = Object::cast_to<RiveFileInstance>(ObjectDB::get_instance((uint64_t)self_id))) {
            instance->pressed_pointer_ids.insert(p_pointer_id);
        }
    }
    return handled || captured;
}

PackedStringArray RiveFileInstance::_get_configuration_warnings() const {
    PackedStringArray warnings = RiveNode::_get_configuration_warnings();
    if (rive_file_resource.is_null()) warnings.push_back("Assign a RiveFile resource.");
    if (!animation_name.is_empty() && !state_machine_name.is_empty()) {
        warnings.push_back("animation_name takes precedence over state_machine_name. Clear animation_name to play the state machine.");
    }
    if (!load_error.is_empty()) warnings.push_back(load_error);
    return warnings;
}

bool RiveFileInstance::hit_test(Vector2 point) {
    if (rive_player.is_valid()) {
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        return rive_player->hit_test(point, rive_transform);
    }
    return false;
}

void RiveFileInstance::pointer_down(Vector2 position) {
    pointer_down_with_id(position, 0);
}

void RiveFileInstance::pointer_up(Vector2 position) {
    pointer_up_with_id(position, 0);
}

void RiveFileInstance::pointer_move(Vector2 position) {
    pointer_move_with_id(position, 0);
}

bool RiveFileInstance::pointer_down_with_id(Vector2 p_position, int p_pointer_id) {
    return _dispatch_pointer(p_position, 0, p_pointer_id);
}

bool RiveFileInstance::pointer_up_with_id(Vector2 p_position, int p_pointer_id) {
    return _dispatch_pointer(p_position, 1, p_pointer_id);
}

bool RiveFileInstance::pointer_move_with_id(Vector2 p_position, int p_pointer_id) {
    return _dispatch_pointer(p_position, 2, p_pointer_id);
}

Ref<RiveViewModelInstance> RiveFileInstance::get_view_model_instance() const {
    if (rive_player.is_valid()) {
        return rive_player->get_rive_view_model_instance();
    }
    return nullptr;
}
