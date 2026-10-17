#ifndef RIVE_FILE_H
#define RIVE_FILE_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "../renderer/rive_render_registry.h"
#include "../rive_svg.h"

using namespace godot;
class RiveViewModelInstance;

class RiveFile : public Resource {
    GDCLASS(RiveFile, Resource);

public:
    enum Type {
        TYPE_RIVE,
        TYPE_SVG
    };

private:
    PackedByteArray data;
    rive::rcp<rive::File> rive_file;
    Ref<RiveSVG> rive_svg;
    Type file_type = TYPE_RIVE;
    Dictionary asset_overrides;
    Array asset_metadata;
    Error last_load_error = OK;
    String last_load_message;
    bool resolving_assets = false;
    Array observed_assets;

protected:
    static void _bind_methods();

public:
    RiveFile();
    ~RiveFile();

    void set_data(const PackedByteArray &p_data);
    PackedByteArray get_data() const;
    void set_asset_overrides(const Dictionary &p_assets);
    Dictionary get_asset_overrides() const;
    Array get_asset_metadata();
    PackedStringArray get_external_dependencies() const;
    Dictionary resolve_assets(const Callable &p_resolver);
    void reload_assets();
    PackedStringArray get_view_model_names();
    PackedStringArray get_artboard_names();
    PackedStringArray get_animation_names(const String &p_artboard = "");
    PackedStringArray get_state_machine_names(const String &p_artboard = "");
    Vector2 get_intrinsic_size(const String &p_artboard = "");
    String get_load_error() const { return last_load_message; }
    int get_load_error_code() const { return last_load_error; }
    PackedStringArray get_global_view_model_names();
    PackedStringArray get_view_model_instance_names(const String &p_view_model);
    Array get_view_model_schema(const String &p_view_model);
    Ref<RiveViewModelInstance> instantiate_view_model(const String &p_view_model,
            const String &p_instance_name = "", int p_instance_index = -1, bool p_blank = false);
    Ref<RiveViewModelInstance> instantiate_view_model_for_artboard(const String &p_artboard = "",
            const String &p_instance_name = "", int p_instance_index = -1, bool p_blank = false);

    Error load_rive_file();
    rive::File* get_rive_file();
    rive::File* _get_cached_rive_file() const { return rive_file.get(); }
    
    std::unique_ptr<rive::ArtboardInstance> instantiate_artboard(String name = "");
};

#endif
