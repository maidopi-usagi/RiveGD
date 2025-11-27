#ifndef RIVE_NODE_H
#define RIVE_NODE_H

#include <godot_cpp/classes/node2d.hpp>
#include <rive/renderer.hpp>

using namespace godot;

class RiveNode : public Node2D {
    GDCLASS(RiveNode, Node2D);

protected:
    static void _bind_methods() {}

public:
    virtual void draw(rive::Renderer* renderer) {}
    virtual void advance(double delta) {}
    virtual Rect2 get_rive_bounds() const { return Rect2(); }
    virtual void pointer_down(Vector2 position) {}
    virtual void pointer_up(Vector2 position) {}
    virtual void pointer_move(Vector2 position) {}
};

#endif
