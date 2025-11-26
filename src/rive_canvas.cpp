#include "rive_canvas.h"
#include "rive/math/raw_path.hpp"
#include "rive_renderer.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/image.hpp>
#include <map>
#include <cmath>

using namespace godot;

// --- RivePath ---

RivePath::RivePath() {
    raw_path = std::make_unique<rive::RawPath>();
}

RivePath::~RivePath() {}

void RivePath::_bind_methods() {
    ClassDB::bind_method(D_METHOD("move_to", "x", "y"), &RivePath::move_to);
    ClassDB::bind_method(D_METHOD("line_to", "x", "y"), &RivePath::line_to);
    ClassDB::bind_method(D_METHOD("cubic_to", "ox", "oy", "ix", "iy", "x", "y"), &RivePath::cubic_to);
    ClassDB::bind_method(D_METHOD("close"), &RivePath::close);
    ClassDB::bind_method(D_METHOD("reset"), &RivePath::reset);
    ClassDB::bind_method(D_METHOD("set_fill_rule", "rule"), &RivePath::set_fill_rule);
}

void RivePath::move_to(float x, float y) {
    raw_path->moveTo(x, y);
    is_dirty = true;
}

void RivePath::line_to(float x, float y) {
    raw_path->lineTo(x, y);
    is_dirty = true;
}

void RivePath::cubic_to(float ox, float oy, float ix, float iy, float x, float y) {
    raw_path->cubicTo(ox, oy, ix, iy, x, y);
    is_dirty = true;
}

void RivePath::close() {
    raw_path->close();
    is_dirty = true;
}

void RivePath::reset() {
    raw_path->reset();
    is_dirty = true;
}

void RivePath::set_fill_rule(int rule) {
    fill_rule = rule;
    is_dirty = true;
}

static bool local_is_digit(char32_t c) {
    return c >= '0' && c <= '9';
}

static bool is_number_start(char32_t c) {
    return local_is_digit(c) || c == '+' || c == '-' || c == '.';
}

static float read_float(const String& d, int& i) {
    // Skip whitespace/commas
    while (i < d.length() && (d[i] == ' ' || d[i] == ',' || d[i] == '\t' || d[i] == '\n')) i++;
    
    int start = i;
    if (i < d.length() && (d[i] == '-' || d[i] == '+')) i++;
    while (i < d.length() && local_is_digit(d[i])) i++;
    if (i < d.length() && d[i] == '.') {
        i++;
        while (i < d.length() && local_is_digit(d[i])) i++;
    }
    if (i < d.length() && (d[i] == 'e' || d[i] == 'E')) {
        i++;
        if (i < d.length() && (d[i] == '-' || d[i] == '+')) i++;
        while (i < d.length() && local_is_digit(d[i])) i++;
    }
    
    if (start == i) return 0.0f;
    return d.substr(start, i - start).to_float();
}

