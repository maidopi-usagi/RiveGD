#include "rive_file.h"
#include "../renderer/godot_file_asset_loader.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveFile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_data", "data"), &RiveFile::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &RiveFile::get_data);
    ClassDB::bind_method(D_METHOD("set_source_path", "path"), &RiveFile::set_source_path);
    ClassDB::bind_method(D_METHOD("get_source_path"), &RiveFile::get_source_path);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "set_data", "get_data");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.riv", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "set_source_path", "get_source_path");
}

RiveFile::RiveFile() {}

RiveFile::~RiveFile() {
    rive_file.reset();
    rive_svg.unref();
}

void RiveFile::set_data(const PackedByteArray &p_data) {
    data = p_data;
    rive_file.reset();
    rive_svg.unref();
}

PackedByteArray RiveFile::get_data() const {
    return data;
}

void RiveFile::set_source_path(const String &p_path) {
    source_path = p_path;
}

String RiveFile::get_source_path() const {
    return source_path;
}

Error RiveFile::load_rive_file() {
    if (rive_file || rive_svg.is_valid()) return OK;
    if (data.is_empty()) return ERR_INVALID_DATA;

    // Sniff first bytes to distinguish SVG (XML) from binary .riv.
    String signature = "";
    if (data.size() > 10) {
        const uint8_t* ptr = data.ptr();
        for(int i=0; i<10; i++) {
            if (ptr[i] != 0) signature += (char)ptr[i];
        }
    }
    
    if (signature.strip_edges().begins_with("<svg") || signature.strip_edges().begins_with("<?xml")) {
        file_type = TYPE_SVG;
        rive_svg.instantiate();
        String xml_content;
        xml_content.parse_utf8((const char*)data.ptr(), data.size());
        rive_svg->parse(xml_content);
        return OK;
    }

    file_type = TYPE_RIVE;
    rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) {
        return ERR_UNAVAILABLE;
    }

    rive::Span<const uint8_t> bytes(data.ptr(), data.size());
    rive::ImportResult result;

    // Create a FileAssetLoader for out-of-band assets (fonts, images)
    // Use source_path if set, otherwise fall back to Godot's Resource::get_path()
    String effective_path = source_path;
    if (effective_path.is_empty()) {
        effective_path = get_path();
    }
    rive::rcp<rive_integration::GodotFileAssetLoader> asset_loader;
    if (!effective_path.is_empty()) {
        asset_loader = rive::rcp<rive_integration::GodotFileAssetLoader>(
            new rive_integration::GodotFileAssetLoader(effective_path));
    }

    rive_file = rive::File::import(bytes, factory, &result, asset_loader);

    if (!rive_file) {
        UtilityFunctions::printerr("Failed to import Rive file. Result: ", (int)result);
        return ERR_PARSE_ERROR;
    }

    return OK;
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
    
    if (!artboard && rive_file->artboardCount() > 0) {
        artboard = rive_file->artboardAt(0);
    }
    
    return artboard;
}

PackedStringArray RiveFile::get_artboard_list() const {
    PackedStringArray list;
    if (!rive_file) return list;
    for (size_t i = 0; i < rive_file->artboardCount(); ++i) {
        const rive::Artboard* ab = rive_file->artboard(i);
        if (ab) {
            list.push_back(String(ab->name().c_str()));
        }
    }
    return list;
}
