#ifndef RIVE_FILE_H
#define RIVE_FILE_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive_render_registry.h"
#include "../rive_svg.h"

using namespace godot;

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

protected:
    static void _bind_methods();

public:
    RiveFile();
    ~RiveFile();

    void set_data(const PackedByteArray &p_data);
    PackedByteArray get_data() const;

    Error load_rive_file();
    rive::File* get_rive_file();
    
    std::unique_ptr<rive::ArtboardInstance> instantiate_artboard(String name = "");
};

#endif // RIVE_FILE_H