static void svg_arc_to(RivePath* path, float rx, float ry, float angle, bool large_arc_flag, bool sweep_flag, float x, float y, float cur_x, float cur_y) {
    if (rx == 0 || ry == 0) {
        path->line_to(x, y);
        return;
    }
    
    rx = std::abs(rx);
    ry = std::abs(ry);
    
    float angle_rad = Math::deg_to_rad(angle);
    float cos_phi = std::cos(angle_rad);
    float sin_phi = std::sin(angle_rad);
    
    float dx = (cur_x - x) / 2.0f;
    float dy = (cur_y - y) / 2.0f;
    
    float x1_p = cos_phi * dx + sin_phi * dy;
    float y1_p = -sin_phi * dx + cos_phi * dy;
    
    float rx_sq = rx * rx;
    float ry_sq = ry * ry;
    float x1_p_sq = x1_p * x1_p;
    float y1_p_sq = y1_p * y1_p;
    
    float check = x1_p_sq / rx_sq + y1_p_sq / ry_sq;
    if (check > 1.0f) {
        float scale = std::sqrt(check);
        rx *= scale;
        ry *= scale;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }
    
    float sign = (large_arc_flag == sweep_flag) ? -1.0f : 1.0f;
    float num = rx_sq * ry_sq - rx_sq * y1_p_sq - ry_sq * x1_p_sq;
    float den = rx_sq * y1_p_sq + ry_sq * x1_p_sq;
    num = std::max(num, 0.0f); // Prevent negative due to precision
    float coef = sign * std::sqrt(num / den);
    
    float cx_p = coef * (rx * y1_p / ry);
    float cy_p = coef * -(ry * x1_p / rx);
    
    float cx_c = cos_phi * cx_p - sin_phi * cy_p + (cur_x + x) / 2.0f;
    float cy_c = sin_phi * cx_p + cos_phi * cy_p + (cur_y + y) / 2.0f;
    
    float theta1 = std::atan2((y1_p - cy_p) / ry, (x1_p - cx_p) / rx);
    float theta2 = std::atan2((-y1_p - cy_p) / ry, (-x1_p - cx_p) / rx);
    
    float d_theta = theta2 - theta1;
    if (sweep_flag && d_theta < 0) d_theta += Math_TAU;
    else if (!sweep_flag && d_theta > 0) d_theta -= Math_TAU;
    
    int segments = (int)(std::ceil(std::abs(d_theta) / (Math_PI / 2.0f)));
    float delta = d_theta / segments;
    float t = 8.0f / 3.0f * std::sin(delta / 4.0f) * std::sin(delta / 4.0f) / std::sin(delta / 2.0f);
    
    float cos_theta1 = std::cos(theta1);
    float sin_theta1 = std::sin(theta1);
    
    for (int i = 0; i < segments; ++i) {
        float theta2_seg = theta1 + delta;
        float cos_theta2 = std::cos(theta2_seg);
        float sin_theta2 = std::sin(theta2_seg);
        
        float e_px = cos_theta1 - t * sin_theta1;
        float e_py = sin_theta1 + t * cos_theta1;
        float e_x = rx * e_px;
        float e_y = ry * e_py;
        
        float cp1_x = cos_phi * e_x - sin_phi * e_y + cx_c;
        float cp1_y = sin_phi * e_x + cos_phi * e_y + cy_c;
        
        e_px = cos_theta2 + t * sin_theta2;
        e_py = sin_theta2 - t * cos_theta2;
        e_x = rx * e_px;
        e_y = ry * e_py;
        
        float cp2_x = cos_phi * e_x - sin_phi * e_y + cx_c;
        float cp2_y = sin_phi * e_x + cos_phi * e_y + cy_c;
        
        e_x = rx * cos_theta2;
        e_y = ry * sin_theta2;
        float p2_x = cos_phi * e_x - sin_phi * e_y + cx_c;
        float p2_y = sin_phi * e_x + cos_phi * e_y + cy_c;
        
        path->cubic_to(cp1_x, cp1_y, cp2_x, cp2_y, p2_x, p2_y);
        
        theta1 = theta2_seg;
        cos_theta1 = cos_theta2;
        sin_theta1 = sin_theta2;
    }
}

