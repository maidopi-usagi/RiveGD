#ifndef RIVE_SVG_H
#define RIVE_SVG_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/xml_parser.hpp>
#include "resources/rive_types.h"
#include "rive/math/mat2d.hpp"

// Forward declarations
namespace rive {
    class Artboard;
    class ArtboardInstance;
}

using namespace godot;

class RiveSVG : public RefCounted {
    GDCLASS(RiveSVG, RefCounted);

public:
    struct Shape {
        Ref<RivePath> path;
        Ref<RivePaint> paint;
        rive::Mat2D transform;
        float cx, cy; // Center for animation
    };

    struct Data : public RefCounted {
        Vector<Shape> shapes;
    };

private:
    Ref<Data> data;

    std::unique_ptr<rive::Artboard> source_artboard;
    std::unique_ptr<rive::ArtboardInstance> artboard_instance;
    Ref<RiveSVG> base_svg; // Keep source alive if this is an instance

protected:
    static void _bind_methods();

public:
    RiveSVG();
    ~RiveSVG();

    void parse(String xml_content);
    void load_file(String path);
    void draw(Ref<RiveRendererWrapper> renderer);
    
    std::unique_ptr<rive::ArtboardInstance> instantiate_artboard();
    Ref<RiveSVG> instance();
};

#endif // RIVE_SVG_H
