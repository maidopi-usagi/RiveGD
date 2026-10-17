#include "rive_view_model_preset.h"
#include "../rive_variant_utils.h"
#include <godot_cpp/core/class_db.hpp>
#include <vector>
#include <algorithm>

void RiveViewModelPreset::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_view_model_name", "name"), &RiveViewModelPreset::set_view_model_name);
    ClassDB::bind_method(D_METHOD("get_view_model_name"), &RiveViewModelPreset::get_view_model_name);
    ClassDB::bind_method(D_METHOD("set_instance_name", "name"), &RiveViewModelPreset::set_instance_name);
    ClassDB::bind_method(D_METHOD("get_instance_name"), &RiveViewModelPreset::get_instance_name);
    ClassDB::bind_method(D_METHOD("set_instance_index", "index"), &RiveViewModelPreset::set_instance_index);
    ClassDB::bind_method(D_METHOD("get_instance_index"), &RiveViewModelPreset::get_instance_index);
    ClassDB::bind_method(D_METHOD("set_blank", "blank"), &RiveViewModelPreset::set_blank);
    ClassDB::bind_method(D_METHOD("is_blank"), &RiveViewModelPreset::is_blank);
    ClassDB::bind_method(D_METHOD("set_values", "values"), &RiveViewModelPreset::set_values);
    ClassDB::bind_method(D_METHOD("get_values"), &RiveViewModelPreset::get_values);
    ClassDB::bind_method(D_METHOD("instantiate", "file", "artboard_name"), &RiveViewModelPreset::instantiate, DEFVAL(String()));
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "view_model_name"), "set_view_model_name", "get_view_model_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "instance_name"), "set_instance_name", "get_instance_name");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "instance_index", PROPERTY_HINT_RANGE, "-1,65535,1,or_greater"), "set_instance_index", "get_instance_index");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blank"), "set_blank", "is_blank");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "values"), "set_values", "get_values");
}

void RiveViewModelPreset::set_view_model_name(const String &p_name) { if (view_model_name != p_name) { view_model_name = p_name; emit_changed(); } }
void RiveViewModelPreset::set_instance_name(const String &p_name) { if (instance_name != p_name) { instance_name = p_name; emit_changed(); } }
void RiveViewModelPreset::set_instance_index(int p_index) { if (p_index >= -1 && instance_index != p_index) { instance_index = p_index; emit_changed(); } }
void RiveViewModelPreset::set_blank(bool p_blank) { if (blank != p_blank) { blank = p_blank; emit_changed(); } }
void RiveViewModelPreset::set_values(const Dictionary &p_values) {
    rive_integration::assign_deep_dictionary_copy(values, p_values);
    emit_changed();
}

Ref<RiveViewModelInstance> RiveViewModelPreset::instantiate(const Ref<RiveFile> &p_file, const String &p_artboard_name) const {
    std::unordered_set<const RiveViewModelPreset *> active;
    auto result = instantiate_recursive(p_file, p_artboard_name, String(), active, 0);
    if (result.is_null()) WARN_PRINT("Invalid RiveViewModelPreset: check schema, instance selection, values, or reference cycles.");
    return result;
}

Ref<RiveViewModelInstance> RiveViewModelPreset::instantiate_recursive(const Ref<RiveFile> &p_file,
        const String &p_artboard, const String &p_inferred_model,
        std::unordered_set<const RiveViewModelPreset *> &p_active, int p_depth) const {
    if (p_file.is_null() || p_depth > 32 || p_active.count(this)) return nullptr;
    p_active.insert(this);
    struct RemoveActive {
        std::unordered_set<const RiveViewModelPreset *> &active;
        const RiveViewModelPreset *preset;
        ~RemoveActive() { active.erase(preset); }
    } remove{p_active, this};
    const String model = view_model_name.is_empty() ? p_inferred_model : view_model_name;
    Ref<RiveViewModelInstance> result = model.is_empty() ?
            p_file->instantiate_view_model_for_artboard(p_artboard, instance_name, instance_index, blank) :
            p_file->instantiate_view_model(model, instance_name, instance_index, blank);
    if (result.is_null()) return nullptr;

    std::vector<std::pair<String, Variant>> parent_first_overrides;
    const Array keys = values.keys();
    Dictionary canonical_keys;
    for (int64_t i = 0; i < keys.size(); ++i) {
        if (keys[i].get_type() != Variant::STRING && keys[i].get_type() != Variant::STRING_NAME) return nullptr;
        const String path = RiveViewModelInstance::canonicalize_property_path(keys[i]);
        if (path.is_empty() || canonical_keys.has(path)) return nullptr;
        canonical_keys[path] = true;
        parent_first_overrides.emplace_back(path, values[keys[i]]);
    }
    std::sort(parent_first_overrides.begin(), parent_first_overrides.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &entry : parent_first_overrides) {
        const Dictionary info = result->get_property_info(entry.first);
        if (info.is_empty() || String(info["type"]) == "trigger" || bool(info["read_only"])) return nullptr;
        const Variant &value = entry.second;
        if (value.get_type() == Variant::OBJECT) {
            Ref<RiveViewModelPreset> child = value;
            if (child.is_valid()) {
                auto previous = result->get_view_model_property(entry.first);
                if (previous.is_null()) return nullptr;
                auto instantiated = child->instantiate_recursive(p_file, String(), previous->get_view_model_name(), p_active, p_depth + 1);
                if (instantiated.is_null() || !result->replace_view_model(entry.first, instantiated)) return nullptr;
                continue;
            }
            Ref<Resource> resource = value;
            if (static_cast<Object *>(value) && resource.is_null()) return nullptr;
        }
        if (value.get_type() == Variant::ARRAY) {
            auto list = result->get_list_property(entry.first);
            if (list.is_null()) return nullptr;
            const Array presets = value;
            Array items;
            String inferred;
            auto initial = list->get_item(0);
            if (initial.is_valid()) inferred = initial->get_view_model_name();
            for (int64_t i = 0; i < presets.size(); ++i) {
                if (presets[i].get_type() != Variant::OBJECT) return nullptr;
                Ref<RiveViewModelPreset> item = presets[i];
                if (item.is_null() || (item->view_model_name.is_empty() && inferred.is_empty())) return nullptr;
                auto instantiated = item->instantiate_recursive(p_file, String(), inferred, p_active, p_depth + 1);
                if (instantiated.is_null()) return nullptr;
                items.push_back(instantiated);
            }
            if (!list->set_items(items)) return nullptr;
        } else if (!result->set_property_value(entry.first, value)) return nullptr;
    }
    return result;
}
