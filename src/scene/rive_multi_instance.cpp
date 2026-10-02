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

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "rive_file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_rive_file", "get_rive_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "state_machine_name"), "set_state_machine_name", "get_state_machine_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "animation_name"), "set_animation_name", "get_animation_name");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_play"), "set_auto_play", "get_auto_play");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "transforms", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_transforms", "get_transforms");
}

RiveMultiInstance::RiveMultiInstance() {}

RiveMultiInstance::~RiveMultiInstance() {
    if (rive_file_resource.is_valid()) {
        const Callable changed_callback(this, "_on_rive_file_changed");
        if (rive_file_resource->is_connected("changed", changed_callback)) {
            rive_file_resource->disconnect("changed", changed_callback);
        }
    }
    _release_playable_on_render_thread();
}

void RiveMultiInstance::_release_playable_on_render_thread() {
    std::unique_ptr<rive::StateMachineInstance> old_state_machine = std::move(state_machine);
    std::unique_ptr<rive::LinearAnimationInstance> old_animation = std::move(animation);
    std::unique_ptr<rive::ArtboardInstance> old_artboard = std::move(artboard);
    rive::rcp<rive::File> old_file = std::move(retained_file);
    if (!old_state_machine && !old_animation && !old_artboard && !old_file) return;

    auto release_old = [&]() {
        old_state_machine.reset();
        old_animation.reset();
        old_artboard.reset();
        old_file.reset();
    };
    if (!rive_integration::run_on_rive_render_thread(release_old)) {
        release_old();
    }
}

void RiveMultiInstance::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY:
            _load_artboard();
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
    queue_redraw();
}

void RiveMultiInstance::_on_rive_file_changed() {
    if (!_load_artboard()) {
        UtilityFunctions::printerr("RiveMultiInstance: resource reload failed; keeping the previous playable instance.");
    }
    queue_redraw();
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
    queue_redraw();
}

String RiveMultiInstance::get_artboard_name() const {
    return artboard_name;
}

void RiveMultiInstance::set_state_machine_name(const String &p_name) {
    if (state_machine_name == p_name) return;
    String previous_name = state_machine_name;
    state_machine_name = p_name;
    if (!_load_artboard()) {
        state_machine_name = previous_name;
    }
}

String RiveMultiInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveMultiInstance::set_animation_name(const String &p_name) {
    if (animation_name == p_name) return;
    String previous_name = animation_name;
    animation_name = p_name;
    if (!_load_artboard()) {
        animation_name = previous_name;
    }
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

bool RiveMultiInstance::_load_artboard() {
    if (rive_file_resource.is_null()) {
        _release_playable_on_render_thread();
        return true;
    }

    // Build a complete replacement before touching the currently playable
    // instance. A failed resource/name change therefore leaves it intact.
    std::unique_ptr<rive::ArtboardInstance> next_artboard = rive_file_resource->instantiate_artboard(artboard_name);
    if (!next_artboard) {
        UtilityFunctions::printerr("RiveMultiInstance: failed to instantiate artboard '" + artboard_name + "'.");
        return false;
    }

    rive::rcp<rive::File> next_file;
    if (rive::File *raw_file = rive_file_resource->get_rive_file()) {
        // Artboard instances depend on their source file's objects. Keep the
        // old file alive across resource hot reloads and failed replacements.
        raw_file->ref();
        next_file = rive::rcp<rive::File>(raw_file);
    }

    std::unique_ptr<rive::StateMachineInstance> next_state_machine;
    std::unique_ptr<rive::LinearAnimationInstance> next_animation;
    if (!state_machine_name.is_empty()) {
        next_state_machine = next_artboard->stateMachineNamed(state_machine_name.utf8().get_data());
        if (!next_state_machine) {
            UtilityFunctions::printerr("RiveMultiInstance: state machine not found: " + state_machine_name);
            return false;
        }
    } else if (next_artboard->stateMachineCount() > 0) {
        next_state_machine = next_artboard->stateMachineAt(0);
    }

    if (!next_state_machine && !animation_name.is_empty()) {
        next_animation = next_artboard->animationNamed(animation_name.utf8().get_data());
        if (!next_animation) {
            UtilityFunctions::printerr("RiveMultiInstance: animation not found: " + animation_name);
            return false;
        }
    } else if (!next_state_machine && next_artboard->animationCount() > 0) {
        next_animation = next_artboard->animationAt(0);
    }

    if (next_state_machine) {
        next_state_machine->advanceAndApply(0.0f);
    } else if (next_animation) {
        next_animation->advanceAndApply(0.0f);
    } else {
        next_artboard->advance(0.0f);
    }

    _release_playable_on_render_thread();
    artboard = std::move(next_artboard);
    state_machine = std::move(next_state_machine);
    animation = std::move(next_animation);
    retained_file = std::move(next_file);
    return true;
}

void RiveMultiInstance::advance(double delta) {
    if (!artboard) return;
    if (!auto_play) return;

    if (state_machine) {
        state_machine->advanceAndApply(delta);
    } else if (animation) {
        animation->advanceAndApply(delta);
    } else {
        artboard->advance(delta);
    }
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
