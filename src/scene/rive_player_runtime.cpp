#include "rive_player.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/classes/input_event_joypad_button.hpp>
#include <godot_cpp/classes/input_event_joypad_motion.hpp>
#include <rive/animation/semantic_listener_group.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/input/gamepad_batch.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/joystick.hpp>
#include <rive/bones/bone.hpp>
#include <rive/world_transform_component.hpp>
#include <cmath>

namespace {
rive::Artboard *resolve_board(rive::ArtboardInstance *root, const String &path) {
    if (!root || path.length() > 4096) return nullptr;
    if (path.is_empty()) return root;
    auto *nested = root->nestedArtboardAtPath(path.utf8().get_data());
    return nested ? nested->artboardInstance() : nullptr;
}
Transform2D convert_transform(const rive::Mat2D &value) {
    return Transform2D(value[0], value[1], value[2], value[3], value[4], value[5]);
}
Vector2 root_point(rive::Artboard *board, const rive::Vec2D &point) {
    const auto result = board->rootTransform(point);
    return Vector2(result.x, result.y);
}
Dictionary describe_component(rive::Component *component) {
    Dictionary result;
    result["name"] = String::utf8(component->name().c_str());
    result["native_type"] = static_cast<int>(component->coreType());
    if (auto *world = dynamic_cast<rive::WorldTransformComponent *>(component)) {
        result["world_transform"] = convert_transform(world->worldTransform());
        auto *board = component->artboard();
        if (board) {
            const auto &matrix = world->worldTransform();
            const auto origin = root_point(board, matrix * rive::Vec2D(0, 0));
            result["root_transform"] = Transform2D(root_point(board, matrix * rive::Vec2D(1, 0)) - origin,
                    root_point(board, matrix * rive::Vec2D(0, 1)) - origin, origin);
        }
    }
    if (auto *transform = dynamic_cast<rive::TransformComponent *>(component)) {
        result["transform"] = convert_transform(transform->transform());
        result["opacity"] = transform->renderOpacity();
    }
    result["kind"] = "component";
    if (component->is<rive::Bone>()) {
        result["kind"] = "bone";
        const auto tip = component->as<rive::Bone>()->tipWorldTranslation();
        result["tip_position"] = Vector2(tip.x, tip.y);
        if (component->artboard()) result["root_tip_position"] = root_point(component->artboard(), tip);
    } else if (component->is<rive::Joystick>()) {
        result["kind"] = "joystick";
        const auto *stick = component->as<rive::Joystick>();
        result["value"] = Vector2(stick->x(), stick->y());
    }
    return result;
}
}

Array RivePlayer::get_components(const String &p_path) const {
    Array result;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *board = resolve_board(artboard.get(), p_path);
        if (board) for (auto *component : board->find<rive::Component>()) result.push_back(describe_component(component));
    });
    return result;
}

Dictionary RivePlayer::get_component(const String &p_name, const String &p_path) const {
    Dictionary result;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *board = resolve_board(artboard.get(), p_path);
        auto *component = board ? board->find(p_name.utf8().get_data()) : nullptr;
        if (component) result = describe_component(component);
    });
    return result;
}

bool RivePlayer::set_joystick(const String &p_name, Vector2 p_value, const String &p_path) {
    if (!p_value.is_finite() || p_value.x < -1 || p_value.x > 1 || p_value.y < -1 || p_value.y > 1) return false;
    bool changed = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *board = resolve_board(artboard.get(), p_path);
        auto *stick = board ? board->find<rive::Joystick>(p_name.utf8().get_data()) : nullptr;
        if (!stick) return;
        stick->x(p_value.x);
        stick->y(p_value.y);
        changed = true;
    });
    if (changed) emit_signal("changed");
    return changed;
}

bool RivePlayer::fire_semantic_action(int64_t p_id, int p_action) {
    if (!semantics_enabled || !state_machine || p_id <= 0 || p_id > UINT32_MAX ||
            p_action < SEMANTIC_TAP || p_action > SEMANTIC_DECREASE) return false;
    bool found = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *manager = state_machine->semanticManager();
        if (!manager || !manager->nodeById(static_cast<uint32_t>(p_id))) return;
        state_machine->fireSemanticAction(static_cast<uint32_t>(p_id), static_cast<rive::SemanticActionType>(p_action));
        found = true;
    });
    if (found) emit_signal("changed");
    return found;
}

bool RivePlayer::send_pointer(Vector2 p_position, int p_phase, int p_pointer_id, int p_button, double p_timestamp) {
    if (!state_machine || !p_position.is_finite() || p_pointer_id < 0 || p_button < 0 || p_button > 2 ||
            p_phase < POINTER_DOWN || p_phase > POINTER_EXIT || !std::isfinite(p_timestamp) || p_timestamp < 0 || p_timestamp > 1e12) return false;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        const rive::Vec2D point(p_position.x, p_position.y);
        const auto button = static_cast<rive::PointerButton>(p_button);
        rive::HitResult result = rive::HitResult::none;
        switch (p_phase) {
            case POINTER_DOWN: result = state_machine->pointerDown(point, p_pointer_id, button); break;
            case POINTER_UP: result = state_machine->pointerUp(point, p_pointer_id, button); break;
            case POINTER_MOVE: result = state_machine->pointerMove(point, p_timestamp, p_pointer_id); break;
            case POINTER_DRAG_START: result = state_machine->dragStart(point, p_timestamp, true, p_pointer_id, button); break;
            case POINTER_DRAG_END: result = state_machine->dragEnd(point, p_timestamp, p_pointer_id, button); break;
            case POINTER_EXIT: result = state_machine->pointerExit(point, p_pointer_id); break;
        }
        pointer_positions[p_pointer_id] = p_position;
        if (p_phase == POINTER_EXIT) pointer_positions.erase(p_pointer_id);
        collect_state_machine_events();
        handled = result != rive::HitResult::none;
    });
    emit_signal("changed");
    return handled;
}

