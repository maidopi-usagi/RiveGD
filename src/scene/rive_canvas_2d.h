#ifndef RIVE_CANVAS_2D_H
#define RIVE_CANVAS_2D_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_texture_target.h"
#include "rive_node.h"
#include <rive/renderer.hpp>

using namespace godot;

class RiveCanvas2D : public Node2D, public RiveDrawable {
    GDCLASS(RiveCanvas2D, Node2D);

private:
    Ref<RiveTextureTarget> texture_target;
    Vector2i size = Vector2i(512, 512);
    
    LocalVector<RiveNode*> active_nodes;
    double current_delta = 0.0;

    void _advance_node(uint32_t p_index);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveCanvas2D();
    ~RiveCanvas2D();

    void set_size(const Vector2i &p_size);
    Vector2i get_size() const;
    
    Ref<Texture2D> get_texture() const;

    void draw(rive::Renderer *renderer) override;
    
    void _process(double delta) override;
    void _draw() override;
};

#endif // RIVE_CANVAS_2D_H
