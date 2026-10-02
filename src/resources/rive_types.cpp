#include "rive_types.h"
#include "../renderer/rive_renderer.h"
#include "rive/math/raw_path.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text_engine.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <cmath>
#include <vector>
#include <string>

using namespace godot;

// --- RivePath ---

RivePath::RivePath() {
    raw_path = std::make_unique<rive::RawPath>();
}

RivePath::~RivePath() {}

void RivePath::_bind_methods() {
    ClassDB::bind_method(D_METHOD("move_to", "x", "y"), &RivePath::move_to);
    ClassDB::bind_method(D_METHOD("line_to", "x", "y"), &RivePath::line_to);
    ClassDB::bind_method(D_METHOD("quad_to", "cx", "cy", "x", "y"), &RivePath::quad_to);
    ClassDB::bind_method(D_METHOD("cubic_to", "ox", "oy", "ix", "iy", "x", "y"), &RivePath::cubic_to);
    ClassDB::bind_method(D_METHOD("close"), &RivePath::close);
    ClassDB::bind_method(D_METHOD("reset"), &RivePath::reset);
    ClassDB::bind_method(D_METHOD("add_rect", "x", "y", "w", "h"), &RivePath::add_rect);
    ClassDB::bind_method(D_METHOD("add_oval", "x", "y", "w", "h"), &RivePath::add_oval);
    ClassDB::bind_method(D_METHOD("add_circle", "cx", "cy", "radius"), &RivePath::add_circle);
    ClassDB::bind_method(D_METHOD("add_path", "other"), &RivePath::add_path);
    ClassDB::bind_method(D_METHOD("add_path_transformed", "other", "xform"), &RivePath::add_path_transformed);
    ClassDB::bind_method(D_METHOD("add_poly", "points", "closed"), &RivePath::add_poly);
    ClassDB::bind_method(D_METHOD("get_bounds"), &RivePath::get_bounds);
    ClassDB::bind_method(D_METHOD("is_empty"), &RivePath::is_empty);
    ClassDB::bind_method(D_METHOD("morph", "proc"), &RivePath::morph);
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

void RivePath::quad_to(float cx, float cy, float x, float y) {
    raw_path->quadTo(cx, cy, x, y);
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

void RivePath::add_rect(float x, float y, float w, float h) {
    raw_path->addRect(rive::AABB(x, y, x + w, y + h));
    is_dirty = true;
}

void RivePath::add_oval(float x, float y, float w, float h) {
    raw_path->addOval(rive::AABB(x, y, x + w, y + h));
    is_dirty = true;
}

void RivePath::add_circle(float cx, float cy, float radius) {
    raw_path->addOval(rive::AABB(cx - radius, cy - radius, cx + radius, cy + radius));
    is_dirty = true;
}

void RivePath::add_path(Ref<RivePath> other) {
    if (other.is_null() || other.ptr() == this) return;
    const rive::RawPath* src = other->raw_path.get();
    if (!src) return;
    for (auto iter = src->begin(); iter != src->end(); ++iter) {
        rive::PathVerb verb = iter.verb();
        const rive::Vec2D* p = iter.pts();
        switch (verb) {
            case rive::PathVerb::move:
                raw_path->moveTo(p[0].x, p[0].y);
                break;
            case rive::PathVerb::line:
                raw_path->lineTo(p[1].x, p[1].y);
                break;
            case rive::PathVerb::quad:
                raw_path->quadTo(p[1].x, p[1].y, p[2].x, p[2].y);
                break;
            case rive::PathVerb::cubic:
                raw_path->cubicTo(p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
                break;
            case rive::PathVerb::close:
                raw_path->close();
                break;
        }
    }
    is_dirty = true;
}

void RivePath::add_path_transformed(Ref<RivePath> other, Transform2D xform) {
    if (other.is_null() || other.ptr() == this) return;
    const rive::RawPath* src = other->raw_path.get();
    if (!src) return;
    auto tx = [&](float x, float y) -> rive::Vec2D {
        Vector2 v = xform.xform(Vector2(x, y));
        return {v.x, v.y};
    };
    for (auto iter = src->begin(); iter != src->end(); ++iter) {
        rive::PathVerb verb = iter.verb();
        const rive::Vec2D* p = iter.pts();
        switch (verb) {
            case rive::PathVerb::move: {
                auto v = tx(p[0].x, p[0].y);
                raw_path->moveTo(v.x, v.y);
                break;
            }
            case rive::PathVerb::line: {
                auto v = tx(p[1].x, p[1].y);
                raw_path->lineTo(v.x, v.y);
                break;
            }
            case rive::PathVerb::quad: {
                auto a = tx(p[1].x, p[1].y);
                auto b = tx(p[2].x, p[2].y);
                raw_path->quadTo(a.x, a.y, b.x, b.y);
                break;
            }
            case rive::PathVerb::cubic: {
                auto a = tx(p[1].x, p[1].y);
                auto b = tx(p[2].x, p[2].y);
                auto c = tx(p[3].x, p[3].y);
                raw_path->cubicTo(a.x, a.y, b.x, b.y, c.x, c.y);
                break;
            }
            case rive::PathVerb::close:
                raw_path->close();
                break;
        }
    }
    is_dirty = true;
}

void RivePath::add_poly(PackedVector2Array points, bool closed) {
    int n = points.size();
    if (n < 2) return;
    raw_path->moveTo(points[0].x, points[0].y);
    for (int k = 1; k < n; k++) {
        raw_path->lineTo(points[k].x, points[k].y);
    }
    if (closed) raw_path->close();
    is_dirty = true;
}

Rect2 RivePath::get_bounds() const {
    if (!raw_path || raw_path->empty()) return Rect2();
    rive::AABB b = raw_path->bounds();
    return Rect2(b.minX, b.minY, b.maxX - b.minX, b.maxY - b.minY);
}

bool RivePath::is_empty() const {
    return !raw_path || raw_path->empty();
}

Ref<RivePath> RivePath::morph(Callable proc) const {
    Ref<RivePath> out;
    out.instantiate();
    if (!raw_path || !proc.is_valid()) return out;
    auto warp = [&](const rive::Vec2D& p) -> rive::Vec2D {
        Variant v = proc.call(Vector2(p.x, p.y));
        if (v.get_type() == Variant::VECTOR2) {
            Vector2 r = v;
            return {r.x, r.y};
        }
        return p;
    };
    for (auto iter = raw_path->begin(); iter != raw_path->end(); ++iter) {
        rive::PathVerb verb = iter.verb();
        const rive::Vec2D* p = iter.pts();
        switch (verb) {
            case rive::PathVerb::move: {
                auto a = warp(p[0]);
                out->raw_path->moveTo(a.x, a.y);
                break;
            }
            case rive::PathVerb::line: {
                auto a = warp(p[1]);
                out->raw_path->lineTo(a.x, a.y);
                break;
            }
            case rive::PathVerb::quad: {
                auto a = warp(p[1]);
                auto b = warp(p[2]);
                out->raw_path->quadTo(a.x, a.y, b.x, b.y);
                break;
            }
            case rive::PathVerb::cubic: {
                auto a = warp(p[1]);
                auto b = warp(p[2]);
                auto c = warp(p[3]);
                out->raw_path->cubicTo(a.x, a.y, b.x, b.y, c.x, c.y);
                break;
            }
            case rive::PathVerb::close:
                out->raw_path->close();
                break;
        }
    }
    out->is_dirty = true;
    return out;
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
    // Endpoint-to-center conversion per SVG 1.1 Appendix F.6.5,
    // then split the resulting elliptical arc into <= 90deg segments,
    // each approximated by a cubic Bezier.
    if (cur_x == x && cur_y == y) return;
    if (rx == 0 || ry == 0) {
        path->line_to(x, y);
        return;
    }

    rx = std::abs(rx);
    ry = std::abs(ry);

    float angle_rad = Math::deg_to_rad(angle);
    float cos_phi = std::cos(angle_rad);
    float sin_phi = std::sin(angle_rad);

    float dx = (cur_x - x) * 0.5f;
    float dy = (cur_y - y) * 0.5f;

    // (x1', y1') — coordinates in the rotated frame.
    float x1p = cos_phi * dx + sin_phi * dy;
    float y1p = -sin_phi * dx + cos_phi * dy;

    float rx_sq = rx * rx;
    float ry_sq = ry * ry;
    float x1p_sq = x1p * x1p;
    float y1p_sq = y1p * y1p;

    // Scale up radii if they are too small to fit the chord.
    float lambda = x1p_sq / rx_sq + y1p_sq / ry_sq;
    if (lambda > 1.0f) {
        float s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    float sign = (large_arc_flag == sweep_flag) ? -1.0f : 1.0f;
    float numer = rx_sq * ry_sq - rx_sq * y1p_sq - ry_sq * x1p_sq;
    float denom = rx_sq * y1p_sq + ry_sq * x1p_sq;
    float coef = sign * std::sqrt(std::max(0.0f, numer / denom));

    float cxp = coef * (rx * y1p) / ry;
    float cyp = coef * -(ry * x1p) / rx;

    // Center in user space.
    float cx = cos_phi * cxp - sin_phi * cyp + (cur_x + x) * 0.5f;
    float cy = sin_phi * cxp + cos_phi * cyp + (cur_y + y) * 0.5f;

    // Start angle and sweep.
    auto angle_between = [](float ux, float uy, float vx, float vy) {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        if (len == 0.0f) return 0.0f;
        float c = dot / len;
        if (c < -1.0f) c = -1.0f;
        if (c > 1.0f) c = 1.0f;
        float a = std::acos(c);
        if ((ux * vy - uy * vx) < 0.0f) a = -a;
        return a;
    };

    float ux = (x1p - cxp) / rx;
    float uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx;
    float vy = (-y1p - cyp) / ry;

    float theta1 = angle_between(1.0f, 0.0f, ux, uy);
    float delta = angle_between(ux, uy, vx, vy);
    if (!sweep_flag && delta > 0.0f) delta -= (float)Math_TAU;
    else if (sweep_flag && delta < 0.0f) delta += (float)Math_TAU;

    // Number of cubic segments (each spans <= 90deg).
    int segments = (int)std::ceil(std::abs(delta) / ((float)Math_PI * 0.5f));
    if (segments < 1) segments = 1;
    float seg_delta = delta / (float)segments;
    // Bezier control point handle length for a unit-circle arc segment.
    float t = (4.0f / 3.0f) * std::tan(seg_delta * 0.25f);

    float a = theta1;
    float prev_cos = std::cos(a);
    float prev_sin = std::sin(a);
    for (int s = 0; s < segments; s++) {
        float a1 = a + seg_delta;
        float c1 = std::cos(a1);
        float s1 = std::sin(a1);

        // Points on the unit circle (x', y') frame.
        float p1x = prev_cos - t * prev_sin;
        float p1y = prev_sin + t * prev_cos;
        float p2x = c1 + t * s1;
        float p2y = s1 - t * c1;

        // Scale by (rx, ry), rotate by phi, translate by center.
        auto map = [&](float px, float py, float& ox, float& oy) {
            float xs = px * rx;
            float ys = py * ry;
            ox = cos_phi * xs - sin_phi * ys + cx;
            oy = sin_phi * xs + cos_phi * ys + cy;
        };

        float c1x, c1y, c2x, c2y, ex, ey;
        map(p1x, p1y, c1x, c1y);
        map(p2x, p2y, c2x, c2y);
        map(c1, s1, ex, ey);

        // Last segment snaps exactly to (x, y) to avoid rounding drift.
        if (s == segments - 1) {
            ex = x;
            ey = y;
        }

        path->cubic_to(c1x, c1y, c2x, c2y, ex, ey);

        a = a1;
        prev_cos = c1;
        prev_sin = s1;
    }
}

void RivePath::parse_svg(String path_data) {
    int i = 0;
    int len = path_data.length();
    float cur_x = 0, cur_y = 0;
    float start_x = 0, start_y = 0;
    float last_ctrl_x = 0, last_ctrl_y = 0;
    float last_quad_x = 0, last_quad_y = 0;
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
            case 'Q': {
                float x1 = read_float(path_data, i);
                float y1 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                quad_to(x1, y1, x, y);
                last_quad_x = x1; last_quad_y = y1;
                last_ctrl_x = x1; last_ctrl_y = y1;
                cur_x = x; cur_y = y;
                break;
            }
            case 'q': {
                float x1 = read_float(path_data, i);
                float y1 = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                float ax = cur_x + x1, ay = cur_y + y1;
                quad_to(ax, ay, cur_x + x, cur_y + y);
                last_quad_x = ax; last_quad_y = ay;
                last_ctrl_x = ax; last_ctrl_y = ay;
                cur_x += x; cur_y += y;
                break;
            }
            case 'T': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                float ax = 2 * cur_x - last_quad_x;
                float ay = 2 * cur_y - last_quad_y;
                quad_to(ax, ay, x, y);
                last_quad_x = ax; last_quad_y = ay;
                last_ctrl_x = ax; last_ctrl_y = ay;
                cur_x = x; cur_y = y;
                break;
            }
            case 't': {
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                float ax = 2 * cur_x - last_quad_x;
                float ay = 2 * cur_y - last_quad_y;
                quad_to(ax, ay, cur_x + x, cur_y + y);
                last_quad_x = ax; last_quad_y = ay;
                last_ctrl_x = ax; last_ctrl_y = ay;
                cur_x += x; cur_y += y;
                break;
            }
            case 'A': {
                float rx = read_float(path_data, i);
                float ry = read_float(path_data, i);
                float ang = read_float(path_data, i);
                float la = read_float(path_data, i);
                float sw = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                svg_arc_to(this, rx, ry, ang, la != 0.0f, sw != 0.0f, x, y, cur_x, cur_y);
                cur_x = x; cur_y = y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
                break;
            }
            case 'a': {
                float rx = read_float(path_data, i);
                float ry = read_float(path_data, i);
                float ang = read_float(path_data, i);
                float la = read_float(path_data, i);
                float sw = read_float(path_data, i);
                float x = read_float(path_data, i);
                float y = read_float(path_data, i);
                svg_arc_to(this, rx, ry, ang, la != 0.0f, sw != 0.0f, cur_x + x, cur_y + y, cur_x, cur_y);
                cur_x += x; cur_y += y;
                last_ctrl_x = cur_x; last_ctrl_y = cur_y;
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

        // For T/t smooth-quad reflection: if the last executed command was not
        // a quadratic, the implied previous control point is the current point.
        if (last_cmd != 'Q' && last_cmd != 'q' && last_cmd != 'T' && last_cmd != 't') {
            last_quad_x = cur_x;
            last_quad_y = cur_y;
        }
    }
}

rive::RenderPath* RivePath::get_render_path(rive::Factory* factory) {
    if (is_dirty || !render_path) {
        if (factory) {
            // makeRenderPath swaps the contents of the supplied RawPath into the
            // resulting RenderPath, leaving our raw_path empty. Pass a copy so
            // we can keep mutating / inspecting raw_path (e.g. via get_bounds,
            // is_empty, add_path, parse_svg) after this call.
            rive::RawPath copy = *raw_path;
            render_path = factory->makeRenderPath(copy, (rive::FillRule)fill_rule);
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
    ClassDB::bind_method(D_METHOD("set_feather", "feather"), &RivePaint::set_feather);
    ClassDB::bind_method(D_METHOD("set_gradient", "gradient"), &RivePaint::set_gradient);
    ClassDB::bind_method(D_METHOD("get_gradient"), &RivePaint::get_gradient);
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
    render_paint->feather(feather);
    if (gradient.is_valid()) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        render_paint->shader(gradient->get_shader_rcp(factory));
    }
    // NOTE: Do NOT call render_paint->shader(null) when no gradient is set.
    // In Rive's PLS renderer, an explicitly-null shader rcp puts the paint
    // into "shader mode with no shader", causing solid-color fills to render
    // as black silhouettes. Leaving the shader unset preserves color().
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

void RivePaint::set_feather(float p_feather) {
    feather = p_feather;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) render_paint->feather(feather);
}

void RivePaint::set_gradient(Ref<RiveGradient> p_gradient) {
    gradient = p_gradient;
    if (!render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) render_paint = factory->makeRenderPaint();
    }
    if (render_paint) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (gradient.is_valid() && factory) {
            render_paint->shader(gradient->get_shader_rcp(factory));
        }
        // See _apply_properties: never call shader(null), it forces black.
    }
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
    ClassDB::bind_method(D_METHOD("clip_path", "path"), &RiveRendererWrapper::clip_path);
    ClassDB::bind_method(D_METHOD("draw_image", "image", "opacity", "blend_mode"), &RiveRendererWrapper::draw_image);
    ClassDB::bind_method(D_METHOD("draw_image_mesh", "image", "vertices", "uvs", "indices", "opacity", "blend_mode"), &RiveRendererWrapper::draw_image_mesh);
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

void RiveRendererWrapper::clip_path(Ref<RivePath> path) {
    if (renderer && path.is_valid()) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) {
            renderer->clipPath(path->get_render_path(factory));
        }
    }
}

void RiveRendererWrapper::draw_image(Ref<RiveImage> image, float opacity, int blend_mode) {
    if (renderer && image.is_valid() && image->is_loaded()) {
        renderer->drawImage(image->get_render_image(),
                            rive::ImageSampler::LinearClamp(),
                            (rive::BlendMode)blend_mode,
                            opacity);
    }
}

void RiveRendererWrapper::draw_image_mesh(Ref<RiveImage> image,
                                          PackedVector2Array vertices,
                                          PackedVector2Array uvs,
                                          PackedInt32Array indices,
                                          float opacity,
                                          int blend_mode) {
    if (!renderer || image.is_null() || !image->is_loaded()) return;
    int vcount = vertices.size();
    int icount = indices.size();
    if (vcount == 0 || icount == 0 || uvs.size() != vcount) {
        UtilityFunctions::push_warning("RiveRendererWrapper.draw_image_mesh: invalid vertex/uv/index counts.");
        return;
    }
    if (vcount > 65535) {
        UtilityFunctions::push_warning("RiveRendererWrapper.draw_image_mesh: vertex count exceeds uint16 limit.");
        return;
    }
    for (int k = 0; k < icount; k++) {
        int v = indices[k];
        if (v < 0 || v >= vcount || v > 65535) {
            UtilityFunctions::printerr("RiveImage.draw_mesh: index is outside the vertex array.");
            return;
        }
    }
    // Each Vector2 is two floats; sizeof(Vector2) == 8 bytes assumed.
    size_t vert_bytes = (size_t)vcount * sizeof(float) * 2;
    size_t idx_bytes = (size_t)icount * sizeof(uint16_t);

    std::vector<float> vertex_data((size_t)vcount * 2);
    std::vector<float> uv_data((size_t)vcount * 2);
    std::vector<uint16_t> index_data((size_t)icount);
    for (int k = 0; k < vcount; k++) {
        vertex_data[(size_t)k * 2] = vertices[k].x;
        vertex_data[(size_t)k * 2 + 1] = vertices[k].y;
        uv_data[(size_t)k * 2] = uvs[k].x;
        uv_data[(size_t)k * 2 + 1] = uvs[k].y;
    }
    for (int k = 0; k < icount; k++) {
        index_data[(size_t)k] = (uint16_t)indices[k];
    }

    // The draw callback is normally recorded on the scene thread. Defer GPU
    // buffer creation/mapping to replay on Godot's render thread.
    if (auto *recording = dynamic_cast<RiveCommandRecordingTarget *>(renderer)) {
        recording->record_image_mesh(image->get_render_image(),
                                     vertex_data.data(), uv_data.data(), index_data.data(),
                                     (uint32_t)vcount, (uint32_t)icount,
                                     blend_mode, opacity);
        return;
    }

    rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) return;

    rive::rcp<rive::RenderBuffer> vb = factory->makeRenderBuffer(
        rive::RenderBufferType::vertex,
        rive::RenderBufferFlags::mappedOnceAtInitialization,
        vert_bytes);
    rive::rcp<rive::RenderBuffer> ub = factory->makeRenderBuffer(
        rive::RenderBufferType::vertex,
        rive::RenderBufferFlags::mappedOnceAtInitialization,
        vert_bytes);
    rive::rcp<rive::RenderBuffer> ib = factory->makeRenderBuffer(
        rive::RenderBufferType::index,
        rive::RenderBufferFlags::mappedOnceAtInitialization,
        idx_bytes);
    if (!vb || !ub || !ib) return;

    if (void* vp = vb->map()) {
        float* dst = (float*)vp;
        for (int k = 0; k < vcount; k++) {
            dst[k * 2 + 0] = vertex_data[(size_t)k * 2];
            dst[k * 2 + 1] = vertex_data[(size_t)k * 2 + 1];
        }
        vb->unmap();
    }
    if (void* up = ub->map()) {
        float* dst = (float*)up;
        for (int k = 0; k < vcount; k++) {
            dst[k * 2 + 0] = uv_data[(size_t)k * 2];
            dst[k * 2 + 1] = uv_data[(size_t)k * 2 + 1];
        }
        ub->unmap();
    }
    if (void* ip = ib->map()) {
        uint16_t* dst = (uint16_t*)ip;
        for (int k = 0; k < icount; k++) {
            dst[k] = index_data[(size_t)k];
        }
        ib->unmap();
    }

    renderer->drawImageMesh(image->get_render_image(),
                            rive::ImageSampler::LinearClamp(),
                            vb, ub, ib,
                            (uint32_t)vcount,
                            (uint32_t)icount,
                            (rive::BlendMode)blend_mode,
                            opacity);
}