bool RivePlayer::send_scroll(Vector2 p_position, Vector2 p_delta, int p_phase, bool p_precise, double p_timestamp) {
    if (!state_machine || !p_position.is_finite() || !p_delta.is_finite() || p_phase < SCROLL_BEGIN || p_phase > SCROLL_INERTIA_CANCEL ||
            !std::isfinite(p_timestamp) || p_timestamp < 0 || p_timestamp > 1e12) return false;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        rive::ScrollEvent event;
        event.delta = rive::Vec2D(p_delta.x, p_delta.y);
        event.phase = static_cast<rive::ScrollPhase>(p_phase);
        event.precise = p_precise;
        handled = state_machine->pointerScroll(rive::Vec2D(p_position.x, p_position.y), event, p_timestamp) != rive::HitResult::none;
        collect_state_machine_events();
    });
    if (handled) emit_signal("changed");
    return handled;
}

void RivePlayer::cancel_scroll() {
    rive_integration::run_on_rive_render_thread([&]() { if (state_machine) state_machine->cancelScroll(); });
    emit_signal("changed");
}

bool RivePlayer::set_gamepad_connected(int p_device, bool p_connected) {
    return update_gamepad_connection(p_device, p_connected, true);
}

bool RivePlayer::update_gamepad_connection(int p_device, bool p_connected, bool p_notify) {
    if (!state_machine || p_device < 0) return false;
    if (p_connected == gamepads.has(p_device)) return true;
    PackedByteArray bytes;
    bytes.resize(p_connected ? 113 : 9);
    bytes.fill(0);
    bytes.encode_u32(0, rive::kGamepadBatchWireVersion);
    bytes.set(4, static_cast<uint8_t>(p_connected ? rive::GamepadRecordType::connected : rive::GamepadRecordType::disconnected));
    bytes.encode_u32(5, p_device);
    if (p_connected) { bytes.set(10, 19); bytes.set(11, 6); }
    bool accepted = false;
    rive_integration::run_on_rive_render_thread([&]() { accepted = state_machine->submitGamepadsFromBuffer(bytes.ptr(), bytes.size()); });
    if (!accepted) return false;
    if (p_connected) {
        PackedFloat32Array buttons; buttons.resize(19); buttons.fill(0);
        PackedFloat32Array axes; axes.resize(6); axes.fill(0);
        Dictionary state; state["buttons"] = buttons; state["axes"] = axes;
        gamepads[p_device] = state;
    } else gamepads.erase(p_device);
    if (p_notify) emit_signal("changed");
    return true;
}

Dictionary RivePlayer::get_gamepad_state(int p_device) const {
    return gamepads.has(p_device) ? gamepads[p_device].duplicate(true) : Dictionary();
}

bool RivePlayer::gamepad_input(const Ref<InputEvent> &p_event) {
    if (!state_machine || p_event.is_null() || p_event->get_device() < 0) return false;
    Ref<InputEventJoypadButton> button = p_event;
    Ref<InputEventJoypadMotion> motion = p_event;
    if (button.is_null() && motion.is_null()) return false;
    static const int GODOT_TO_RIVE_BUTTON[] = {0, 1, 2, 3, 8, 16, 9, 10, 11, 4, 5, 12, 13, 14, 15};
    int index = -1;
    bool is_axis = motion.is_valid();
    float value = 0;
    if (button.is_valid()) {
        const int godot_button = static_cast<int>(button->get_button_index());
        if (godot_button < 0 || godot_button >= 15) return false;
        index = GODOT_TO_RIVE_BUTTON[godot_button];
        const float pressure = button->get_pressure();
        if (!std::isfinite(pressure) || pressure < 0 || pressure > 1) return false;
        value = button->is_pressed() ? (pressure > 0 ? pressure : 1.0f) : 0.0f;
    } else {
        index = static_cast<int>(motion->get_axis());
        value = motion->get_axis_value();
        if (index < 0 || index > 5 || !std::isfinite(value) || value < -1 || value > 1) return false;
        if (index >= 4) value = std::max(0.0f, value);
    }
    const int device = p_event->get_device();
    if (!gamepads.has(device)) {
        if (!update_gamepad_connection(device, true, false)) return false;
    }
    Dictionary state = gamepads[device];
    PackedFloat32Array values = state[is_axis ? "axes" : "buttons"];
    values.set(index, value);
    state[is_axis ? "axes" : "buttons"] = values;
    const bool trigger = is_axis && index >= 4;
    PackedByteArray bytes; bytes.resize(trigger ? 22 : 16); bytes.fill(0);
    bytes.encode_u32(0, rive::kGamepadBatchWireVersion);
    bytes.set(4, static_cast<uint8_t>(rive::GamepadRecordType::update));
    bytes.encode_u32(5, device);
    bytes.set(9, trigger ? 2 : 1);
    bytes.set(10, is_axis ? 1 : 0); bytes.set(11, index); bytes.encode_float(12, value);
    if (trigger) {
        bytes.set(16, 0); bytes.set(17, index + 2); bytes.encode_float(18, value);
        PackedFloat32Array buttons = state["buttons"]; buttons.set(index + 2, value); state["buttons"] = buttons;
    }
    bool accepted = false;
    rive_integration::run_on_rive_render_thread([&]() {
        accepted = state_machine->submitGamepadsFromBuffer(bytes.ptr(), bytes.size());
        collect_state_machine_events();
    });
    if (accepted) { gamepads[device] = state; emit_signal("changed"); }
    return accepted;
}
