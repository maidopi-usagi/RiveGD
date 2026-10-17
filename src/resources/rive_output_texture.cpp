#include "rive_output_texture.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

void RiveOutputTexture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("read_image"), &RiveOutputTexture::get_image);
}

RID RiveOutputTexture::_get_rid() const {
    if (target.is_null() || !target->is_valid()) return RID();
    const Ref<Texture2DRD> rd_texture = target->get_texture_rd();
    return rd_texture.is_valid() ? rd_texture->get_rid() : target->get_texture_rid();
}

int32_t RiveOutputTexture::_get_width() const {
    return target.is_valid() ? target->get_size().x : 0;
}

int32_t RiveOutputTexture::_get_height() const {
    return target.is_valid() ? target->get_size().y : 0;
}

Ref<Image> RiveOutputTexture::get_image() const {
    RenderingServer *rs = RenderingServer::get_singleton();
    const RID rid = _get_rid();
    return rs && rid.is_valid() ? rs->texture_2d_get(rid) : Ref<Image>();
}

void RiveOutputTexture::_draw(const RID &p_canvas_item, const Vector2 &p_position, const Color &p_modulate, bool p_transpose) const {
    _draw_rect(p_canvas_item, Rect2(p_position, Vector2(_get_width(), _get_height())), false, p_modulate, p_transpose);
}

void RiveOutputTexture::_draw_rect(const RID &p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) const {
    RenderingServer *rs = RenderingServer::get_singleton();
    const RID rid = _get_rid();
    if (rs && rid.is_valid()) rs->canvas_item_add_texture_rect(p_canvas_item, p_rect, rid, p_tile, p_modulate, p_transpose);
}

void RiveOutputTexture::_draw_rect_region(const RID &p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) const {
    RenderingServer *rs = RenderingServer::get_singleton();
    const RID rid = _get_rid();
    if (rs && rid.is_valid()) rs->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, rid, p_src_rect, p_modulate, p_transpose, p_clip_uv);
}
