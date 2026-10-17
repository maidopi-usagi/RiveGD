#include "rive_file.h"
#include "../rive_variant_utils.h"
#include "../renderer/rive_renderer.h"
#include "../scene/rive_view_model.h"
#include <godot_cpp/classes/font_file.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <rive/viewmodel/viewmodel.hpp>
#include <rive/assets/file_asset.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/assets/font_asset.hpp>
#include <rive/assets/blob_asset.hpp>
#include <rive/assets/script_asset.hpp>
#include <rive/simple_array.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/viewmodel/viewmodel_property_viewmodel.hpp>
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>

void RiveFile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_data", "data"), &RiveFile::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &RiveFile::get_data);
    ClassDB::bind_method(D_METHOD("load_rive_file"), &RiveFile::load_rive_file);
    ClassDB::bind_method(D_METHOD("set_asset_overrides", "assets"), &RiveFile::set_asset_overrides);
    ClassDB::bind_method(D_METHOD("get_asset_overrides"), &RiveFile::get_asset_overrides);
    ClassDB::bind_method(D_METHOD("get_asset_metadata"), &RiveFile::get_asset_metadata);
    ClassDB::bind_method(D_METHOD("get_external_dependencies"), &RiveFile::get_external_dependencies);
    ClassDB::bind_method(D_METHOD("resolve_assets", "resolver"), &RiveFile::resolve_assets);
    ClassDB::bind_method(D_METHOD("reload_assets"), &RiveFile::reload_assets);
    ClassDB::bind_method(D_METHOD("get_view_model_names"), &RiveFile::get_view_model_names);
    ClassDB::bind_method(D_METHOD("get_artboard_names"), &RiveFile::get_artboard_names);
    ClassDB::bind_method(D_METHOD("get_animation_names", "artboard"), &RiveFile::get_animation_names, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_state_machine_names", "artboard"), &RiveFile::get_state_machine_names, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_intrinsic_size", "artboard"), &RiveFile::get_intrinsic_size, DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("get_load_error"), &RiveFile::get_load_error);
    ClassDB::bind_method(D_METHOD("get_load_error_code"), &RiveFile::get_load_error_code);
    ClassDB::bind_method(D_METHOD("get_global_view_model_names"), &RiveFile::get_global_view_model_names);
    ClassDB::bind_method(D_METHOD("get_view_model_instance_names", "view_model"), &RiveFile::get_view_model_instance_names);
    ClassDB::bind_method(D_METHOD("get_view_model_schema", "view_model"), &RiveFile::get_view_model_schema);
    ClassDB::bind_method(D_METHOD("instantiate_view_model", "view_model", "instance_name", "instance_index", "blank"), &RiveFile::instantiate_view_model, DEFVAL(String()), DEFVAL(-1), DEFVAL(false));
    ClassDB::bind_method(D_METHOD("instantiate_view_model_for_artboard", "artboard", "instance_name", "instance_index", "blank"), &RiveFile::instantiate_view_model_for_artboard, DEFVAL(String()), DEFVAL(String()), DEFVAL(-1), DEFVAL(false));

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "set_data", "get_data");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "asset_overrides"), "set_asset_overrides", "get_asset_overrides");
}

RiveFile::RiveFile() {}

RiveFile::~RiveFile() {
    for (int64_t i = 0; i < observed_assets.size(); ++i) {
        Ref<Resource> resource = observed_assets[i];
        const Callable callback(this, "reload_assets");
        if (resource.is_valid() && resource->is_connected("changed", callback)) resource->disconnect("changed", callback);
    }
    if (rive_file) {
        if (!rive_integration::run_on_rive_render_thread([this]() {
                rive_file.reset();
            })) {
            rive_file.reset();
        }
    }
    rive_svg.unref();
}

void RiveFile::set_data(const PackedByteArray &p_data) {
    if (data == p_data) return;
    data = p_data;
    if (rive_file) {
        if (!rive_integration::run_on_rive_render_thread([this]() {
                rive_file.reset();
            })) {
            rive_file.reset();
        }
    }
    rive_svg.unref();
    asset_metadata.clear();
    last_load_error = OK;
    last_load_message = String();
    emit_changed();
}

