#ifndef RIVE_CANVAS_H
#define RIVE_CANVAS_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2drd.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>

#include "rive/renderer.hpp"
#include "rive/factory.hpp"
#include "rive/math/mat2d.hpp"
#include "rive_render_registry.h"

using namespace godot;

namespace rive {
    class RawPath;
}

class RivePath : public RefCounted {
    GDCLASS(RivePath, RefCounted);

private:
    std::unique_ptr<rive::RawPath> raw_path;
    rive::rcp<rive::RenderPath> render_path;
    bool is_dirty = true;
    int fill_rule = 0; // 0: nonZero, 1: evenOdd

protected:
    static void _bind_methods();

public:
    RivePath();
    ~RivePath();

    void move_to(float x, float y);
    void line_to(float x, float y);
    void cubic_to(float ox, float oy, float ix, float iy, float x, float y);
    void close();
    void reset();
    void set_fill_rule(int rule);
    
    void parse_svg(String path_data);

    rive::RenderPath* get_render_path(rive::Factory* factory);
};

class RivePaint : public RefCounted {
    GDCLASS(RivePaint, RefCounted);

private:
    rive::rcp<rive::RenderPaint> render_paint;

protected:
    static void _bind_methods();

public:
    RivePaint();
    ~RivePaint();

    void set_color(Color color);
    void set_thickness(float thickness);
    void set_style(int style); // 0: Stroke, 1: Fill
    void set_join(int join);
    void set_cap(int cap);
    void set_blend_mode(int blend_mode);

    rive::RenderPaint* get_render_paint(rive::Factory* factory);
};

class RiveRendererWrapper : public RefCounted {
    GDCLASS(RiveRendererWrapper, RefCounted);

    friend class RiveSVG;

private:
    rive::Renderer* renderer = nullptr;

protected:
    static void _bind_methods();

public:
    void set_renderer(rive::Renderer* p_renderer) { renderer = p_renderer; }
    
    void save();
    void restore();
    void transform(float xx, float xy, float yx, float yy, float tx, float ty);
    void translate(float x, float y);
    void scale(float x, float y);
    void rotate(float angle);
    void draw_path(Ref<RivePath> path, Ref<RivePaint> paint);
};

class RiveCanvas : public Control, public RiveDrawable {
    GDCLASS(RiveCanvas, Control);

private:
    RID texture_rid;
    Ref<Texture2DRD> texture_rd_ref;
    Size2i texture_size;
    
    Ref<RiveRendererWrapper> renderer_wrapper;

    void _render_rive();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveCanvas();
    ~RiveCanvas();

    // RiveDrawable implementation
    void draw(rive::Renderer* renderer) override;
};

#endif // RIVE_CANVAS_H
