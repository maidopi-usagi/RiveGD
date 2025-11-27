#include "rive_types.h"
#include "rive/math/raw_path.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
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
    ClassDB::bind_method(D_METHOD("parse_svg", "path_data"), &RivePath::parse_svg);
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
    
    float x1 = cos_phi * dx + sin_phi * dy;
    float y1 = -sin_phi * dx + cos_phi * dy;
    
    float rx_sq = rx * rx;
    float ry_sq = ry * ry;
    float x1_sq = x1 * x1;
    float y1_sq = y1 * y1;
    
    float check = x1_sq / rx_sq + y1_sq / ry_sq;
    if (check > 1) {
        rx *= std::sqrt(check);
        ry *= std::sqrt(check);
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }
    
    float sign = (large_arc_flag == sweep_flag) ? -1 : 1;
    float sq = ((rx_sq * ry_sq) - (rx_sq * y1_sq) - (ry_sq * x1_sq)) / ((rx_sq * y1_sq) + (ry_sq * x1_sq));
    sq = (sq < 0) ? 0 : sq;
    float coef = sign * std::sqrt(sq);
    
    float cx1 = coef * ((rx * y1) / ry);
    float cy1 = coef * -((ry * x1) / rx);
    
    float cx = cur_x + (x - cur_x) / 2.0f + (cos_phi * cx1 - sin_phi * cy1);
    float cy = cur_y + (y - cur_y) / 2.0f + (sin_phi * cx1 + cos_phi * cy1);
    
    // TODO: Calculate start/end angles and draw arc using cubicTo approximation
    // For now, just line to end point as fallback
    path->line_to(x, y);
}

void RivePath::parse_svg(String path_data) {
    int i = 0;
    int len = path_data.length();
    float cur_x = 0, cur_y = 0;
    float start_x = 0, start_y = 0;
    float last_ctrl_x = 0, last_ctrl_y = 0;
    char32_t last_cmd = 0;
    
    while (i < len) {
        char32_t c = path_data[i];
        
        // Skip whitespace
        if (c == ' ' || c == ',' || c == '\t' || c == '\n') {
            i++;
            continue;
        }
        
        if (!local_is_digit(c) && c != '-' && c != '+' && c != '.') {
            last_cmd = c;
            i++;
        } else {
            // Implicit command (same as last)
            if (last_cmd == 'm') last_cmd = 'l';
            if (last_cmd == 'M') last_cmd = 'L';
        }
        
        switch (last_cmd) {
            case 'M': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                move_to(x, y);
                cur_x = x; cur_y = y;
                start_x = x; start_y = y;
                last_ctrl_x = x; last_ctrl_y = y;
                break;
            }
            case 'm': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                move_to(cur_x + x, cur_y + y);
                cur_x += x; cur_y += y;
                start_x = cur_x; start_y = cur_y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'L': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                line_to(x, y);
                cur_x = x; cur_y = y;
                last_ctrl_x = x; last_ctrl_y = y;
                break;
            }
            case 'l': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                line_to(cur_x + x, cur_y + y);
                cur_x += x; cur_y += y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'H': {
                float x = read_float(path_data, i);
                line_to(x, cur_y);
                cur_x = x;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'h': {
                float x = read_float(path_data, i);
                line_to(cur_x + x, cur_y);
                cur_x += x;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'V': {
                float y = read_float(path_data, i);
                line_to(cur_x, y);
                cur_y = y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'v': {
                float y = read_float(path_data, i);
                line_to(cur_x, cur_y + y);
                cur_y += y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'C': {
                float x1 = read_float(path_data, i);
                float y1 = read_float(path_data, i);
                float x2 = read_float(path_data, i);
                float y2 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                cubic_to(x1, y1, x2, y2, x, y);
                last_ctrl_x = x2; last_ctrl_y = y2;
                cur_x = x; cur_y = y;
                break;
            }
            case 'c': {
                float x1 = read_float(path_data, i);
                float y1 = read_float(path_data, i);
                float x2 = read_float(path_data, i);
                float y2 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                cubic_to(cur_x + x1, cur_y + y1, cur_x + x2, cur_y + y2, cur_x + x, cur_y + y);
                last_ctrl_x = cur_x + x2; last_ctrl_y = cur_y + y2;
                cur_x += x; cur_y += y;
                break;
            }
            case 'S': {
                float x2 = read_float(path_data, i);
                float y2 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                float x1 = 2 * cur_x - last_ctrl_x;
                float y1 = 2 * cur_y - last_ctrl_y;
                cubic_to(x1, y1, x2, y2, x, y);
                last_ctrl_x = x2; last_ctrl_y = y2;
                cur_x = x; cur_y = y;
                break;
            }
            case 's': {
                float x2 = read_float(path_data, i);
                float y2 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                float x1 = 2 * cur_x - last_ctrl_x;
                float y1 = 2 * cur_y - last_ctrl_y;
                cubic_to(x1, y1, cur_x + x2, cur_y + y2, cur_x + x, cur_y + y);
                last_ctrl_x = cur_x + x2; last_ctrl_y = cur_y + y2;
                cur_x += x; cur_y += y;
                break;
            }
            case 'Z':
            case 'z': {
                close();
                cur_x = start_x; cur_y = start_y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            default:
                // Unknown command, skip
                i++;
                break;
        }
    }
}

rive::RenderPath* RivePath::get_render_path(rive::Factory* factory) {
    if (is_dirty || !render_path) {
        if (factory) {
            render_path = factory->makeRenderPath(*raw_path, (rive::FillRule)fill_rule);
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

void RivePaint::_apply_properties() {
    if (!render_paint) return;
    
    unsigned int r = (unsigned int)(color.r * 255);
    unsigned int g = (unsigned int)(color.g * 255);
    unsigned int b = (unsigned int)(color.b * 255);
    unsigned int a = (unsigned int)(color.a * 255);
    
    render_paint->color((a << 24) | (r << 16) | (g << 8) | b);
    render_paint->thickness(thickness);
    render_paint->style((rive::RenderPaintStyle)style);
    render_paint->join((rive::StrokeJoin)join);
    render_paint->cap((rive::StrokeCap)cap);
    render_paint->blendMode((rive::BlendMode)blend_mode);
}

void RivePaint::set_color(Color p_color) {
    color = p_color;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) {
        unsigned int r = (unsigned int)(color.r * 255);
        unsigned int g = (unsigned int)(color.g * 255);
        unsigned int b = (unsigned int)(color.b * 255);
        unsigned int a = (unsigned int)(color.a * 255);
        render_paint->color((a << 24) | (r << 16) | (g << 8) | b);
    }
}

void RivePaint::set_thickness(float p_thickness) {
    thickness = p_thickness;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->thickness(thickness);
}

void RivePaint::set_style(int p_style) {
    style = p_style;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->style((rive::RenderPaintStyle)style);
}

void RivePaint::set_join(int p_join) {
    join = p_join;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->join((rive::StrokeJoin)join);
}

void RivePaint::set_cap(int p_cap) {
    cap = p_cap;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->cap((rive::StrokeCap)cap);
}

void RivePaint::set_blend_mode(int p_blend_mode) {
    blend_mode = p_blend_mode;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->blendMode((rive::BlendMode)blend_mode);
}

rive::RenderPaint* RivePaint::get_render_paint(rive::Factory* factory) {
    if (!render_paint && factory) {
        render_paint = factory->makeRenderPaint();
        _apply_properties();
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