void RiveFile::set_asset_overrides(const Dictionary &p_assets) {
    const Array keys = p_assets.keys();
    for (int64_t i = 0; i < keys.size(); ++i) {
        if ((keys[i].get_type() != Variant::INT && keys[i].get_type() != Variant::STRING && keys[i].get_type() != Variant::STRING_NAME) ||
                (keys[i].get_type() == Variant::INT && (int64_t(keys[i]) < 0 || int64_t(keys[i]) > UINT32_MAX)) ||
                (keys[i].get_type() != Variant::INT && String(keys[i]).is_empty())) {
            WARN_PRINT("Rive asset keys must be asset IDs or names.");
            return;
        }
        const Variant value = p_assets[keys[i]];
        if (value.get_type() == Variant::OBJECT) {
            Ref<Texture2D> texture = value;
            Ref<FontFile> font = value;
            if (texture.is_null() && font.is_null()) { WARN_PRINT("Rive assets accept Texture2D, FontFile, bytes, or a file path."); return; }
        } else if (value.get_type() != Variant::PACKED_BYTE_ARRAY && value.get_type() != Variant::STRING) {
            WARN_PRINT("Rive assets accept Texture2D, FontFile, bytes, or a file path.");
            return;
        }
    }
    if (asset_overrides == p_assets) return;
    const Callable dependency_changed(this, "reload_assets");
    for (int64_t i = 0; i < observed_assets.size(); ++i) {
        Ref<Resource> resource = observed_assets[i];
        if (resource.is_valid() && resource->is_connected("changed", dependency_changed)) resource->disconnect("changed", dependency_changed);
    }
    observed_assets.clear();
    rive_integration::assign_deep_dictionary_copy(asset_overrides, p_assets);
    const Array values = asset_overrides.values();
    for (int64_t i = 0; i < values.size(); ++i) {
        if (values[i].get_type() != Variant::OBJECT) continue;
        Ref<Resource> resource = values[i];
        if (resource.is_valid() && !observed_assets.has(resource)) {
            observed_assets.push_back(resource);
            resource->connect("changed", dependency_changed);
        }
    }
    if (!rive_integration::run_on_rive_render_thread([&]() { rive_file.reset(); })) rive_file.reset();
    rive_svg.unref();
    asset_metadata.clear();
    last_load_error = OK;
    last_load_message = String();
    emit_changed();
}

Dictionary RiveFile::get_asset_overrides() const { return asset_overrides.duplicate(true); }

void RiveFile::reload_assets() {
    Ref<RiveFile> keep_alive(this);
    if (!rive_integration::run_on_rive_render_thread([&]() { rive_file.reset(); })) rive_file.reset();
    rive_svg.unref(); asset_metadata.clear();
    last_load_error = OK; last_load_message = String();
    emit_changed();
}