void RivePath::parse_svg(String d) {
    int i = 0;
    int len = d.length();
    
    float cx = 0, cy = 0;
    float start_x = 0, start_y = 0;
    float last_cx = 0, last_cy = 0; // For smooth curves (S, T)
    char32_t last_cmd = 0;
    
    while (i < len) {
        // Skip whitespace
        while (i < len && d[i] <= ' ') i++;
        if (i >= len) break;
        
        char32_t cmd = d[i];
        if (is_number_start(cmd)) {
            // Implicit command (same as last, but if last was M/m, it becomes L/l)
            if (last_cmd == 'M') cmd = 'L';
            else if (last_cmd == 'm') cmd = 'l';
            else cmd = last_cmd;
        } else {
            i++;
        }
        
        bool relative = (cmd >= 'a' && cmd <= 'z');
        
        switch (cmd) {
            case 'M': case 'm': {
                float x = read_float(d, i);
                float y = read_float(d, i);
                if (relative) { x += cx; y += cy; }
                move_to(x, y);
                cx = x; cy = y;
                start_x = cx; start_y = cy;
                last_cx = cx; last_cy = cy;
                break;
            }
            case 'L': case 'l': {
                float x = read_float(d, i);
                float y = read_float(d, i);
                if (relative) { x += cx; y += cy; }
                line_to(x, y);
                cx = x; cy = y;
                last_cx = cx; last_cy = cy;
                break;
            }
            case 'H': case 'h': {
                float x = read_float(d, i);
                if (relative) x += cx;
                line_to(x, cy);
                cx = x;
                last_cx = cx; last_cy = cy;
                break;
            }
            case 'V': case 'v': {
                float y = read_float(d, i);
                if (relative) y += cy;
                line_to(cx, y);
                cy = y;
                last_cx = cx; last_cy = cy;
                break;
            }
            case 'C': case 'c': {
                float x1 = read_float(d, i);
                float y1 = read_float(d, i);
                float x2 = read_float(d, i);
                float y2 = read_float(d, i);
                float x = read_float(d, i);
                float y = read_float(d, i);
                
                if (relative) {
                    x1 += cx; y1 += cy;
                    x2 += cx; y2 += cy;
                    x += cx; y += cy;
                }
                
                cubic_to(x1, y1, x2, y2, x, y);
                last_cx = x2; last_cy = y2;
                cx = x; cy = y;
                break;
            }
            case 'S': case 's': {
                float x2 = read_float(d, i);
                float y2 = read_float(d, i);
                float x = read_float(d, i);
                float y = read_float(d, i);
                
                if (relative) {
                    x2 += cx; y2 += cy;
                    x += cx; y += cy;
                }
                
                float x1 = cx, y1 = cy;
                if (last_cmd == 'C' || last_cmd == 'c' || last_cmd == 'S' || last_cmd == 's') {
                    x1 = 2 * cx - last_cx;
                    y1 = 2 * cy - last_cy;
                }
                
                cubic_to(x1, y1, x2, y2, x, y);
                last_cx = x2; last_cy = y2;
                cx = x; cy = y;
                break;
            }
            case 'Q': case 'q': {
                // Quadratic bezier - convert to cubic
                float x1 = read_float(d, i);
                float y1 = read_float(d, i);
                float x = read_float(d, i);
                float y = read_float(d, i);
                
                if (relative) {
                    x1 += cx; y1 += cy;
                    x += cx; y += cy;
                }
                
                // CP1 = QP0 + 2/3 * (QP1 - QP0)
                // CP2 = QP2 + 2/3 * (QP1 - QP2)
                float cx1 = cx + (2.0f/3.0f) * (x1 - cx);
                float cy1 = cy + (2.0f/3.0f) * (y1 - cy);
                float cx2 = x + (2.0f/3.0f) * (x1 - x);
                float cy2 = y + (2.0f/3.0f) * (y1 - y);
                
                cubic_to(cx1, cy1, cx2, cy2, x, y);
                
                last_cx = x1; last_cy = y1;
                cx = x; cy = y;
                break;
            }
            case 'T': case 't': {
                float x = read_float(d, i);
                float y = read_float(d, i);
                
                if (relative) {
                    x += cx; y += cy;
                }
                
                float x1 = cx, y1 = cy;
                if (last_cmd == 'Q' || last_cmd == 'q' || last_cmd == 'T' || last_cmd == 't') {
                    x1 = 2 * cx - last_cx;
                    y1 = 2 * cy - last_cy;
                }
                
                // Convert quadratic T to cubic
                float cx1 = cx + (2.0f/3.0f) * (x1 - cx);
                float cy1 = cy + (2.0f/3.0f) * (y1 - cy);
                float cx2 = x + (2.0f/3.0f) * (x1 - x);
                float cy2 = y + (2.0f/3.0f) * (y1 - y);
                
                cubic_to(cx1, cy1, cx2, cy2, x, y);
                
                last_cx = x1; last_cy = y1;
                cx = x; cy = y;
                break;
            }
            case 'Z': case 'z': {
                close();
                cx = start_x; cy = start_y;
                last_cx = cx; last_cy = cy;
                break;
            }
            case 'A': case 'a': {
                float rx = read_float(d, i);
                float ry = read_float(d, i);
                float angle = read_float(d, i);
                
                // Flags are single digits 0 or 1
                // Skip whitespace
                while (i < len && (d[i] <= ' ' || d[i] == ',')) i++;
                float large_arc_flag = (i < len && d[i] == '1') ? 1.0f : 0.0f;
                if (i < len) i++;
                
                while (i < len && (d[i] <= ' ' || d[i] == ',')) i++;
                float sweep_flag = (i < len && d[i] == '1') ? 1.0f : 0.0f;
                if (i < len) i++;
                
                float x = read_float(d, i);
                float y = read_float(d, i);
                
                if (relative) {
                    x += cx; y += cy;
                }
                
                svg_arc_to(this, rx, ry, angle, large_arc_flag > 0.5f, sweep_flag > 0.5f, x, y, cx, cy);
                
                cx = x; cy = y;
                last_cx = cx; last_cy = cy;
                break;
            }
            default:
                // Unknown command
                break;
        }
        last_cmd = cmd;
    }
}