// --- RiveImage ---

void RiveImage::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_from_buffer", "bytes"), &RiveImage::load_from_buffer);
    ClassDB::bind_method(D_METHOD("load_from_texture", "texture"), &RiveImage::load_from_texture);
    ClassDB::bind_method(D_METHOD("get_width"), &RiveImage::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &RiveImage::get_height);
    ClassDB::bind_method(D_METHOD("is_loaded"), &RiveImage::is_loaded);
}

RiveImage::~RiveImage() {
    if (!render_image) return;
    if (!rive_integration::run_on_rive_render_thread([this]() {
            render_image.reset();
        })) {
        // Renderer teardown normally destroys scene resources before removing
        // the RenderingServer. Avoid leaking if destruction happens later.
        render_image.reset();
    }
}

bool RiveImage::load_from_buffer(PackedByteArray bytes) {
    if (bytes.size() == 0) return false;

    bool factory_ready = false;
    bool loaded = false;
    const bool executed = rive_integration::run_on_rive_render_thread([&]() {
        rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (!factory) return;
        factory_ready = true;

        rive::Span<const uint8_t> span(bytes.ptr(), (size_t)bytes.size());
        rive::rcp<rive::RenderImage> next_image = factory->decodeImage(span);
        // Both replacing the old image and dropping its final reference must
        // happen on the render thread.
        render_image = std::move(next_image);
        if (render_image) {
            width = render_image->width();
            height = render_image->height();
            loaded = true;
        } else {
            width = 0;
            height = 0;
        }
    });

    if (!executed || !factory_ready) {
        UtilityFunctions::push_warning("RiveImage: factory not ready, cannot decode image yet.");
        return false;
    }
    return loaded;
}