Dictionary RiveFile::resolve_assets(const Callable &p_resolver) {
    Dictionary result;
    result["error"] = ERR_INVALID_PARAMETER;
    result["resolved"] = 0;
    if (!p_resolver.is_valid()) return result;
    if (resolving_assets || OS::get_singleton()->get_thread_caller_id() != OS::get_singleton()->get_main_thread_id()) {
        result["error"] = ERR_BUSY; return result;
    }
    Ref<RiveFile> keep_alive(this);
    resolving_assets = true;
    const PackedByteArray source = data;
    const Dictionary original = asset_overrides.duplicate(true);
    Dictionary candidates = original.duplicate(true);
    const Array metadata = get_asset_metadata();
    int resolved = 0;
    Error error = static_cast<Error>(get_load_error_code());
    if (error == OK) for (int64_t i = 0; i < metadata.size(); ++i) {
        const Dictionary asset = metadata[i];
        if (bool(asset.get("embedded", false)) || bool(asset.get("overridden", false))) continue;
        Array arguments; arguments.push_back(asset.duplicate(true));
        const Variant value = p_resolver.callv(arguments);
        if (data != source || asset_overrides != original) { error = ERR_BUSY; break; }
        if (value.get_type() == Variant::NIL) continue;
        bool valid = value.get_type() == Variant::PACKED_BYTE_ARRAY || value.get_type() == Variant::STRING;
        if (value.get_type() == Variant::OBJECT) {
            Ref<Texture2D> image = value; Ref<FontFile> font = value;
            valid = image.is_valid() || font.is_valid();
        }
        if (!valid) { error = ERR_INVALID_PARAMETER; break; }
        candidates[asset["id"]] = value;
        ++resolved;
    }
    if (error == OK && resolved > 0) {
        Ref<RiveFile> candidate; candidate.instantiate();
        candidate->set_data(source);
        candidate->set_asset_overrides(candidates);
        error = candidate->load_rive_file();
        if (error == OK) {
            set_asset_overrides(candidates);
            error = data == source && asset_overrides == candidates ? load_rive_file() : ERR_BUSY;
        }
    }
    resolving_assets = false;
    result["error"] = error;
    result["resolved"] = error == OK ? resolved : 0;
    return result;
}

PackedStringArray RiveFile::get_external_dependencies() const {
    PackedStringArray dependencies;
    const Array values = asset_overrides.values();
    for (int64_t i = 0; i < values.size(); ++i) {
        String path;
        if (values[i].get_type() == Variant::STRING) path = values[i];
        else if (values[i].get_type() == Variant::OBJECT) {
            Ref<Resource> resource = values[i];
            if (resource.is_valid()) path = resource->get_path();
        }
        if (!path.is_empty() && !dependencies.has(path)) dependencies.push_back(path);
    }
    return dependencies;
}

namespace {
bool can_create_blank(rive::ViewModel *p_definition, rive::File *p_file,
        std::unordered_set<rive::ViewModel *> &p_active, int p_depth) {
    if (!p_definition || p_depth > 32 || p_active.count(p_definition)) return false;
    p_active.insert(p_definition);
    for (auto *property : p_definition->properties()) {
        if (property && property->is<rive::ViewModelPropertyViewModel>()) {
            auto *child = p_file->viewModel(property->as<rive::ViewModelPropertyViewModel>()->viewModelReferenceId());
            if (!can_create_blank(child, p_file, p_active, p_depth + 1)) { p_active.erase(p_definition); return false; }
        }
    }
    p_active.erase(p_definition);
    return true;
}

struct AssetRecord {
    uint32_t id;
    std::string name, unique_name, extension, uuid;
    bool embedded, overridden, loaded;
    rive::FileAsset *asset;
    bool is_script = false, script_verified = false, script_is_module = false;
    uint32_t script_generator_ref = 0;
};

class GodotRiveAssetLoader : public rive::FileAssetLoader {
public:
    std::unordered_map<std::string, PackedByteArray> by_name;
    std::unordered_map<uint32_t, PackedByteArray> by_id;
    std::vector<AssetRecord> records;
    bool invalid_override = false;
    bool loadContents(rive::FileAsset &asset, rive::Span<const uint8_t> in_band, rive::Factory *factory) override {
        const PackedByteArray *override_bytes = nullptr;
        const auto id = by_id.find(asset.assetId());
        if (id != by_id.end()) override_bytes = &id->second;
        else {
            for (const std::string &key : {asset.uniqueName(), asset.name(), asset.uniqueFilename()}) {
                const auto entry = by_name.find(key);
                if (entry != by_name.end()) { override_bytes = &entry->second; break; }
            }
        }
        bool loaded = false;
        if (override_bytes) {
            rive::SimpleArray<uint8_t> bytes(override_bytes->ptr(), override_bytes->size());
            loaded = asset.decode(bytes, factory);
            if (!loaded) invalid_override = true;
        }
        records.push_back({asset.assetId(), asset.name(), asset.uniqueName(), asset.fileExtension(), asset.cdnUuidStr(), !in_band.empty(), override_bytes != nullptr, loaded, &asset});
        return override_bytes != nullptr;
    }
};
}

