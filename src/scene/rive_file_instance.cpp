#include "rive_file_instance.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>

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

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
}

RiveFileInstance::RiveFileInstance() {
    rive_player.instantiate();
}

RiveFileInstance::~RiveFileInstance() {
    if (rive_file_resource.is_valid()) {
        const Callable changed_callback(this, "_on_rive_file_changed");
        if (rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->disconnect("changed", changed_callback);
        }
    }
    rive_player.unref();
    rive_file_resource.unref();
}

void RiveFileInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
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
    queue_redraw();
}

void RiveFileInstance::_on_rive_file_changed() {
    if (!_load_artboard()) {
        UtilityFunctions::printerr("RiveFileInstance: resource reload failed; keeping the previous playable instance.");
    }
    queue_redraw();
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
    queue_redraw();
}

String RiveFileInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveFileInstance::set_state_machine_name(const String &p_name) {
    if (state_machine_name == p_name) return;
    String previous_name = state_machine_name;
    state_machine_name = p_name;
    if (!_load_artboard()) {
        state_machine_name = previous_name;
    }
}

String RiveFileInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveFileInstance::set_animation_name(const String &p_name) {
    if (animation_name == p_name) return;
    String previous_name = animation_name;
    animation_name = p_name;
    if (!_load_artboard()) {
        animation_name = previous_name;
    }
}

String RiveFileInstance::get_animation_name() const {
    return animation_name;
}

void RiveFileInstance::set_auto_play(bool p_auto) {
    auto_play = p_auto;
}

bool RiveFileInstance::get_auto_play() const {
    return auto_play;
}

bool RiveFileInstance::_load_artboard() {
    if (!rive_player.is_valid()) return false;
    if (rive_file_resource.is_null()) {
        rive_player->set_artboard(nullptr);
        return true;
    }
    
    std::unique_ptr<rive::ArtboardInstance> artboard = rive_file_resource->instantiate_artboard(artboard_name);

    if (!artboard) {
        UtilityFunctions::printerr("RiveFileInstance: failed to instantiate artboard '" + artboard_name + "'.");
        return false;
    }

    rive::rcp<rive::File> file;
    if (rive::File* raw = rive_file_resource->get_rive_file()) {
        // safe_ref before adopting into rcp (rcp(T*) does not increment refcount)
        raw->ref();
        file = rive::rcp<rive::File>(raw);
    }

    // Validate the requested scene against the candidate before replacing the
    // currently playable player state.
    if (!state_machine_name.is_empty()) {
        std::unique_ptr<rive::StateMachineInstance> candidate = artboard->stateMachineNamed(state_machine_name.utf8().get_data());
        if (!candidate) {
            UtilityFunctions::printerr("RiveFileInstance: state machine not found: " + state_machine_name);
            return false;
        }
    } else if (!animation_name.is_empty()) {
        std::unique_ptr<rive::LinearAnimationInstance> candidate = artboard->animationNamed(animation_name.utf8().get_data());
        if (!candidate) {
            UtilityFunctions::printerr("RiveFileInstance: animation not found: " + animation_name);
            return false;
        }
    }

    rive_player->set_artboard(std::move(artboard), file);
    if (!state_machine_name.is_empty()) {
        rive_player->play_state_machine(state_machine_name);
    } else if (!animation_name.is_empty()) {
        rive_player->play_animation(animation_name);
    }
    return true;
}

void RiveFileInstance::advance(double delta) {
    if (!rive_player.is_valid()) return;
    if (!auto_play) return;
    rive_player->advance(delta);
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
    return Rect2(-32, -32, 64, 64); // default so node is selectable in editor
}

void RiveFileInstance::_draw() {
    if (Engine::get_singleton()->is_editor_hint()) {
        draw_rect(get_rive_bounds(), Color(1, 1, 1, 0.01));
    }
}

void RiveFileInstance::_input(const Ref<InputEvent> &p_event) {
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!is_inside_tree()) return;
    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid()) {
        if (mb->is_pressed()) {
            pointer_down(mb->get_position());
        } else {
            pointer_up(mb->get_position());
        }
        return;
    }
    Ref<InputEventMouseMotion> mm = p_event;
    if (mm.is_valid()) {
        pointer_move(mm->get_position());
    }
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
    if (rive_player.is_valid()) {
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        rive_player->pointer_down(position, rive_transform);
    }
}

void RiveFileInstance::pointer_up(Vector2 position) {
    if (rive_player.is_valid()) {
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        rive_player->pointer_up(position, rive_transform);
    }
}

void RiveFileInstance::pointer_move(Vector2 position) {
    if (rive_player.is_valid()) {
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        rive_player->pointer_move(position, rive_transform);
    }
}

Ref<RiveViewModelInstance> RiveFileInstance::get_view_model_instance() const {
    if (rive_player.is_valid()) {
        return rive_player->get_rive_view_model_instance();
    }
    return nullptr;
}