bool RiveImage::load_from_texture(Ref<Texture2D> texture) {
    if (texture.is_null()) return false;
    Ref<Image> img = texture->get_image();
    if (img.is_null() || img->is_empty()) return false;
    PackedByteArray png = img->save_png_to_buffer();
    if (png.size() == 0) return false;
    return load_from_buffer(png);
}

// --- RiveGradient ---

void RiveGradient::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_linear", "from", "to"), &RiveGradient::set_linear);
    ClassDB::bind_method(D_METHOD("set_radial", "center", "radius"), &RiveGradient::set_radial);
    ClassDB::bind_method(D_METHOD("set_stops", "colors", "stops"), &RiveGradient::set_stops);
}

void RiveGradient::set_linear(Vector2 from, Vector2 to) {
    is_radial = false;
    sx = from.x; sy = from.y;
    ex = to.x; ey = to.y;
    dirty = true;
}

void RiveGradient::set_radial(Vector2 center, float radius) {
    is_radial = true;
    sx = center.x; sy = center.y;
    ex = radius; ey = 0.0f;
    dirty = true;
}

void RiveGradient::set_stops(PackedColorArray p_colors, PackedFloat32Array p_stops) {
    colors = p_colors;
    stops = p_stops;
    dirty = true;
}

rive::rcp<rive::RenderShader> RiveGradient::get_shader_rcp(rive::Factory* factory) {
    if (!factory) return rive::rcp<rive::RenderShader>(nullptr);
    if (dirty || !shader) {
        size_t count = (size_t)Math::min(colors.size(), stops.size());
        if (count < 2) return rive::rcp<rive::RenderShader>(nullptr);

        std::vector<rive::ColorInt> color_ints(count);
        std::vector<float> stop_floats(count);
        for (size_t i = 0; i < count; i++) {
            Color c = colors[i];
            unsigned int r = (unsigned int)(c.r * 255);
            unsigned int g = (unsigned int)(c.g * 255);
            unsigned int b = (unsigned int)(c.b * 255);
            unsigned int a = (unsigned int)(c.a * 255);
            color_ints[i] = (a << 24) | (r << 16) | (g << 8) | b;
            stop_floats[i] = stops[i];
        }
        if (is_radial) {
            shader = factory->makeRadialGradient(sx, sy, ex, color_ints.data(), stop_floats.data(), count);
        } else {
            shader = factory->makeLinearGradient(sx, sy, ex, ey, color_ints.data(), stop_floats.data(), count);
        }
        dirty = false;
    }
    return shader;
}

