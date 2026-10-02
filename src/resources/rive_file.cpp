#include "rive_file.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>

void RiveFile::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_data", "data"), &RiveFile::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &RiveFile::get_data);

    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "set_data", "get_data");
}

RiveFile::RiveFile() {}

RiveFile::~RiveFile() {
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
    emit_changed();
}

PackedByteArray RiveFile::get_data() const {
    return data;
}

Error RiveFile::load_rive_file() {
    if (rive_file || rive_svg.is_valid()) return OK;
    if (data.is_empty()) return ERR_INVALID_DATA;

    // Inspect the prefix as bytes: binary .riv data is not Unicode text.
    // Converting arbitrary signed bytes to Godot characters emits parsing
    // errors even when the binary file itself is perfectly valid.
    const uint8_t *bytes = data.ptr();
    int64_t offset = 0;
    if (data.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) {
        offset = 3;
    }
    while (offset < data.size() && (bytes[offset] == ' ' || bytes[offset] == '\t' ||
            bytes[offset] == '\r' || bytes[offset] == '\n')) {
        ++offset;
    }
    const auto has_prefix = [&](const char *prefix, size_t length) {
        return static_cast<uint64_t>(data.size() - offset) >= length &&
                std::memcmp(bytes + offset, prefix, length) == 0;
    };

    if (has_prefix("<svg", 4) || has_prefix("<?xml", 5)) {
        file_type = TYPE_SVG;
        rive_svg.instantiate();
        String xml_content;
        xml_content.parse_utf8((const char*)data.ptr(), data.size());
        rive_svg->parse(xml_content);
        return OK;
    }

    file_type = TYPE_RIVE;
    bool factory_ready = false;
    rive::ImportResult result{};
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (!factory) return;
        factory_ready = true;
        rive::Span<const uint8_t> bytes(data.ptr(), data.size());
        rive_file = rive::File::import(bytes, factory, &result);
    });

    if (!executed || !factory_ready) {
        return ERR_UNAVAILABLE;
    }

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
    
    if (name.is_empty() && !artboard && rive_file->artboardCount() > 0) {
        artboard = rive_file->artboardAt(0);
    }
    
    return artboard;
}
