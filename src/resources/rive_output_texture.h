#ifndef RIVE_OUTPUT_TEXTURE_H
#define RIVE_OUTPUT_TEXTURE_H

#include <godot_cpp/classes/texture2d.hpp>
#include "../renderer/rive_texture_target.h"

using namespace godot;

class RiveOutputTexture : public Texture2D {
    GDCLASS(RiveOutputTexture, Texture2D);
    Ref<RiveTextureTarget> target;

protected:
    static void _bind_methods();

public:
    void set_target(const Ref<RiveTextureTarget> &p_target) { target = p_target; }
    void notify_target_changed() { emit_changed(); }
    RID _get_rid() const override;
    int32_t _get_width() const override;
    int32_t _get_height() const override;
    bool _has_alpha() const override { return true; }
    Ref<Image> get_image() const;
    void _draw(const RID &p_canvas_item, const Vector2 &p_position, const Color &p_modulate, bool p_transpose) const override;
    void _draw_rect(const RID &p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) const override;
    void _draw_rect_region(const RID &p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) const override;
};

#endif
