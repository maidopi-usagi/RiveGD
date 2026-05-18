#include "godot_file_asset_loader.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "rive/simple_array.hpp"

using namespace godot;
using namespace rive_integration;

GodotFileAssetLoader::GodotFileAssetLoader(const String& riv_path) {
    // Extract the directory portion of the .riv file path
    String dir = riv_path.get_base_dir();
    // Ensure trailing slash for concatenation
    if (!dir.ends_with("/")) {
        dir += "/";
    }
    m_basePath = dir.utf8().get_data();
}

bool GodotFileAssetLoader::loadContents(rive::FileAsset& asset,
                                         rive::Span<const uint8_t> inBandBytes,
                                         rive::Factory* factory) {
    // If the asset already has in-band data, let Rive handle it
    if (inBandBytes.size() > 0) {
        return false;
    }

    // Build the full path: base_dir + uniqueFilename
    std::string uniqueFilename = asset.uniqueFilename();
    if (uniqueFilename.empty()) {
        UtilityFunctions::push_warning("RiveGD: Out-of-band asset has no unique filename, cannot load");
        return false;
    }

    String godotPath = String::utf8(m_basePath.c_str()) + String::utf8(uniqueFilename.c_str());

    // Try to open the file from Godot's virtual filesystem
    Ref<FileAccess> file = FileAccess::open(godotPath, FileAccess::READ);
    if (file.is_null()) {
        UtilityFunctions::push_warning("RiveGD: Could not find out-of-band asset at '", godotPath, "'");
        return false;
    }

    // Read the file contents
    PackedByteArray data = file->get_buffer(file->get_length());
    if (data.size() == 0) {
        UtilityFunctions::push_warning("RiveGD: Out-of-band asset file is empty: '", godotPath, "'");
        return false;
    }

    // Copy into a SimpleArray and decode
    rive::SimpleArray<uint8_t> bytes(data.size());
    memcpy(bytes.data(), data.ptr(), data.size());
    asset.decode(bytes, factory);

    UtilityFunctions::print_verbose("RiveGD: Loaded out-of-band asset '",
        String::utf8(uniqueFilename.c_str()), "' from '", godotPath, "'");
    return true;
}