rive::RenderShader* RiveGradient::get_shader(rive::Factory* factory) {
    return get_shader_rcp(factory).get();
}

// --- RiveFont ---

void RiveFont::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_from_buffer", "bytes"), &RiveFont::load_from_buffer);
    ClassDB::bind_method(D_METHOD("load_from_file", "path"), &RiveFont::load_from_file);
    ClassDB::bind_method(D_METHOD("is_loaded"), &RiveFont::is_loaded);
    ClassDB::bind_method(D_METHOD("get_weight"), &RiveFont::get_weight);
    ClassDB::bind_method(D_METHOD("is_italic"), &RiveFont::is_italic);
    ClassDB::bind_method(D_METHOD("shape_text", "text", "size"), &RiveFont::shape_text);
}

bool RiveFont::load_from_buffer(PackedByteArray bytes) {
    if (bytes.size() == 0) return false;
    rive::Span<const uint8_t> span(bytes.ptr(), bytes.size());
    font = HBFont::Decode(span);
    if (!font) {
        UtilityFunctions::push_warning("RiveFont.load_from_buffer: HBFont::Decode returned null.");
        return false;
    }
    return true;
}

bool RiveFont::load_from_file(String path) {
    Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
    if (f.is_null()) {
        UtilityFunctions::push_warning("RiveFont.load_from_file: cannot open " + path);
        return false;
    }
    PackedByteArray bytes = f->get_buffer(f->get_length());
    return load_from_buffer(bytes);
}

