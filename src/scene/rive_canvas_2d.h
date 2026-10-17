#ifndef RIVE_CANVAS_2D_H
#define RIVE_CANVAS_2D_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include "../renderer/rive_render_registry.h"
#include "../renderer/rive_texture_target.h"
#include "rive_node.h"
#include "../resources/rive_output_texture.h"
#include <rive/renderer.hpp>

using namespace godot;

class RiveCanvas2D : public Node2D, public RiveDrawable {
    GDCLASS(RiveCanvas2D, Node2D);

private:
    Ref<RiveTextureTarget> texture_target;
    Ref<RiveOutputTexture> output_texture;
    bool render_dirty = true;
    Vector2i size = Vector2i(512, 512);
    Rect2 cull_rect = Rect2(0, 0, 0, 0);
    bool cull_enabled = true;

    LocalVector<ObjectID> active_nodes;
    LocalVector<Transform2D> active_parent_xforms;
    LocalVector<Transform2D> last_node_xforms;
    LocalVector<ObjectID> last_nodes;
    LocalVector<bool> last_visibility;
    double current_delta = 0.0;

    void _advance_node(uint32_t p_index);
    void _collect_descendants(Node *parent, const Transform2D &parent_xform_in_canvas);
    RiveNode *_resolve_active_node(uint32_t p_index) const;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveCanvas2D();
    ~RiveCanvas2D();

    void set_size(const Vector2i &p_size);
    Vector2i get_size() const;

    void set_cull_rect(const Rect2 &p_rect);
    Rect2 get_cull_rect() const;
    void set_cull_enabled(bool p_enabled);
    bool is_cull_enabled() const;

    Ref<Texture2D> get_texture() const;
    Ref<Image> read_image() const;

    void draw(rive::Renderer *renderer) override;
    
    void _process(double delta) override;
    void _draw() override;
    PackedStringArray _get_configuration_warnings() const override;
};

#endif
