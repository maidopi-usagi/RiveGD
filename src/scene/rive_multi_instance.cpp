#include "rive_multi_instance.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveMultiInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_file", "file"), &RiveMultiInstance::set_rive_file);
    ClassDB::bind_method(D_METHOD("get_rive_file"), &RiveMultiInstance::get_rive_file);
    ClassDB::bind_method(D_METHOD("_on_rive_file_changed"), &RiveMultiInstance::_on_rive_file_changed);
    
    ClassDB::bind_method(D_METHOD("set_artboard_name", "name"), &RiveMultiInstance::set_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_name"), &RiveMultiInstance::get_artboard_name);
    
    ClassDB::bind_method(D_METHOD("set_state_machine_name", "name"), &RiveMultiInstance::set_state_machine_name);
    ClassDB::bind_method(D_METHOD("get_state_machine_name"), &RiveMultiInstance::get_state_machine_name);
    
    ClassDB::bind_method(D_METHOD("set_animation_name", "name"), &RiveMultiInstance::set_animation_name);
    ClassDB::bind_method(D_METHOD("get_animation_name"), &RiveMultiInstance::get_animation_name);
    
    ClassDB::bind_method(D_METHOD("set_auto_play", "auto_play"), &RiveMultiInstance::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &RiveMultiInstance::get_auto_play);

    ClassDB::bind_method(D_METHOD("set_transforms", "transforms"), &RiveMultiInstance::set_transforms);
    ClassDB::bind_method(D_METHOD("get_transforms"), &RiveMultiInstance::get_transforms);
    ClassDB::bind_method(D_METHOD("get_rive_player"), &RiveMultiInstance::get_rive_player);
    ClassDB::bind_method(D_METHOD("get_view_model_instance"), &RiveMultiInstance::get_view_model_instance);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "transforms", PROPERTY_HINT_ARRAY_TYPE, "Transform2D", PROPERTY_USAGE_DEFAULT), "set_transforms", "get_transforms");
}

RiveMultiInstance::RiveMultiInstance() {
    rive_player.instantiate();
    connect_rive_player_signals(rive_player.ptr());
}

RiveMultiInstance::~RiveMultiInstance() {
    if (rive_file_resource.is_valid()) {
        const Callable changed_callback(this, "_on_rive_file_changed");
        if (rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->disconnect("changed", changed_callback);
        }
    }
    connect_rive_player_signals(rive_player.ptr(), false);
    rive_player.unref();
}

void RiveMultiInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
            update_configuration_warnings();
            break;
        case NOTIFICATION_VISIBILITY_CHANGED:
            request_rive_update();
            break;
    }
}