int RiveFont::get_weight() const {
    return font ? (int)font->getWeight() : 0;
}

bool RiveFont::is_italic() const {
    return font ? font->isItalic() : false;
}

Array RiveFont::shape_text(String text, float size) {
    Array out;
    if (!font || text.length() == 0 || size <= 0.0f) return out;

    // Convert Godot UTF-32 String to a vector of Unichar.
    std::vector<rive::Unichar> unichars;
    unichars.reserve(text.length());
    for (int i = 0; i < text.length(); i++) {
        unichars.push_back((rive::Unichar)text[i]);
    }

    // Single-run shaping with this font/size.
    rive::TextRun run;
    run.font = font;
    run.size = size;
    run.lineHeight = -1.0f;
    run.letterSpacing = 0.0f;
    run.unicharCount = (uint32_t)unichars.size();
    run.script = 0;
    run.styleId = 0;
    run.level = 0;

    rive::TextRun runs[1] = { run };
    rive::SimpleArray<rive::Paragraph> paragraphs = font->shapeText(
        rive::Span<const rive::Unichar>(unichars.data(), unichars.size()),
        rive::Span<const rive::TextRun>(runs, 1));

    for (size_t pi = 0; pi < paragraphs.size(); pi++) {
        const rive::Paragraph& para = paragraphs[pi];
        for (size_t ri = 0; ri < para.runs.size(); ri++) {
            const rive::GlyphRun& gr = para.runs[ri];
            for (size_t gi = 0; gi < gr.glyphs.size(); gi++) {
                rive::GlyphID gid = gr.glyphs[gi];
                rive::RawPath glyph_path = gr.font->getPath(gid);
                // Glyph path is at 1pt; scale to requested size and apply
                // baseline x offset (xpos[gi]) + per-glyph offset.
                float gx = gr.xpos[gi] + gr.offsets[gi].x;
                float gy = gr.offsets[gi].y;
                Ref<RivePath> rp;
                rp.instantiate();
                for (auto it = glyph_path.begin(); it != glyph_path.end(); ++it) {
                    rive::PathVerb verb = it.verb();
                    const rive::Vec2D* p = it.pts();
                    auto tx = [&](const rive::Vec2D& v) -> rive::Vec2D {
                        return { v.x * size + gx, v.y * size + gy };
                    };
                    switch (verb) {
                        case rive::PathVerb::move: {
                            auto a = tx(p[0]);
                            rp->move_to(a.x, a.y);
                            break;
                        }
                        case rive::PathVerb::line: {
                            auto a = tx(p[1]);
                            rp->line_to(a.x, a.y);
                            break;
                        }
                        case rive::PathVerb::quad: {
                            auto a = tx(p[1]);
                            auto b = tx(p[2]);
                            rp->quad_to(a.x, a.y, b.x, b.y);
                            break;
                        }
                        case rive::PathVerb::cubic: {
                            auto a = tx(p[1]);
                            auto b = tx(p[2]);
                            auto c = tx(p[3]);
                            rp->cubic_to(a.x, a.y, b.x, b.y, c.x, c.y);
                            break;
                        }
                        case rive::PathVerb::close:
                            rp->close();
                            break;
                    }
                }
                Dictionary entry;
                entry["path"] = rp;
                entry["x"] = gx;
                entry["y"] = gy;
                entry["advance"] = gi + 1 < gr.xpos.size() ? (gr.xpos[gi + 1] - gr.xpos[gi]) : 0.0f;
                entry["glyph_id"] = (int)gid;
                out.push_back(entry);
            }
        }
    }
    return out;
}

