#include "rive_export_plugin.h"
#include "../resources/rive_file.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/scene_state.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveExportPlugin::_export_file(const String &path, const String &type, const PackedStringArray &) {
    if (type == "PackedScene" || type == "RiveFile" || path.get_extension() == "tres" || path.get_extension() == "res") {
        inspect_value(ResourceLoader::get_singleton()->load(path), 0);
    }
}

void RiveExportPlugin::inspect_value(const Variant &value, int depth) {
    if (depth >= 64 || inspected.size() >= 16384) return;
    if (value.get_type() == Variant::ARRAY) {
        const Array array = value;
        for (int64_t i = 0; i < array.size(); ++i) inspect_value(array[i], depth + 1);
    } else if (value.get_type() == Variant::DICTIONARY) {
        const Array values = Dictionary(value).values();
        for (int64_t i = 0; i < values.size(); ++i) inspect_value(values[i], depth + 1);
    } else if (value.get_type() == Variant::OBJECT) {
        Ref<Resource> resource = value;
        if (resource.is_null() || inspected.has(resource->get_instance_id())) return;
        inspected.insert(resource->get_instance_id());
        Ref<RiveFile> file = resource;
        if (file.is_valid()) {
            const Array overrides = file->get_asset_overrides().values();
            for (int64_t i = 0; i < overrides.size(); ++i) {
                if (overrides[i].get_type() != Variant::STRING) continue;
                const String dependency = overrides[i];
                const String path = dependency.simplify_path();
                if (!path.begins_with("res://") || path.trim_prefix("res://").begins_with("../") || !FileAccess::file_exists(path)) {
                    UtilityFunctions::push_error("Rive export requires a readable res:// asset path: " + dependency);
                    continue;
                }
                if (!included.has(path)) {
                    add_file(path, FileAccess::get_file_as_bytes(path), false);
                    included.insert(path);
                }
            }
            return;
        }
        Ref<PackedScene> scene = resource;
        if (scene.is_valid()) {
            Ref<SceneState> state = scene->get_state();
            for (int level = 0; state.is_valid() && level < 64; ++level, state = state->get_base_scene_state()) {
                for (int node = 0; node < state->get_node_count(); ++node) {
                    inspect_value(state->get_node_instance(node), depth + 1);
                    for (int p = 0; p < state->get_node_property_count(node); ++p) inspect_value(state->get_node_property_value(node, p), depth + 1);
                }
            }
            return;
        }
        const Array properties = resource->get_property_list();
        for (int64_t i = 0; i < properties.size(); ++i) {
            const Dictionary property = properties[i];
            if ((int64_t(property["usage"]) & PROPERTY_USAGE_STORAGE) != 0) inspect_value(resource->get(property["name"]), depth + 1);
        }
    }
}
