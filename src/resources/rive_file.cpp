#include "rive_file.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

void RiveFile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_data", "data"), &RiveFile::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &RiveFile::get_data);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "set_data", "get_data");
}

RiveFile::RiveFile() {}

RiveFile::~RiveFile() {
    rive_file.reset();
    rive_svg.unref();
}

void RiveFile::set_data(const PackedByteArray &p_data) {
    data = p_data;
    // In editor, we might want to load immediately to check validity, 
    // but usually we load on demand or when the resource is loaded.
    // For now, let's clear the cached file so it reloads if data changes.
    rive_file.reset();
    rive_svg.unref();
}

PackedByteArray RiveFile::get_data() const {
    return data;
}

Error RiveFile::load_rive_file() {
    if (rive_file || rive_svg.is_valid()) return OK;
    if (data.is_empty()) return ERR_INVALID_DATA;

    // Check if it's SVG
    String signature = "";
    if (data.size() > 10) {
        // Read first few bytes
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
    rive_file = rive::File::import(bytes, factory, &result);

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