// --- RiveText ---

void RiveText::_bind_methods() {
    ClassDB::bind_method(D_METHOD("clear"), &RiveText::clear);
    ClassDB::bind_method(D_METHOD("append_run", "text", "font", "paint", "size", "line_height", "letter_spacing"), &RiveText::append_run);
    ClassDB::bind_method(D_METHOD("render", "renderer", "override_paint"), &RiveText::render);
    ClassDB::bind_method(D_METHOD("get_bounds"), &RiveText::get_bounds);
    ClassDB::bind_method(D_METHOD("shape_glyphs"), &RiveText::shape_glyphs);
    ClassDB::bind_method(D_METHOD("set_sizing", "v"), &RiveText::set_sizing);
    ClassDB::bind_method(D_METHOD("set_overflow", "v"), &RiveText::set_overflow);
    ClassDB::bind_method(D_METHOD("set_align", "v"), &RiveText::set_align);
    ClassDB::bind_method(D_METHOD("set_max_width", "v"), &RiveText::set_max_width);
    ClassDB::bind_method(D_METHOD("set_max_height", "v"), &RiveText::set_max_height);
    ClassDB::bind_method(D_METHOD("set_paragraph_spacing", "v"), &RiveText::set_paragraph_spacing);
    ClassDB::bind_method(D_METHOD("get_sizing"), &RiveText::get_sizing);
    ClassDB::bind_method(D_METHOD("get_overflow"), &RiveText::get_overflow);
    ClassDB::bind_method(D_METHOD("get_align"), &RiveText::get_align);
    ClassDB::bind_method(D_METHOD("get_max_width"), &RiveText::get_max_width);
    ClassDB::bind_method(D_METHOD("get_max_height"), &RiveText::get_max_height);
    ClassDB::bind_method(D_METHOD("get_paragraph_spacing"), &RiveText::get_paragraph_spacing);
}