PackedByteArray RiveFile::get_data() const {
    return data;
}

Error RiveFile::load_rive_file() {
    const auto fail = [&](Error p_error, const String &p_message) {
        last_load_error = p_error;
        last_load_message = p_message;
        return p_error;
    };
    if (rive_file || rive_svg.is_valid()) return OK;
    last_load_error = OK;
    last_load_message = String();
    if (data.is_empty()) return fail(ERR_INVALID_DATA, "The Rive file has no data.");

    const uint8_t *bytes = data.ptr();
    int64_t offset = 0;
    if (data.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) {
        offset = 3;
    }
    while (offset < data.size() && (bytes[offset] == ' ' || bytes[offset] == '\t' ||
            bytes[offset] == '\r' || bytes[offset] == '\n')) {
        ++offset;
    }
    const auto has_ascii_prefix = [&](const char *prefix, size_t length) {
        return static_cast<uint64_t>(data.size() - offset) >= length &&
                std::memcmp(bytes + offset, prefix, length) == 0;
    };

    if (has_ascii_prefix("<svg", 4) || has_ascii_prefix("<?xml", 5)) {
        file_type = TYPE_SVG;
        rive_svg.instantiate();
        String xml_content;
        xml_content.parse_utf8((const char*)data.ptr(), data.size());
        rive_svg->parse(xml_content);
        return OK;
    }

    file_type = TYPE_RIVE;
    auto asset_loader = rive::make_rcp<GodotRiveAssetLoader>();
    const Array asset_keys = asset_overrides.keys();
    for (int64_t i = 0; i < asset_keys.size(); ++i) {
        const Variant value = asset_overrides[asset_keys[i]];
        PackedByteArray bytes;
        if (value.get_type() == Variant::PACKED_BYTE_ARRAY) bytes = value;
        else if (value.get_type() == Variant::STRING) bytes = FileAccess::get_file_as_bytes(value);
        else if (value.get_type() == Variant::OBJECT) {
            Ref<Texture2D> texture = value;
            Ref<FontFile> font = value;
            if (texture.is_valid()) {
                Ref<Image> image = texture->get_image();
                if (image.is_valid()) bytes = image->save_png_to_buffer();
            } else if (font.is_valid()) bytes = font->get_data();
        }
        if (bytes.is_empty()) return fail(ERR_INVALID_DATA, "An external asset override contains no readable data.");
        if (asset_keys[i].get_type() == Variant::INT) {
            const int64_t id = asset_keys[i];
            if (id < 0 || id > UINT32_MAX) return fail(ERR_INVALID_PARAMETER, "An asset ID is outside the unsigned 32-bit range.");
            asset_loader->by_id[static_cast<uint32_t>(id)] = bytes;
        } else asset_loader->by_name[String(asset_keys[i]).utf8().get_data()] = bytes;
    }
    bool factory_ready = false;
    rive::ImportResult result{};
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        RiveRenderRegistry *registry = RiveRenderRegistry::get_singleton();
        rive::Factory *factory = registry ? registry->get_factory() : nullptr;
        if (!factory) return;
        factory_ready = true;
        rive::Span<const uint8_t> bytes(data.ptr(), data.size());
        rive_file = rive::File::import(bytes, factory, &result, asset_loader);
        if (asset_loader->invalid_override) rive_file.reset();
        if (rive_file) {
            for (auto &record : asset_loader->records) {
                if (!record.asset || !record.asset->is<rive::ScriptAsset>()) continue;
                auto *script = record.asset->as<rive::ScriptAsset>();
                record.is_script = true;
                record.script_is_module = script->isModule();
#ifdef WITH_RIVE_SCRIPTING
                record.script_verified = script->verified();
                record.script_generator_ref = script->verified() ? script->generatorFunctionRef() : 0;
#endif
            }
        }
    });

    if (!executed || !factory_ready) {
        return fail(ERR_UNAVAILABLE, "A configured Rive render factory is required to import this file.");
    }

    if (!rive_file) {
        UtilityFunctions::printerr("Failed to import Rive file. Result: ", (int)result);
        return fail(ERR_PARSE_ERROR, asset_loader->invalid_override ? String("An external asset override could not be decoded.") : String("The native Rive runtime rejected this file."));
    }

    asset_metadata.clear();
    for (const auto &record : asset_loader->records) {
        Dictionary info;
        info["id"] = record.id;
        info["name"] = String::utf8(record.name.c_str());
        info["unique_name"] = String::utf8(record.unique_name.c_str());
        info["extension"] = String::utf8(record.extension.c_str());
        info["cdn_uuid"] = String::utf8(record.uuid.c_str());
        info["embedded"] = record.embedded;
        info["overridden"] = record.overridden;
        info["unresolved_external"] = !record.embedded && !record.loaded;
        info["resolution"] = record.loaded ? String("override") : (record.embedded ? String("embedded") : String("missing"));
        if (record.is_script) {
            info["script_verified"] = record.script_verified;
            info["script_is_module"] = record.script_is_module;
            info["script_generator_ref"] = record.script_generator_ref;
        }
        asset_metadata.push_back(info);
    }

    return OK;
}

