#ifndef RIVE_TYPES_H
#define RIVE_TYPES_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include "rive/renderer.hpp"
#include "rive/factory.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/text_engine.hpp"
#include "../renderer/rive_render_registry.h"
#include <vector>

using namespace godot;

namespace rive {
    class RawPath;
}

class RiveSVG;

class RivePath : public RefCounted {
    GDCLASS(RivePath, RefCounted);

private:
    std::unique_ptr<rive::RawPath> raw_path;
    rive::rcp<rive::RenderPath> render_path;
    bool is_dirty = true;
    int fill_rule = FILL_NON_ZERO;

protected:
    static void _bind_methods();

public:
    enum FillRule {
        FILL_NON_ZERO = 0,
        FILL_EVEN_ODD = 1,
        FILL_CLOCKWISE = 2,
    };

    RivePath();
    ~RivePath();

    void move_to(float x, float y);
    void line_to(float x, float y);
    void quad_to(float cx, float cy, float x, float y);
    void cubic_to(float ox, float oy, float ix, float iy, float x, float y);
    void close();
    void reset();
    void add_rect(float x, float y, float w, float h);
    bool add_round_rect(float x, float y, float w, float h, float radius);
    void add_oval(float x, float y, float w, float h);
    void add_circle(float cx, float cy, float radius);
    bool add_arc(float cx, float cy, float radius, float start_angle, float sweep_angle);
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
    Vector2 linear_start;
    Vector2 linear_end;
    Vector2 radial_center;
    float radial_radius = 0.0f;
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
    rive::rcp<rive::RenderShader> applied_shader;
    bool dirty = true;
    
    Color color = Color(0, 0, 0, 1);
    float thickness = 1.0f;
    int style = STYLE_FILL;
    int join = JOIN_MITER;
    int cap = CAP_BUTT;
    int blend_mode = static_cast<int>(rive::BlendMode::srcOver);
    int stroke_position = STROKE_CENTER;
    float additiveness = 0.0f;
    float feather = 0.0f;
    Ref<RiveGradient> gradient;

    void _initialize_paint_snapshot();

protected:
    static void _bind_methods();

public:
    enum Style {
        STYLE_STROKE = 0,
        STYLE_FILL = 1,
    };
    enum Join {
        JOIN_MITER = 0,
        JOIN_ROUND = 1,
        JOIN_BEVEL = 2,
    };
    enum Cap {
        CAP_BUTT = 0,
        CAP_ROUND = 1,
        CAP_SQUARE = 2,
    };
    enum StrokePosition { STROKE_INSIDE = 0, STROKE_CENTER = 1, STROKE_OUTSIDE = 2 };

    RivePaint();
    ~RivePaint();

    void set_color(Color color);
    void set_thickness(float thickness);
    void set_style(int style);
    void set_join(int join);
    void set_cap(int cap);
    void set_blend_mode(int blend_mode);
    void set_stroke_position(int position);
    void set_additiveness(float amount);
    void set_feather(float feather);
    void set_gradient(Ref<RiveGradient> gradient);
    Ref<RiveGradient> get_gradient() const { return gradient; }

    rive::RenderPaint* get_render_paint(rive::Factory* factory);
};

class RiveRendererWrapper : public RefCounted {
    GDCLASS(RiveRendererWrapper, RefCounted);

private:
    rive::Renderer* renderer = nullptr;

protected:
    static void _bind_methods();

public:
    enum ImageFilter { FILTER_LINEAR = 0, FILTER_NEAREST = 1 };
    enum ImageWrap { WRAP_CLAMP = 0, WRAP_REPEAT = 1, WRAP_MIRROR = 2 };
    void set_renderer(rive::Renderer* p_renderer) { renderer = p_renderer; }
    rive::Renderer* get_renderer() const { return renderer; }
    
    void save();
    void restore();
    void transform(float xx, float xy, float yx, float yy, float tx, float ty);
    void translate(float x, float y);
    void scale(float x, float y);
    void rotate(float angle);
    void modulate_opacity(float opacity);
    void modulate_color(Color color, bool replace = false);
    void clip_stroke(Ref<RivePath> path, float thickness, int join = 0, int cap = 0, int position = 1);
    void draw_path(Ref<RivePath> path, Ref<RivePaint> paint);
    void clip_path(Ref<RivePath> path);
    static int make_image_sampler(int filter, int wrap_x = 0, int wrap_y = 0);
    void draw_image(Ref<class RiveImage> image, float opacity, int blend_mode, int sampler_key = 0);
    void draw_image_mesh(Ref<class RiveImage> image,
                         PackedVector2Array vertices,
                         PackedVector2Array uvs,
                         PackedInt32Array indices,
                         float opacity,
                         int blend_mode, int sampler_key = 0);
    bool draw_image_mesh_instanced(Ref<class RiveImage> image, Ref<class RiveMesh> mesh,
                                   Array transforms, PackedFloat32Array opacities = PackedFloat32Array(), int sampler_key = 0);
};