RiveText::RiveText() {}
RiveText::~RiveText() {}

void RiveText::_ensure_raw_text() {
    if (!raw_text) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (factory) {
            raw_text = std::make_unique<rive::RawText>(factory);
            _apply_props();
        }
    }
}

void RiveText::_apply_props() {
    if (!raw_text) return;
    raw_text->sizing((rive::TextSizing)sizing);
    raw_text->overflow((rive::TextOverflow)overflow);
    raw_text->align((rive::TextAlign)align);
    raw_text->maxWidth(max_width);
    raw_text->maxHeight(max_height);
    raw_text->paragraphSpacing(paragraph_spacing);
}

void RiveText::clear() {
    _ensure_raw_text();
    if (raw_text) raw_text->clear();
    run_paints.clear();
    run_fonts.clear();
    run_texts.clear();
    run_sizes.clear();
    run_letter_spacings.clear();
}

void RiveText::append_run(String text, Ref<RiveFont> p_font, Ref<RivePaint> p_paint, float size, float line_height, float letter_spacing) {
    _ensure_raw_text();
    if (!raw_text) {
        UtilityFunctions::push_warning("RiveText.append_run: factory not ready.");
        return;
    }
    if (p_font.is_null() || !p_font->is_loaded()) {
        UtilityFunctions::push_warning("RiveText.append_run: font is null or not loaded.");
        return;
    }
    rive::rcp<rive::RenderPaint> paint_rcp;
    rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (p_paint.is_valid()) {
        // Borrow the paint's RenderPaint via an rcp that does NOT take ownership of the Godot ref.
        // Easiest: keep p_paint alive in run_paints, and create an rcp wrapping its render paint
        // by ref'ing it.
        rive::RenderPaint* rp = p_paint->get_render_paint(factory);
        if (rp) {
            // safe_rcp() pattern: ref then construct rcp.
            rp->ref();
            paint_rcp = rive::rcp<rive::RenderPaint>(rp);
        }
    }
    std::string std_text = std::string(text.utf8().get_data());
    raw_text->append(std_text, paint_rcp, p_font->get_font(), size, line_height, letter_spacing);
    run_fonts.push_back(p_font);
    run_texts.push_back(text);
    run_sizes.push_back(size);
    run_letter_spacings.push_back(letter_spacing);
    if (p_paint.is_valid()) run_paints.push_back(p_paint);
}

void RiveText::render(Ref<RiveRendererWrapper> renderer, Ref<RivePaint> override_paint) {
    if (!raw_text || renderer.is_null() || !renderer->get_renderer()) return;
    rive::rcp<rive::RenderPaint> override_rcp;
    if (override_paint.is_valid()) {
        rive::Factory* factory = RiveRenderRegistry::get_singleton()->get_factory();
        rive::RenderPaint* rp = override_paint->get_render_paint(factory);
        if (rp) {
            rp->ref();
            override_rcp = rive::rcp<rive::RenderPaint>(rp);
        }
    }
    raw_text->render(renderer->get_renderer(), override_rcp);
}

