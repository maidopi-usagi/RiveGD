#include "rive_file_instance.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveFileInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_file", "file"), &RiveFileInstance::set_rive_file);
    ClassDB::bind_method(D_METHOD("get_rive_file"), &RiveFileInstance::get_rive_file);
    
    ClassDB::bind_method(D_METHOD("set_artboard_name", "name"), &RiveFileInstance::set_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_name"), &RiveFileInstance::get_artboard_name);
    
    ClassDB::bind_method(D_METHOD("set_state_machine_name", "name"), &RiveFileInstance::set_state_machine_name);
    ClassDB::bind_method(D_METHOD("get_state_machine_name"), &RiveFileInstance::get_state_machine_name);
    
    ClassDB::bind_method(D_METHOD("set_animation_name", "name"), &RiveFileInstance::set_animation_name);
    ClassDB::bind_method(D_METHOD("get_animation_name"), &RiveFileInstance::get_animation_name);
    
    ClassDB::bind_method(D_METHOD("set_auto_play", "auto_play"), &RiveFileInstance::set_auto_play);
    ClassDB::bind_method(D_METHOD("get_auto_play"), &RiveFileInstance::get_auto_play);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
}

RiveFileInstance::RiveFileInstance() {}

RiveFileInstance::~RiveFileInstance() {
    artboard.reset();
    state_machine.reset();
    animation.reset();
}

void RiveFileInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
            break;
    }
}

void RiveFileInstance::set_rive_file(const Ref<RiveFile> &p_file) {
    rive_file_resource = p_file;
    _load_artboard();
    queue_redraw();
}

Ref<RiveFile> RiveFileInstance::get_rive_file() const {
    return rive_file_resource;
}

void RiveFileInstance::set_artboard_name(const String &p_name) {
    artboard_name = p_name;
    _load_artboard();
    queue_redraw();
}

String RiveFileInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveFileInstance::set_state_machine_name(const String &p_name) {
    state_machine_name = p_name;
    _load_artboard(); // Reload to apply state machine
}

String RiveFileInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveFileInstance::set_animation_name(const String &p_name) {
    animation_name = p_name;
    _load_artboard(); // Reload to apply animation
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

void RiveFileInstance::_load_artboard() {
    if (rive_file_resource.is_null()) return;
    
    // Reset existing
    artboard.reset();
    state_machine.reset();
    animation.reset();

    // Use RiveFile to instantiate artboard (handles both Rive and SVG)
    artboard = rive_file_resource->instantiate_artboard(artboard_name);

    if (!artboard) return;

    // Load State Machine or Animation
    if (!state_machine_name.is_empty()) {
        state_machine = artboard->stateMachineNamed(state_machine_name.utf8().get_data());
    } else if (artboard->stateMachineCount() > 0) {
        // Default to first state machine if available and no animation specified?
        // Or maybe we should be explicit.
        // Let's try to load default state machine if no name provided
        state_machine = artboard->stateMachineAt(0);
    }

    if (!state_machine && !animation_name.is_empty()) {
        animation = artboard->animationNamed(animation_name.utf8().get_data());
    } else if (!state_machine && artboard->animationCount() > 0) {
        animation = artboard->animationAt(0);
    }
    
    if (artboard) {
        artboard->advance(0.0f);
    }
}

void RiveFileInstance::advance(double delta) {
    if (!artboard) return;
    if (!auto_play) return;

    if (state_machine) {
        state_machine->advance(delta);
    } else if (animation) {
        animation->advance(delta);
        animation->apply();
    }
    
    artboard->advance(delta);
}

void RiveFileInstance::draw(rive::Renderer *renderer) {
    if (!artboard) return;
    
    renderer->save();
    
    Transform2D xform = get_transform();
    rive::Mat2D rive_transform(
        xform.columns[0].x, xform.columns[0].y,
        xform.columns[1].x, xform.columns[1].y,
        xform.columns[2].x, xform.columns[2].y
    );
    
    renderer->transform(rive_transform);
    artboard->draw(renderer);
    
    renderer->restore();
}

bool RiveFileInstance::hit_test(Vector2 point) {
    // Point is in parent space (CanvasLayer space)
    // We need to transform it to local space?
    // Rive hitTest expects coordinates in the artboard space?
    // No, Rive hitTest usually works with the transformed components.
    // But here we are transforming the renderer.
    
    // If we want to hit test, we should probably transform the point into the Node's local space
    // and then check against the artboard bounds?
    // Or better, let Rive handle it if we can pass the transform.
    
    // For now, simple bounding box check or just return false.
    // Implementing proper hit test requires more complex logic with Rive.
    return false; 
}

void RiveFileInstance::pointer_down(Vector2 position) {
    if (state_machine) {
        // Transform position to local space
        Transform2D inv = get_transform().affine_inverse();
        Vector2 local = inv.xform(position);
        state_machine->pointerDown(rive::Vec2D(local.x, local.y));
    }
}

void RiveFileInstance::pointer_up(Vector2 position) {
    if (state_machine) {
        Transform2D inv = get_transform().affine_inverse();
        Vector2 local = inv.xform(position);
        state_machine->pointerUp(rive::Vec2D(local.x, local.y));
    }
}

void RiveFileInstance::pointer_move(Vector2 position) {
    if (state_machine) {
        Transform2D inv = get_transform().affine_inverse();
        Vector2 local = inv.xform(position);
        state_machine->pointerMove(rive::Vec2D(local.x, local.y));
    }
}
