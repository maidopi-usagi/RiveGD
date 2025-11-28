#ifndef RIVE_EDITOR_PLUGIN_H
#define RIVE_EDITOR_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/editor_import_plugin.hpp>
#include "rive_inspector_plugin.h"

using namespace godot;

class RiveImportPlugin : public EditorImportPlugin {
    GDCLASS(RiveImportPlugin, EditorImportPlugin);

protected:
    static void _bind_methods();

public:
    String _get_importer_name() const override;
    String _get_visible_name() const override;
    PackedStringArray _get_recognized_extensions() const override;
    String _get_save_extension() const override;
    String _get_resource_type() const override;
    float _get_priority() const override;
    int32_t _get_preset_count() const override;
    String _get_preset_name(int32_t preset_index) const override;
    TypedArray<Dictionary> _get_import_options(const String &path, int32_t preset_index) const override;
    bool _get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const override;
    Error _import(const String &source_file, const String &save_path, const Dictionary &options, const TypedArray<String> &platform_variants, const TypedArray<String> &gen_files) const override;
};

class RiveEditorPlugin : public EditorPlugin {
    GDCLASS(RiveEditorPlugin, EditorPlugin);

private:
    Ref<RiveImportPlugin> import_plugin;
    Ref<RiveInspectorPlugin> inspector_plugin;

protected:
    static void _bind_methods();

public:
    void _enter_tree() override;
    void _exit_tree() override;
};

#endif
