#pragma once

#include "rive/file_asset_loader.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/factory.hpp"
#include <string>

namespace godot {
    class String;
}

namespace rive_integration {

/// A FileAssetLoader that loads out-of-band assets (fonts, images) from Godot's
/// res:// filesystem. When a .riv file references external resources, this
/// loader resolves them relative to the .riv file's location.
class GodotFileAssetLoader : public rive::FileAssetLoader {
public:
    /// Construct with the .riv file's res:// path. The base directory is
    /// extracted automatically (e.g. "res://animations/file.riv" → "res://animations/").
    GodotFileAssetLoader(const godot::String& riv_path);

    bool loadContents(rive::FileAsset& asset,
                      rive::Span<const uint8_t> inBandBytes,
                      rive::Factory* factory) override;

private:
    std::string m_basePath;  // res:// directory of the .riv file (UTF-8)
};

} // namespace rive_integration
