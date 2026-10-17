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
    return 0.5;
}

int32_t RiveImportPlugin::_get_preset_count() const {
    return 1;
}

String RiveImportPlugin::_get_preset_name(int32_t preset_index) const {
    return "Default";
}

TypedArray<Dictionary> RiveImportPlugin::_get_import_options(const String &path, int32_t preset_index) const {
    TypedArray<Dictionary> result;
    Dictionary assets;
    assets["name"] = "assets/overrides";
    assets["default_value"] = Dictionary();
    result.push_back(assets);
    return result;
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
    if (data.is_empty()) return ERR_FILE_CORRUPT;
    if (source_file.get_extension().to_lower() == "riv" &&
            (data.size() < 4 || data[0] != 'R' || data[1] != 'I' || data[2] != 'V' || data[3] != 'E')) {
        return ERR_FILE_UNRECOGNIZED;
    }
    
    Ref<RiveFile> rive_file;
    rive_file.instantiate();
    rive_file->set_data(data);
    const Variant raw_assets = options.get("assets/overrides", Dictionary());
    if (raw_assets.get_type() != Variant::DICTIONARY) return ERR_INVALID_PARAMETER;
    const Dictionary assets = raw_assets;
    rive_file->set_asset_overrides(assets);
    if (rive_file->get_asset_overrides() != assets) return ERR_INVALID_PARAMETER;
    
    return ResourceSaver::get_singleton()->save(rive_file, save_path + String(".") + _get_save_extension());
}

void RiveEditorPlugin::_bind_methods() {}

void RiveEditorPlugin::_enter_tree() {
    export_plugin.instantiate();
    add_export_plugin(export_plugin);
    import_plugin.instantiate();
    add_import_plugin(import_plugin);
    
    inspector_plugin.instantiate();
    add_inspector_plugin(inspector_plugin);
}

void RiveEditorPlugin::_exit_tree() {
    if (export_plugin.is_valid()) { remove_export_plugin(export_plugin); export_plugin.unref(); }
    remove_import_plugin(import_plugin);
    import_plugin.unref();
    
    if (inspector_plugin.is_valid()) {
        remove_inspector_plugin(inspector_plugin);
        inspector_plugin.unref();
    }
}
