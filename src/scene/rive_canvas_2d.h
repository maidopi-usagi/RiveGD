#ifndef RIVE_CANVAS_2D_H
#define RIVE_CANVAS_2D_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include "../rive_render_registry.h"
#include <rive/renderer.hpp>

using namespace godot;

class RiveCanvas2D : public Node2D, public RiveDrawable {
    GDCLASS(RiveCanvas2D, Node2D);

private:
    Ref<Texture2DRD> texture_rd;
    RID texture_rid;
    Vector2i size = Vector2i(512, 512);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveCanvas2D();
    ~RiveCanvas2D();

    void set_size(const Vector2i &p_size);
    Vector2i get_size() const;

    // RiveDrawable implementation
    void draw(rive::Renderer *renderer) override;
    
    // Godot overrides
    void _process(double delta) override;
    void _draw() override;
};

#endif // RIVE_CANVAS_2D_H