rive::RenderPath* RivePath::get_render_path(rive::Factory* factory) {
    if (is_dirty || !render_path) {
        if (factory) {
            // makeRenderPath consumes the RawPath, so we must copy it.
            rive::RawPath copy = *raw_path;
            render_path = factory->makeRenderPath(copy, static_cast<rive::FillRule>(fill_rule));
            is_dirty = false;
        }
    }
    return render_path.get();
}

// --- RivePaint ---

RivePaint::RivePaint() {}

RivePaint::~RivePaint() {}

void RivePaint::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_color", "color"), &RivePaint::set_color);
    ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &RivePaint::set_thickness);
    ClassDB::bind_method(D_METHOD("set_style", "style"), &RivePaint::set_style);
    ClassDB::bind_method(D_METHOD("set_join", "join"), &RivePaint::set_join);
    ClassDB::bind_method(D_METHOD("set_cap", "cap"), &RivePaint::set_cap);
    ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &RivePaint::set_blend_mode);
}

// Store properties to apply when creating/updating the paint
struct PaintProps {
    Color color = Color(0, 0, 0, 1);
    float thickness = 1.0f;
    int style = 1; // 0: Stroke, 1: Fill (Default to Fill)
    int join = 0; // 0: Miter, 1: Round, 2: Bevel
    int cap = 0; // 0: Butt, 1: Round, 2: Square
    int blend_mode = 3; // 3: SrcOver (default)
};

static PaintProps* get_paint_props(Object* obj) {
    static std::map<Object*, PaintProps> props_map;
    return &props_map[obj];
}

// Helper to convert Godot Color to Rive ColorInt
static rive::ColorInt color_to_int(Color c) {
    unsigned int r = (unsigned int)(c.r * 255.0f);
    unsigned int g = (unsigned int)(c.g * 255.0f);
    unsigned int b = (unsigned int)(c.b * 255.0f);
    unsigned int a = (unsigned int)(c.a * 255.0f);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void RivePaint::set_color(Color color) {
    PaintProps* props = get_paint_props(this);
    props->color = color;
    if (render_paint) {
        render_paint->color(color_to_int(color));
    }
}

void RivePaint::set_thickness(float thickness) {
    PaintProps* props = get_paint_props(this);
    props->thickness = thickness;
    if (render_paint) {
        render_paint->thickness(thickness);
    }
}

void RivePaint::set_style(int style) {
    PaintProps* props = get_paint_props(this);
    props->style = style;
    if (render_paint) {
        render_paint->style(static_cast<rive::RenderPaintStyle>(style));
    }
}

void RivePaint::set_join(int join) {
    PaintProps* props = get_paint_props(this);
    props->join = join;
    if (render_paint) {
        render_paint->join(static_cast<rive::StrokeJoin>(join));
    }
}

void RivePaint::set_cap(int cap) {
    PaintProps* props = get_paint_props(this);
    props->cap = cap;
    if (render_paint) {
        render_paint->cap(static_cast<rive::StrokeCap>(cap));
    }
}

void RivePaint::set_blend_mode(int blend_mode) {
    PaintProps* props = get_paint_props(this);
    props->blend_mode = blend_mode;
    if (render_paint) {
        render_paint->blendMode(static_cast<rive::BlendMode>(blend_mode));
    }
}

rive::RenderPaint* RivePaint::get_render_paint(rive::Factory* factory) {
    if (!render_paint && factory) {
        render_paint = factory->makeRenderPaint();
        PaintProps* props = get_paint_props(this);
        render_paint->color(color_to_int(props->color));
        render_paint->thickness(props->thickness);
        render_paint->style(static_cast<rive::RenderPaintStyle>(props->style));
        render_paint->join(static_cast<rive::StrokeJoin>(props->join));
        render_paint->cap(static_cast<rive::StrokeCap>(props->cap));
        render_paint->blendMode(static_cast<rive::BlendMode>(props->blend_mode));
    }
    return render_paint.get();
}

// --- RiveRendererWrapper ---

void RiveRendererWrapper::_bind_methods() {
    ClassDB::bind_method(D_METHOD("save"), &RiveRendererWrapper::save);
    ClassDB::bind_method(D_METHOD("restore"), &RiveRendererWrapper::restore);
    ClassDB::bind_method(D_METHOD("transform", "xx", "xy", "yx", "yy", "tx", "ty"), &RiveRendererWrapper::transform);
    ClassDB::bind_method(D_METHOD("translate", "x", "y"), &RiveRendererWrapper::translate);
    ClassDB::bind_method(D_METHOD("scale", "x", "y"), &RiveRendererWrapper::scale);
    ClassDB::bind_method(D_METHOD("rotate", "angle"), &RiveRendererWrapper::rotate);
    ClassDB::bind_method(D_METHOD("draw_path", "path", "paint"), &RiveRendererWrapper::draw_path);
}

void RiveRendererWrapper::save() {
    if (renderer) renderer->save();
}

void RiveRendererWrapper::restore() {
    if (renderer) renderer->restore();
}

void RiveRendererWrapper::transform(float xx, float xy, float yx, float yy, float tx, float ty) {
    if (renderer) renderer->transform(rive::Mat2D(xx, xy, yx, yy, tx, ty));
}

void RiveRendererWrapper::translate(float x, float y) {
    if (renderer) renderer->translate(x, y);
}

void RiveRendererWrapper::scale(float x, float y) {
    if (renderer) renderer->scale(x, y);
}

void RiveRendererWrapper::rotate(float angle) {
    if (renderer) renderer->rotate(angle);
}

void RiveRendererWrapper::draw_path(Ref<RivePath> path, Ref<RivePaint> paint) {
    if (renderer && path.is_valid() && paint.is_valid()) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) {
            renderer->drawPath(path->get_render_path(factory), paint->get_render_paint(factory));
        }
    }
}