Array RiveFile::get_asset_metadata() {
    if (load_rive_file() != OK) return Array();
    return asset_metadata.duplicate(true);
}

PackedStringArray RiveFile::get_view_model_names() {
    PackedStringArray names;
    if (load_rive_file() != OK || !rive_file) return names;
    for (size_t i = 0; i < rive_file->viewModelCount(); ++i) {
        auto *definition = rive_file->viewModel(i);
        if (definition) names.push_back(String::utf8(definition->name().c_str()));
    }
    return names;
}

PackedStringArray RiveFile::get_artboard_names() {
    PackedStringArray names;
    if (load_rive_file() != OK) return names;
    if (rive_file) {
        for (size_t i = 0; i < rive_file->artboardCount(); ++i) names.push_back(String::utf8(rive_file->artboardNameAt(i).c_str()));
    } else if (rive_svg.is_valid()) names.push_back("SVG");
    return names;
}

PackedStringArray RiveFile::get_animation_names(const String &p_artboard) {
    PackedStringArray names;
    auto artboard = instantiate_artboard(p_artboard);
    if (!artboard) return names;
    rive_integration::run_on_rive_render_thread([&]() {
        for (size_t i = 0; i < artboard->animationCount(); ++i) {
            auto *animation = artboard->animation(i);
            if (animation) names.push_back(String::utf8(animation->name().c_str()));
        }
        artboard.reset();
    });
    return names;
}

PackedStringArray RiveFile::get_state_machine_names(const String &p_artboard) {
    PackedStringArray names;
    auto artboard = instantiate_artboard(p_artboard);
    if (!artboard) return names;
    rive_integration::run_on_rive_render_thread([&]() {
        for (size_t i = 0; i < artboard->stateMachineCount(); ++i) {
            auto *machine = artboard->stateMachine(i);
            if (machine) names.push_back(String::utf8(machine->name().c_str()));
        }
        artboard.reset();
    });
    return names;
}

Vector2 RiveFile::get_intrinsic_size(const String &p_artboard) {
    Vector2 size;
    auto artboard = instantiate_artboard(p_artboard);
    if (!artboard) return size;
    rive_integration::run_on_rive_render_thread([&]() {
        size = Vector2(artboard->width(), artboard->height());
        artboard.reset();
    });
    return size;
}

PackedStringArray RiveFile::get_global_view_model_names() {
    PackedStringArray names;
    if (load_rive_file() != OK || !rive_file) return names;
    for (const auto &name : rive_file->globalViewModelNames()) names.push_back(String::utf8(name.c_str()));
    return names;
}

