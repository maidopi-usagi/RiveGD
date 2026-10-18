#include "rive_view_model_preset.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/classes/font_file.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_image.hpp>
#include <rive/viewmodel/viewmodel_instance_asset_font.hpp>
#include <unordered_map>

namespace {
using AssetResources = std::unordered_map<const void *, Variant>;

bool collect_asset_resources(const Ref<RiveViewModelInstance> &p_instance,
        const Ref<RiveViewModelInstance> &p_root, const String &p_prefix, AssetResources &p_resources,
        std::unordered_set<rive::ViewModelInstance *> &p_visited, int p_depth, int &p_count) {
    if (p_instance.is_null() || p_depth > 32 || ++p_count > 1000) return false;
    const auto native = p_instance->_get_native_instance();
    if (!native || !p_visited.insert(native.get()).second) return true;
    const Array properties = p_instance->get_properties();
    for (int64_t i = 0; i < properties.size(); ++i) {
        if (++p_count > 1000) return false;
        const Dictionary property = properties[i];
        const String name = property["name"];
        const String type = property["type"];
        const String path = p_prefix.is_empty() ? name : p_prefix + String(".") + name;
        if (type == "view_model") {
            if (!collect_asset_resources(p_instance->get_view_model_property(name), p_root, path, p_resources, p_visited, p_depth + 1, p_count)) return false;
        } else if (type == "list") {
            const auto list = p_instance->get_list_property(name);
            for (int index = 0; list.is_valid() && index < list->get_size(); ++index) {
                if (!collect_asset_resources(list->get_item(index), p_root, path + String(".") + String::num_int64(index),
                        p_resources, p_visited, p_depth + 1, p_count)) return false;
            }
        } else if (type == "font" || type == "image") {
            Variant value = p_root->get_property_value(path);
            if (value.get_type() != Variant::OBJECT || !static_cast<Object *>(value)) value = p_instance->get_property_value(name);
            if (value.get_type() != Variant::OBJECT || !static_cast<Object *>(value)) continue;
            rive_integration::run_on_rive_render_thread([&]() {
                auto *asset = native->propertyValue(name.utf8().get_data());
                if (type == "font") {
                    const Ref<FontFile> font = value;
                    auto native_font = asset->as<rive::ViewModelInstanceAssetFont>()->asset()->font();
                    if (font.is_valid() && native_font) p_resources[native_font.get()] = value;
                } else {
                    auto native_image = asset->as<rive::ViewModelInstanceAssetImage>()->asset()->renderImage();
                    if (native_image) p_resources[native_image] = value;
                }
            });
        }
    }
    return true;
}

Ref<RiveViewModelPreset> capture_instance(const Ref<RiveViewModelInstance> &p_instance,
        const Ref<RiveViewModelInstance> &p_root, const String &p_prefix,
        const AssetResources &p_resources, std::unordered_set<rive::ViewModelInstance *> &p_active, int p_depth, int &p_count) {
    if (p_instance.is_null() || p_depth > 32 || ++p_count > 1000) return nullptr;
    auto native = p_instance->_get_native_instance();
    if (!native || !p_instance->_get_native_file() || !native->viewModel() || !p_active.insert(native.get()).second) return nullptr;
    struct RemoveActive {
        std::unordered_set<rive::ViewModelInstance *> &active;
        rive::ViewModelInstance *instance;
        ~RemoveActive() { active.erase(instance); }
    } remove{p_active, native.get()};

    Ref<RiveViewModelPreset> result;
    result.instantiate();
    result->set_view_model_name(p_instance->get_view_model_name());
    const String instance_name = p_instance->get_name();
    auto *source = native->viewModel()->instance(instance_name.utf8().get_data());
    if (source && !instance_name.is_empty()) result->set_instance_name(instance_name);
    else result->set_blank(true);
    rive::rcp<rive::ViewModelInstance> baseline;
    if (source) {
        rive_integration::run_on_rive_render_thread([&]() {
            if (rive_is_safe_view_model_reference(source, nullptr, nullptr)) {
                baseline = p_instance->_get_native_file()->copyViewModelInstance(source);
            }
        });
        if (!baseline) return nullptr;
    }

    Dictionary values;
    const Array properties = p_instance->get_properties();
    for (int64_t i = 0; i < properties.size(); ++i) {
        if (++p_count > 1000) return nullptr;
        const Dictionary property = properties[i];
        const String name = property["name"];
        const String type = property["type"];
        if (type == "trigger" || bool(property["read_only"])) continue;
        const String path = p_prefix.is_empty() ? name : p_prefix + String(".") + name;
        if (type == "view_model") {
            const auto child = p_instance->get_view_model_property(name);
            const auto preset = capture_instance(child, p_root, path, p_resources, p_active, p_depth + 1, p_count);
            if (preset.is_null()) return nullptr;
            values[name] = preset;
        } else if (type == "list") {
            const auto list = p_instance->get_list_property(name);
            if (list.is_null()) return nullptr;
            Array items;
            for (int index = 0; index < list->get_size(); ++index) {
                const auto preset = capture_instance(list->get_item(index), p_root,
                        path + String(".") + String::num_int64(index), p_resources, p_active, p_depth + 1, p_count);
                if (preset.is_null()) return nullptr;
                items.push_back(preset);
            }
            values[name] = items;
        } else {
            if (type != "string" && type != "number" && type != "boolean" && type != "color" && type != "enum" &&
                    type != "image" && type != "font" && type != "artboard" && type != "blob") return nullptr;
            Variant value = p_root->get_property_value(path);
            if (type == "image" || type == "font") {
                if (value.get_type() != Variant::OBJECT || !static_cast<Object *>(value)) {
                    value = p_instance->get_property_value(name);
                }
                bool embedded = false;
                bool serializable = false;
                rive_integration::run_on_rive_render_thread([&]() {
                    auto *property_value = native->propertyValue(name.utf8().get_data());
                    auto *original = baseline ? baseline->propertyValue(name.utf8().get_data()) : nullptr;
                    if (type == "image") {
                        auto *image = property_value->as<rive::ViewModelInstanceAssetImage>();
                        const auto resource = p_resources.find(image->asset()->renderImage());
                        if (resource != p_resources.end()) value = resource->second;
                        embedded = image->propertyValue() != UINT32_MAX;
                        serializable = embedded ? original && original->as<rive::ViewModelInstanceAssetImage>()->propertyValue() == image->propertyValue() :
                                !image->asset()->renderImage() || (value.get_type() == Variant::OBJECT && static_cast<Object *>(value));
                    } else {
                        auto *font = property_value->as<rive::ViewModelInstanceAssetFont>();
                        const auto resource = p_resources.find(font->asset()->font().get());
                        if (resource != p_resources.end()) value = resource->second;
                        embedded = font->propertyValue() != UINT32_MAX;
                        Ref<FontFile> font_file = value;
                        serializable = embedded ? original && original->as<rive::ViewModelInstanceAssetFont>()->propertyValue() == font->propertyValue() :
                                !font->asset()->font() || font_file.is_valid();
                    }
                });
                if (!serializable) return nullptr;
                if (embedded) continue;
            }
            if (type == "blob" && PackedByteArray(value).is_empty()) value = Variant();
            values[name] = value;
        }
    }
    result->set_values(values);
    return result;
}
}

Ref<RiveViewModelPreset> RiveViewModelPreset::from_instance(const Ref<RiveViewModelInstance> &p_instance) {
    return _capture_instance(p_instance, p_instance, String());
}

Ref<RiveViewModelPreset> RiveViewModelPreset::_capture_instance(const Ref<RiveViewModelInstance> &p_instance,
        const Ref<RiveViewModelInstance> &p_property_root, const String &p_prefix) {
    if (p_property_root.is_null()) return nullptr;
    AssetResources resources;
    std::unordered_set<rive::ViewModelInstance *> visited;
    int scanned = 0;
    if (!collect_asset_resources(p_property_root, p_property_root, String(), resources, visited, 0, scanned)) return nullptr;
    std::unordered_set<rive::ViewModelInstance *> active;
    int count = 0;
    return capture_instance(p_instance, p_property_root, p_prefix, resources, active, 0, count);
}