void RiveMultiInstance::set_rive_file(const Ref<RiveFile> &p_file) {
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

void RiveMultiInstance::_on_rive_file_changed() {
    if (!_load_artboard()) {
        UtilityFunctions::printerr("RiveMultiInstance: resource reload failed; keeping the previous playable instance.");
    }
    update_configuration_warnings();
    request_rive_update();
}

Ref<RiveFile> RiveMultiInstance::get_rive_file() const {
    return rive_file_resource;
}

void RiveMultiInstance::set_artboard_name(const String &p_name) {
    if (artboard_name == p_name) return;
    String previous_name = artboard_name;
    artboard_name = p_name;
    if (!_load_artboard()) {
        artboard_name = previous_name;
    }
    update_configuration_warnings();
    request_rive_update();
}

String RiveMultiInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveMultiInstance::set_state_machine_name(const String &p_name) {
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

String RiveMultiInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveMultiInstance::set_animation_name(const String &p_name) {
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

String RiveMultiInstance::get_animation_name() const {
    return animation_name;
}

void RiveMultiInstance::set_auto_play(bool p_auto) {
    set_playing(p_auto);
}

bool RiveMultiInstance::get_auto_play() const {
    return is_playing();
}

void RiveMultiInstance::set_transforms(const Array &p_transforms) {
    TypedArray<Transform2D> validated;
    for (int i = 0; i < p_transforms.size(); ++i) {
        ERR_FAIL_COND_MSG(p_transforms[i].get_type() != Variant::TRANSFORM2D, "RiveMultiInstance transforms must contain only Transform2D values.");
        const Transform2D transform = p_transforms[i];
        ERR_FAIL_COND_MSG(!transform.is_finite(), "RiveMultiInstance transforms must be finite.");
        validated.push_back(transform);
    }
    transforms = validated;
    update_configuration_warnings();
    request_rive_update();
}

TypedArray<Transform2D> RiveMultiInstance::get_transforms() const {
    return TypedArray<Transform2D>(transforms.duplicate());
}

bool RiveMultiInstance::_load_artboard() {
    if (rive_file_resource.is_null()) {
        rive_player->set_artboard(nullptr);
        load_error = String();
        emit_signal("view_model_changed");
        return true;
    }

    std::unique_ptr<rive::ArtboardInstance> next_artboard = rive_file_resource->instantiate_artboard(artboard_name);
    if (!next_artboard) {
        load_error = "Unable to load artboard '" + artboard_name + "'.";
        UtilityFunctions::printerr("RiveMultiInstance: failed to instantiate artboard '" + artboard_name + "'.");
        return false;
    }

    rive::rcp<rive::File> next_file;
    if (rive::File *raw_file = rive_file_resource->get_rive_file()) {
        raw_file->ref();
        next_file = rive::rcp<rive::File>(raw_file);
    }

    if (!animation_name.is_empty()) {
        if (!next_artboard->animation(animation_name.utf8().get_data())) {
            load_error = "Animation not found: " + animation_name;
            UtilityFunctions::printerr("RiveMultiInstance: " + load_error);
            return false;
        }
    } else if (!state_machine_name.is_empty()) {
        if (!next_artboard->stateMachine(state_machine_name.utf8().get_data())) {
            load_error = "State machine not found: " + state_machine_name;
            UtilityFunctions::printerr("RiveMultiInstance: " + load_error);
            return false;
        }
    }

    Ref<RivePlayer> next_player;
    next_player.instantiate();
    next_player->configure_playback(animation_name, state_machine_name);
    next_player->set_artboard(std::move(next_artboard), next_file);
    connect_rive_player_signals(rive_player.ptr(), false);
    rive_player = next_player;
    connect_rive_player_signals(rive_player.ptr());
    load_error = String();
    future_active = true;
    request_rive_update();
    emit_signal("view_model_changed");
    return true;
}

void RiveMultiInstance::advance(double delta) {
    _advance_player(delta, false);
}

void RiveMultiInstance::advance_rive_automatically(double delta) {
    if (get_update_mode() != UPDATE_MANUAL) _advance_player(delta, true);
}

void RiveMultiInstance::_advance_player(double delta, bool p_automatic) {
    if (rive_player.is_null()) return;
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
    if (RiveMultiInstance *instance = Object::cast_to<RiveMultiInstance>(ObjectDB::get_instance((uint64_t)self_id))) {
        if (playing) instance->future_active = active;
        if (changed) instance->request_rive_update();
    }
}

void RiveMultiInstance::draw(rive::Renderer *renderer) {
    if (rive_player.is_null() || !rive_player->get_artboard()) return;
    const Ref<RivePlayer> player = rive_player;
    
    Transform2D base_xform = get_transform();
    
    for (int i = 0; i < transforms.size(); i++) {
        Transform2D t = transforms[i];
        Transform2D final_xform = base_xform * t;
        
        renderer->save();
        
        rive::Mat2D rive_transform(
            final_xform.columns[0].x, final_xform.columns[0].y,
            final_xform.columns[1].x, final_xform.columns[1].y,
            final_xform.columns[2].x, final_xform.columns[2].y
        );
        
        player->draw(renderer, rive_transform);
        
        renderer->restore();
    }
}

Rect2 RiveMultiInstance::get_rive_bounds() const {
    if (rive_player.is_valid() && rive_player->get_artboard() && transforms.size() > 0) {
        rive::AABB aabb = rive_player->get_artboard()->bounds();
        Rect2 base_rect(aabb.minX, aabb.minY, aabb.width(), aabb.height());
        
        Rect2 total_rect;
        bool first = true;
        
        for (int i = 0; i < transforms.size(); i++) {
            Transform2D t = transforms[i];
            Rect2 r = t.xform(base_rect);
            if (first) {
                total_rect = r;
                first = false;
            } else {
                total_rect = total_rect.merge(r);
            }
        }
        return total_rect;
    }
    return Rect2();
}

Ref<RiveViewModelInstance> RiveMultiInstance::get_view_model_instance() const {
    return rive_player.is_valid() ? rive_player->get_rive_view_model_instance() : Ref<RiveViewModelInstance>();
}

PackedStringArray RiveMultiInstance::_get_configuration_warnings() const {
    PackedStringArray warnings = RiveNode::_get_configuration_warnings();
    if (rive_file_resource.is_null()) warnings.push_back("Assign a RiveFile resource.");
    if (transforms.is_empty()) warnings.push_back("Add at least one Transform2D to render a copy.");
    if (!animation_name.is_empty() && !state_machine_name.is_empty()) {
        warnings.push_back("animation_name takes precedence over state_machine_name. Clear animation_name to play the state machine.");
    }
    if (!load_error.is_empty()) warnings.push_back(load_error);
    return warnings;
}
