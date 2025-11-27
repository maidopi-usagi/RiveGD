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

RiveFileInstance::RiveFileInstance() {
    rive_player.instantiate();
}

RiveFileInstance::~RiveFileInstance() {
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
    _load_artboard();
}

String RiveFileInstance::get_state_machine_name() const {
    return state_machine_name;
}

void RiveFileInstance::set_animation_name(const String &p_name) {
    animation_name = p_name;
    _load_artboard();
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
    if (!rive_player.is_valid()) return;
    
    std::unique_ptr<rive::ArtboardInstance> artboard = rive_file_resource->instantiate_artboard(artboard_name);

    if (!artboard) return;

    rive::rcp<rive::File> file;
    if (rive_file_resource->get_rive_file()) {
        file = rive::rcp<rive::File>(rive_file_resource->get_rive_file());
    }

    rive_player->set_artboard(std::move(artboard), file);

    if (!state_machine_name.is_empty()) {
        rive_player->play_state_machine(state_machine_name);
    } else if (!animation_name.is_empty()) {
        rive_player->play_animation(animation_name);
    }
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
    return Rect2();
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
