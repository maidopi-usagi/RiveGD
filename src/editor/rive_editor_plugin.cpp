#include "rive_editor_plugin.h"
#include "../resources/rive_file.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>

void RiveImportPlugin::_bind_methods() {}

String RiveImportPlugin::_get_importer_name() const {
    return "rive.import";
}

String RiveImportPlugin::_get_visible_name() const {
    return "Rive File";
}

PackedStringArray RiveImportPlugin::_get_recognized_extensions() const {
    PackedStringArray arr;
    arr.append("riv");
    arr.append("svg");
    return arr;
}

String RiveImportPlugin::_get_save_extension() const {
    return "res";
}

String RiveImportPlugin::_get_resource_type() const {
    return "RiveFile";
}

float RiveImportPlugin::_get_priority() const {
    return 1.0;
}

int32_t RiveImportPlugin::_get_preset_count() const {
    return 1;
}

String RiveImportPlugin::_get_preset_name(int32_t preset_index) const {
    return "Default";
}

TypedArray<Dictionary> RiveImportPlugin::_get_import_options(const String &path, int32_t preset_index) const {
    return TypedArray<Dictionary>();
}

bool RiveImportPlugin::_get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const {
    return true;
}

Error RiveImportPlugin::_import(const String &source_file, const String &save_path, const Dictionary &options, const TypedArray<String> &platform_variants, const TypedArray<String> &gen_files) const {
    Ref<FileAccess> file = FileAccess::open(source_file, FileAccess::READ);
    if (file.is_null()) {
        return ERR_FILE_CANT_OPEN;
    }

    PackedByteArray data = file->get_buffer(file->get_length());
    
    Ref<RiveFile> rive_file;
    rive_file.instantiate();
    rive_file->set_data(data);
    
    return ResourceSaver::get_singleton()->save(rive_file, save_path + String(".") + _get_save_extension());
}

void RiveEditorPlugin::_bind_methods() {}

void RiveEditorPlugin::_enter_tree() {
    import_plugin.instantiate();
    add_import_plugin(import_plugin);
    
    inspector_plugin.instantiate();
    add_inspector_plugin(inspector_plugin);
}

void RiveEditorPlugin::_exit_tree() {
    remove_import_plugin(import_plugin);
    import_plugin.unref();
    
    if (inspector_plugin.is_valid()) {
        remove_inspector_plugin(inspector_plugin);
        inspector_plugin.unref();
    }
}