Rect2 RiveText::get_bounds() {
    if (!raw_text) return Rect2();
    rive::AABB b = raw_text->bounds();
    return Rect2(b.minX, b.minY, b.maxX - b.minX, b.maxY - b.minY);
}

Array RiveText::shape_glyphs() {
    Array out;
    if (run_texts.size() == 0) return out;

    // Concatenate all runs into one Unichar array and build matching TextRun
    // metadata.
    std::vector<rive::Unichar> unichars;
    std::vector<rive::TextRun> runs;
    runs.reserve(run_texts.size());
    int paint_idx = 0;
    for (int i = 0; i < run_texts.size(); i++) {
        const String& s = run_texts[i];
        Ref<RiveFont> rf = run_fonts[i];
        if (rf.is_null() || !rf->is_loaded() || s.length() == 0) continue;
        uint32_t start_count = (uint32_t)unichars.size();
        for (int j = 0; j < s.length(); j++) {
            unichars.push_back((rive::Unichar)s[j]);
        }
        rive::TextRun r;
        r.font = rf->get_font();
        r.size = run_sizes[i];
        r.lineHeight = -1.0f;
        r.letterSpacing = run_letter_spacings[i];
        r.unicharCount = (uint32_t)unichars.size() - start_count;
        r.script = 0;
        r.styleId = (uint16_t)i;
        r.level = 0;
        runs.push_back(r);
    }
    if (runs.empty() || unichars.empty()) return out;

    rive::rcp<rive::Font> shaper_font = runs[0].font;
    rive::SimpleArray<rive::Paragraph> paragraphs = shaper_font->shapeText(
        rive::Span<const rive::Unichar>(unichars.data(), unichars.size()),
        rive::Span<const rive::TextRun>(runs.data(), runs.size()));

    for (size_t pi = 0; pi < paragraphs.size(); pi++) {
        const rive::Paragraph& para = paragraphs[pi];
        for (size_t ri = 0; ri < para.runs.size(); ri++) {
            const rive::GlyphRun& gr = para.runs[ri];
            // Resolve which RivePaint to associate with this glyph run.
            Ref<RivePaint> paint_for_run;
            if ((int)gr.styleId < run_paints.size()) {
                paint_for_run = run_paints[gr.styleId];
            }
            float run_size = gr.size;
            for (size_t gi = 0; gi < gr.glyphs.size(); gi++) {
                rive::GlyphID gid = gr.glyphs[gi];
                rive::RawPath glyph_path = gr.font->getPath(gid);
                float gx = gr.xpos[gi] + gr.offsets[gi].x;
                float gy = gr.offsets[gi].y;
                Ref<RivePath> rp;
                rp.instantiate();
                for (auto it = glyph_path.begin(); it != glyph_path.end(); ++it) {
                    rive::PathVerb verb = it.verb();
                    const rive::Vec2D* p = it.pts();
                    auto tx = [&](const rive::Vec2D& v) -> rive::Vec2D {
                        return { v.x * run_size + gx, v.y * run_size + gy };
                    };
                    switch (verb) {
                        case rive::PathVerb::move:   { auto a = tx(p[0]); rp->move_to(a.x, a.y); break; }
                        case rive::PathVerb::line:   { auto a = tx(p[1]); rp->line_to(a.x, a.y); break; }
                        case rive::PathVerb::quad:   { auto a = tx(p[1]); auto b = tx(p[2]); rp->quad_to(a.x, a.y, b.x, b.y); break; }
                        case rive::PathVerb::cubic:  { auto a = tx(p[1]); auto b = tx(p[2]); auto c = tx(p[3]); rp->cubic_to(a.x, a.y, b.x, b.y, c.x, c.y); break; }
                        case rive::PathVerb::close:  rp->close(); break;
                    }
                }
                Dictionary entry;
                entry["path"] = rp;
                entry["paint"] = paint_for_run;
                entry["x"] = gx;
                entry["y"] = gy;
                entry["advance"] = gi + 1 < gr.xpos.size() ? (gr.xpos[gi + 1] - gr.xpos[gi]) : 0.0f;
                entry["glyph_id"] = (int)gid;
                out.push_back(entry);
            }
            (void)paint_idx;
        }
    }
    return out;
}

void RiveText::set_sizing(int v)            { sizing = v; _ensure_raw_text(); if (raw_text) raw_text->sizing((rive::TextSizing)v); }
void RiveText::set_overflow(int v)          { overflow = v; _ensure_raw_text(); if (raw_text) raw_text->overflow((rive::TextOverflow)v); }
void RiveText::set_align(int v)             { align = v; _ensure_raw_text(); if (raw_text) raw_text->align((rive::TextAlign)v); }
void RiveText::set_max_width(float v)       { max_width = v; _ensure_raw_text(); if (raw_text) raw_text->maxWidth(v); }
void RiveText::set_max_height(float v)      { max_height = v; _ensure_raw_text(); if (raw_text) raw_text->maxHeight(v); }
void RiveText::set_paragraph_spacing(float v){ paragraph_spacing = v; _ensure_raw_text(); if (raw_text) raw_text->paragraphSpacing(v); }
