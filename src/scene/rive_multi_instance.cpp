#include "rive_multi_instance.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveMultiInstance::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rive_file", "file"), &RiveMultiInstance::set_rive_file);
    ClassDB::bind_method(D_METHOD("get_rive_file"), &RiveMultiInstance::get_rive_file);
    
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

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "transforms", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_transforms", "get_transforms");
}

RiveMultiInstance::RiveMultiInstance() {}

RiveMultiInstance::~RiveMultiInstance() {
    artboard.reset();
    state_machine.reset();
    animation.reset();
}

void RiveMultiInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
            break;
    }
}

void RiveMultiInstance::set_rive_file(const Ref<RiveFile> &p_file) {
    rive_file_resource = p_file;
    _load_artboard();
    queue_redraw();
}

Ref<RiveFile> RiveMultiInstance::get_rive_file() const {
    return rive_file_resource;
}

void RiveMultiInstance::set_artboard_name(const String &p_name) {
    artboard_name = p_name;
    _load_artboard();
    queue_redraw();
}

String RiveMultiInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveMultiInstance::set_state_machine_name(const String &p_name) {
    state_machine_name = p_name;
    _load_artboard();
}

String RiveMultiInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveMultiInstance::set_animation_name(const String &p_name) {
    animation_name = p_name;
    _load_artboard();
}

String RiveMultiInstance::get_animation_name() const {
    return animation_name;
}

void RiveMultiInstance::set_auto_play(bool p_auto) {
    auto_play = p_auto;
}

bool RiveMultiInstance::get_auto_play() const {
    return auto_play;
}

void RiveMultiInstance::set_transforms(const Array &p_transforms) {
    transforms = p_transforms;
    queue_redraw();
}

Array RiveMultiInstance::get_transforms() const {
    return transforms;
}

void RiveMultiInstance::_load_artboard() {
    if (rive_file_resource.is_null()) return;
    
    artboard.reset();
    state_machine.reset();
    animation.reset();

    artboard = rive_file_resource->instantiate_artboard(artboard_name);

    if (!artboard) return;

    if (!state_machine_name.is_empty()) {
        state_machine = artboard->stateMachineNamed(state_machine_name.utf8().get_data());
    } else if (artboard->stateMachineCount() > 0) {
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

void RiveMultiInstance::advance(double delta) {
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

void RiveMultiInstance::draw(rive::Renderer *renderer) {
    if (!artboard) return;
    
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
        
        renderer->transform(rive_transform);
        artboard->draw(renderer);
        
        renderer->restore();
    }
}

Rect2 RiveMultiInstance::get_rive_bounds() const {
    if (artboard && transforms.size() > 0) {
        rive::AABB aabb = artboard->bounds();
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