PackedStringArray RiveFile::get_view_model_instance_names(const String &p_view_model) {
    PackedStringArray names;
    if (load_rive_file() != OK || !rive_file) return names;
    auto *definition = rive_file->viewModel(p_view_model.utf8().get_data());
    if (definition) {
        for (auto *instance : definition->instances()) if (instance) names.push_back(String::utf8(instance->name().c_str()));
    }
    return names;
}

Array RiveFile::get_view_model_schema(const String &p_view_model) {
    auto instance = instantiate_view_model(p_view_model);
    return instance.is_valid() ? instance->get_properties() : Array();
}

Ref<RiveViewModelInstance> RiveFile::instantiate_view_model(const String &p_view_model,
        const String &p_instance_name, int p_instance_index, bool p_blank) {
    if (p_view_model.is_empty() || p_instance_index < -1 || load_rive_file() != OK || !rive_file) return nullptr;
    if ((p_blank && (!p_instance_name.is_empty() || p_instance_index >= 0)) ||
            (!p_instance_name.is_empty() && p_instance_index >= 0)) return nullptr;
    rive::rcp<rive::ViewModelInstance> instance;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *definition = rive_file->viewModel(p_view_model.utf8().get_data());
        if (!definition) return;
        rive::ViewModelInstance *source = nullptr;
        if (!p_blank) {
            if (!p_instance_name.is_empty()) source = definition->instance(p_instance_name.utf8().get_data());
            else source = definition->instance(static_cast<size_t>(p_instance_index >= 0 ? p_instance_index : 0));
        }
        if (source) {
            if (!rive_is_safe_view_model_reference(source, nullptr, nullptr)) return;
            instance = rive_file->copyViewModelInstance(source);
        } else if (p_blank || (p_instance_name.is_empty() && p_instance_index < 0)) {
            std::unordered_set<rive::ViewModel *> active;
            if (can_create_blank(definition, rive_file.get(), active, 0)) instance = rive_file->createViewModelInstance(definition);
        }
    });
    if (!instance) return nullptr;
    Ref<RiveViewModelInstance> wrapper;
    wrapper.instantiate();
    wrapper->_init(std::move(instance), rive_file);
    return wrapper;
}

Ref<RiveViewModelInstance> RiveFile::instantiate_view_model_for_artboard(const String &p_artboard,
        const String &p_instance_name, int p_instance_index, bool p_blank) {
    if (load_rive_file() != OK || !rive_file) return nullptr;
    auto *artboard = p_artboard.is_empty() ? rive_file->artboard() : rive_file->artboard(p_artboard.utf8().get_data());
    if (!artboard && p_artboard.is_empty() && rive_file->artboardCount()) artboard = rive_file->artboard(static_cast<size_t>(0));
    if (!artboard) return nullptr;
    auto *definition = rive_file->viewModel(static_cast<size_t>(artboard->viewModelId()));
    return definition ? instantiate_view_model(String::utf8(definition->name().c_str()), p_instance_name, p_instance_index, p_blank) : Ref<RiveViewModelInstance>();
}

rive::File* RiveFile::get_rive_file() {
    if (!rive_file && file_type == TYPE_RIVE) {
        load_rive_file();
    }
    return rive_file.get();
}

std::unique_ptr<rive::ArtboardInstance> RiveFile::instantiate_artboard(String name) {
    load_rive_file();
    
    if (file_type == TYPE_SVG) {
        if (rive_svg.is_valid()) {
            return rive_svg->instantiate_artboard();
        }
        return nullptr;
    }
    
    if (!rive_file) return nullptr;
    
    std::unique_ptr<rive::ArtboardInstance> artboard;
    if (name.is_empty()) {
        artboard = rive_file->artboardDefault();
    } else {
        artboard = rive_file->artboardNamed(name.utf8().get_data());
    }
    
    if (name.is_empty() && !artboard && rive_file->artboardCount() > 0) {
        artboard = rive_file->artboardAt(0);
    }
    
    return artboard;
}
