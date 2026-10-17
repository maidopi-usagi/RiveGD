#include "rive_artboard_reference.h"
#include <godot_cpp/core/class_db.hpp>

void RiveArtboardReference::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_file", "file"), &RiveArtboardReference::set_file);
    ClassDB::bind_method(D_METHOD("get_file"), &RiveArtboardReference::get_file);
    ClassDB::bind_method(D_METHOD("set_artboard_name", "name"), &RiveArtboardReference::set_artboard_name);
    ClassDB::bind_method(D_METHOD("get_artboard_name"), &RiveArtboardReference::get_artboard_name);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "file", PROPERTY_HINT_RESOURCE_TYPE, "RiveFile"), "set_file", "get_file");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "artboard_name"), "set_artboard_name", "get_artboard_name");
}

void RiveArtboardReference::set_file(const Ref<RiveFile> &p_file) {
    if (file == p_file) return;
    file = p_file;
    emit_changed();
}

void RiveArtboardReference::set_artboard_name(const String &p_name) {
    if (artboard_name == p_name) return;
    artboard_name = p_name;
    emit_changed();
}