// --- RiveCanvas ---

RiveCanvas::RiveCanvas() {
    renderer_wrapper.instantiate();
}

RiveCanvas::~RiveCanvas() {
    if (texture_rid.is_valid()) {
        RenderingServer *rs = RenderingServer::get_singleton();
        if (rs) {
            RenderingDevice *rd = rs->get_rendering_device();
            if (rd) {
                rd->free_rid(texture_rid);
            } else {
                rs->free_rid(texture_rid);
            }
        }
    }
    texture_rd_ref.unref();
    RiveRenderRegistry::get_singleton()->remove_drawable(this);
}

void RiveCanvas::_bind_methods() {
    ADD_SIGNAL(MethodInfo("draw_rive", PropertyInfo(Variant::OBJECT, "renderer", PROPERTY_HINT_RESOURCE_TYPE, "RiveRendererWrapper")));
}

void RiveCanvas::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
            RiveRenderRegistry::get_singleton()->add_drawable(this);
            set_process(true);
            break;
        case NOTIFICATION_EXIT_TREE:
            RiveRenderRegistry::get_singleton()->remove_drawable(this);
            break;
        case NOTIFICATION_DRAW:
            if (texture_rd_ref.is_valid()) {
                draw_texture(texture_rd_ref, Point2());
            } else if (texture_rid.is_valid()) {
                RenderingServer::get_singleton()->canvas_item_add_texture_rect(get_canvas_item(), Rect2(Point2(), get_size()), texture_rid);
            }
            break;
        case NOTIFICATION_PROCESS:
            _render_rive();
            queue_redraw();
            break;
    }
}

void RiveCanvas::_render_rive() {
    Size2i size = get_size();
    if (size.width <= 0 || size.height <= 0) return;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (!rs) return;
    RenderingDevice *rd = rs->get_rendering_device();
    
    String driver = rs->get_current_rendering_driver_name();
    bool is_opengl = (driver == "opengl3");

    if (!rd && !is_opengl) return;

    if (texture_size != size) {
        if (texture_rid.is_valid()) {
            if (rd) {
                rd->free_rid(texture_rid);
            } else {
                rs->free_rid(texture_rid);
            }
            texture_rid = RID();
        }

        if (rd) {
            Ref<RDTextureFormat> tf;
            tf.instantiate();
            tf->set_format(RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM);
            tf->set_width(size.width);
            tf->set_height(size.height);
            tf->set_usage_bits(RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT);

            Ref<RDTextureView> tv;
            tv.instantiate();

            texture_rid = rd->texture_create(tf, tv);
            
            if (texture_rd_ref.is_null()) {
                texture_rd_ref.instantiate();
            }
            texture_rd_ref->set_texture_rd_rid(texture_rid);
        } else {
            Ref<Image> img = Image::create(size.width, size.height, false, Image::FORMAT_RGBA8);
            texture_rid = rs->texture_2d_create(img);
            texture_rd_ref.unref();
        }
        texture_size = size;
    }

    rive_integration::render_texture(rd, texture_rid, this, size.width, size.height);
}

void RiveCanvas::draw(rive::Renderer* renderer) {
    if (renderer_wrapper.is_valid()) {
        renderer_wrapper->set_renderer(renderer);
        emit_signal("draw_rive", renderer_wrapper);
        renderer_wrapper->set_renderer(nullptr);
    }
}
