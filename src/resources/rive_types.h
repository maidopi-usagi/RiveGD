#ifndef RIVE_TYPES_H
#define RIVE_TYPES_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include "rive/renderer.hpp"
#include "rive/factory.hpp"
#include "rive/math/mat2d.hpp"
#include "../renderer/rive_render_registry.h"

using namespace godot;

namespace rive {
    class RawPath;
}

class RiveSVG; // Forward declaration

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
    void quad_to(float cx, float cy, float x, float y);
    void cubic_to(float ox, float oy, float ix, float iy, float x, float y);
    void close();
    void reset();
    void add_rect(float x, float y, float w, float h);
    void add_oval(float x, float y, float w, float h);
    void add_circle(float cx, float cy, float radius);
    void add_path(Ref<RivePath> other);
    void add_path_transformed(Ref<RivePath> other, Transform2D xform);
    void add_poly(PackedVector2Array points, bool closed);
    Rect2 get_bounds() const;
    bool is_empty() const;
    Ref<RivePath> morph(Callable proc) const;
    void set_fill_rule(int rule);
    
    void parse_svg(String path_data);

    rive::RenderPath* get_render_path(rive::Factory* factory);
};

class RiveGradient : public RefCounted {
    GDCLASS(RiveGradient, RefCounted);

private:
    rive::rcp<rive::RenderShader> shader;
    bool is_radial = false;
    float sx = 0, sy = 0, ex = 0, ey = 0; // linear: start->end; radial: (sx,sy) center, ex radius
    PackedColorArray colors;
    PackedFloat32Array stops;
    bool dirty = true;

protected:
    static void _bind_methods();

public:
    void set_linear(Vector2 from, Vector2 to);
    void set_radial(Vector2 center, float radius);
    void set_stops(PackedColorArray p_colors, PackedFloat32Array p_stops);

    rive::RenderShader* get_shader(rive::Factory* factory);
    rive::rcp<rive::RenderShader> get_shader_rcp(rive::Factory* factory);
};

class RivePaint : public RefCounted {
    GDCLASS(RivePaint, RefCounted);

private:
    rive::rcp<rive::RenderPaint> render_paint;
    
    // Cached properties
    Color color = Color(0, 0, 0, 1);
    float thickness = 1.0f;
    int style = 1; // Fill
    int join = 0;
    int cap = 0;
    int blend_mode = 3; // SrcOver
    float feather = 0.0f;
    Ref<RiveGradient> gradient;

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
    void set_feather(float feather);
    void set_gradient(Ref<RiveGradient> gradient);
    Ref<RiveGradient> get_gradient() const { return gradient; }

    rive::RenderPaint* get_render_paint(rive::Factory* factory);
    void _apply_properties();
};

class RiveRendererWrapper : public RefCounted {
    GDCLASS(RiveRendererWrapper, RefCounted);

private:
    rive::Renderer* renderer = nullptr;

protected:
    static void _bind_methods();

public:
    void set_renderer(rive::Renderer* p_renderer) { renderer = p_renderer; }
    rive::Renderer* get_renderer() const { return renderer; }
    
    void save();
    void restore();
    void transform(float xx, float xy, float yx, float yy, float tx, float ty);
    void translate(float x, float y);
    void scale(float x, float y);
    void rotate(float angle);
    void draw_path(Ref<RivePath> path, Ref<RivePaint> paint);
    void clip_path(Ref<RivePath> path);
    void draw_image(Ref<class RiveImage> image, float opacity, int blend_mode);
    void draw_image_mesh(Ref<class RiveImage> image,
                         PackedVector2Array vertices,
                         PackedVector2Array uvs,
                         PackedInt32Array indices,
                         float opacity,
                         int blend_mode);
};

class RiveImage : public RefCounted {
    GDCLASS(RiveImage, RefCounted);

private:
    rive::rcp<rive::RenderImage> render_image;
    int width = 0;
    int height = 0;

protected:
    static void _bind_methods();

public:
    ~RiveImage();

    bool load_from_buffer(PackedByteArray bytes);
    bool load_from_texture(Ref<Texture2D> texture);

    int get_width() const { return width; }
    int get_height() const { return height; }
    bool is_loaded() const { return render_image.get() != nullptr; }

    rive::RenderImage* get_render_image() const { return render_image.get(); }
};

namespace rive {
    class Font;
    class RawText;
}

class RiveFont : public RefCounted {
    GDCLASS(RiveFont, RefCounted);

private:
    rive::rcp<rive::Font> font;

protected:
    static void _bind_methods();

public:
    bool load_from_buffer(PackedByteArray bytes);
    bool load_from_file(String path);
    bool is_loaded() const { return font.get() != nullptr; }
    int get_weight() const;
    bool is_italic() const;
    Array shape_text(String text, float size);
    rive::rcp<rive::Font> get_font() const { return font; }
};

class RiveText : public RefCounted {
    GDCLASS(RiveText, RefCounted);

private:
    std::unique_ptr<rive::RawText> raw_text;
    // Hold refs so paints/fonts stay alive across frames.
    Vector<Ref<RivePaint>> run_paints;
    Vector<Ref<RiveFont>> run_fonts;
    Vector<String> run_texts;
    Vector<float> run_sizes;
    Vector<float> run_letter_spacings;
    int sizing = 0;       // autoWidth / autoHeight / fixed
    int overflow = 0;     // visible / hidden / clipped / ellipsis / fit
    int align = 0;        // left / right / center
    float max_width = 0.0f;
    float max_height = 0.0f;
    float paragraph_spacing = 0.0f;
    bool dirty = true;

    void _ensure_raw_text();
    void _apply_props();

protected:
    static void _bind_methods();

public:
    RiveText();
    ~RiveText();

    void clear();
    void append_run(String text, Ref<RiveFont> font, Ref<RivePaint> paint, float size, float line_height, float letter_spacing);
    void render(Ref<RiveRendererWrapper> renderer, Ref<RivePaint> override_paint);
    Rect2 get_bounds();
    // Re-shape all stored runs on a single baseline and return per-glyph data:
    // [{path: RivePath, paint: RivePaint, x, y, advance, glyph_id}]. Useful for
    // GDScript-side morphing/warping. Ignores wrap/align/sizing.
    Array shape_glyphs();

    void set_sizing(int v);
    void set_overflow(int v);
    void set_align(int v);
    void set_max_width(float v);
    void set_max_height(float v);
    void set_paragraph_spacing(float v);
    int get_sizing() const { return sizing; }
    int get_overflow() const { return overflow; }
    int get_align() const { return align; }
    float get_max_width() const { return max_width; }
    float get_max_height() const { return max_height; }
    float get_paragraph_spacing() const { return paragraph_spacing; }
};

#endif