class RiveMesh : public RefCounted {
    GDCLASS(RiveMesh, RefCounted);
    friend class RiveRendererWrapper;
    PackedVector2Array vertices, uvs;
    PackedInt32Array indices;
    rive::rcp<rive::RenderBuffer> vertex_buffer, uv_buffer, index_buffer;
    bool dirty = true;
    bool ensure_buffers(rive::Factory *factory);
protected:
    static void _bind_methods();
public:
    ~RiveMesh();
    bool set_geometry(PackedVector2Array p_vertices, PackedVector2Array p_uvs, PackedInt32Array p_indices);
    int get_vertex_count() const { return vertices.size(); }
    int get_index_count() const { return indices.size(); }
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
    Array get_axes() const;
    PackedStringArray get_features() const;
    Ref<RiveFont> with_options(const Dictionary &variations, const Dictionary &features) const;
    static bool set_fallback_fonts(const Array &fonts);
    static void clear_fallback_fonts();
    rive::rcp<rive::Font> get_font() const { return font; }
};

class RiveText : public RefCounted {
    GDCLASS(RiveText, RefCounted);

private:
    std::unique_ptr<rive::RawText> raw_text;
    Vector<Ref<RivePaint>> run_paints;
    Vector<Ref<RiveFont>> run_fonts;
    Vector<String> run_texts;
    Vector<float> run_sizes;
    Vector<float> run_line_heights;
    Vector<float> run_letter_spacings;
    std::vector<rive::rcp<rive::RenderPaint>> run_native_paints;
    int sizing = static_cast<int>(rive::TextSizing::autoWidth);
    int overflow = static_cast<int>(rive::TextOverflow::visible);
    int align = static_cast<int>(rive::TextAlign::left);
    float max_width = 0.0f;
    float max_height = 0.0f;
    float paragraph_spacing = 0.0f;
    int direction = DIRECTION_AUTO;
    int logical_align = LOGICAL_NONE;
    int wrap = WRAP_ENABLED;
    int word_break = BREAK_WORD;
    bool dirty = true;

    void _ensure_raw_text();
    void _apply_props();
    void _refresh_run_paints();

protected:
    static void _bind_methods();

public:
    enum Direction { DIRECTION_AUTO = -1, DIRECTION_LTR = 0, DIRECTION_RTL = 1 };
    enum LogicalAlign { LOGICAL_NONE = 0, LOGICAL_START = 1, LOGICAL_END = 2 };
    enum Wrap { WRAP_ENABLED = 0, WRAP_DISABLED = 1 };
    enum WordBreak { BREAK_WORD = 0, BREAK_NORMAL = 1, BREAK_ALL = 2 };
    RiveText();
    ~RiveText();

    void clear();
    void append_run(String text, Ref<RiveFont> font, Ref<RivePaint> paint, float size, float line_height, float letter_spacing);
    void render(Ref<RiveRendererWrapper> renderer, Ref<RivePaint> override_paint);
    Rect2 get_bounds();
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
    void set_direction(int value);
    int get_direction() const { return direction; }
    void set_logical_align(int value);
    int get_logical_align() const { return logical_align; }
    void set_wrap(int value);
    int get_wrap() const { return wrap; }
    void set_word_break(int value);
    int get_word_break() const { return word_break; }
    Array get_lines();
};

VARIANT_ENUM_CAST(RivePath::FillRule);
VARIANT_ENUM_CAST(RivePaint::Style);
VARIANT_ENUM_CAST(RivePaint::Join);
VARIANT_ENUM_CAST(RivePaint::Cap);
VARIANT_ENUM_CAST(RivePaint::StrokePosition);
VARIANT_ENUM_CAST(RiveRendererWrapper::ImageFilter);
VARIANT_ENUM_CAST(RiveRendererWrapper::ImageWrap);
VARIANT_ENUM_CAST(RiveText::Direction);
VARIANT_ENUM_CAST(RiveText::LogicalAlign);
VARIANT_ENUM_CAST(RiveText::Wrap);
VARIANT_ENUM_CAST(RiveText::WordBreak);

#endif
