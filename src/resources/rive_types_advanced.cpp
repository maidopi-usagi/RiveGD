#include "rive_types.h"
#include "../renderer/rive_renderer.h"
#include <godot_cpp/classes/font_file.hpp>
#include <rive/text/raw_text.hpp>
#include <rive/text/font_hb.hpp>
#include <cmath>
#include <mutex>

namespace {
std::mutex fallback_mutex;
std::vector<rive::rcp<rive::Font>> fallback_fonts;
rive::rcp<rive::Font> fallback_font(rive::Unichar missing, uint32_t index, const rive::Font *) {
    std::lock_guard<std::mutex> lock(fallback_mutex);
    for (size_t i = index; i < fallback_fonts.size(); ++i) {
        if (fallback_fonts[i]->hasGlyph(missing)) return fallback_fonts[i];
    }
    return nullptr;
}
uint32_t font_tag(const Variant &key) {
    if (key.get_type() == Variant::INT) {
        int64_t value = key;
        return value > 0 && value <= UINT32_MAX ? static_cast<uint32_t>(value) : 0;
    }
    if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME) return 0;
    const String name = key;
    if (name.length() != 4) return 0;
    uint32_t tag = 0;
    for (int i = 0; i < 4; ++i) {
        if (name[i] < 32 || name[i] > 126) return 0;
        tag = (tag << 8) | name[i];
    }
    return tag;
}
String font_tag_name(uint32_t tag) {
    String name;
    for (int i = 3; i >= 0; --i) name += String::chr((tag >> (i * 8)) & 255);
    return name;
}
}

Array RiveFont::get_axes() const {
    Array axes;
    if (font) for (uint16_t i = 0; i < font->getAxisCount(); ++i) {
        const auto axis = font->getAxis(i);
        Dictionary data;
        data["tag"] = font_tag_name(axis.tag); data["min"] = axis.min;
        data["default"] = axis.def; data["max"] = axis.max;
        data["value"] = font->getAxisValue(axis.tag);
        axes.push_back(data);
    }
    return axes;
}

PackedStringArray RiveFont::get_features() const {
    PackedStringArray result;
    if (font) for (auto tag : font->features()) result.push_back(font_tag_name(tag));
    return result;
}

Ref<RiveFont> RiveFont::with_options(const Dictionary &variations, const Dictionary &features) const {
    if (!font || variations.size() > 64 || features.size() > 256) return Ref<RiveFont>();
    std::vector<rive::Font::Coord> coords;
    std::vector<rive::Font::Feature> options;
    const Array axes = variations.keys();
    for (int64_t i = 0; i < axes.size(); ++i) {
        const uint32_t tag = font_tag(axes[i]);
        const Variant value = variations[axes[i]];
        if (!tag || (value.get_type() != Variant::INT && value.get_type() != Variant::FLOAT)) return Ref<RiveFont>();
        const double number = value;
        bool valid = false;
        for (uint16_t j = 0; j < font->getAxisCount(); ++j) {
            const auto axis = font->getAxis(j);
            if (tag == axis.tag && std::isfinite(number) && number >= axis.min && number <= axis.max) valid = true;
        }
        if (!valid) return Ref<RiveFont>();
        coords.push_back({tag, static_cast<float>(number)});
    }
    const Array feature_names = features.keys();
    for (int64_t i = 0; i < feature_names.size(); ++i) {
        const uint32_t tag = font_tag(feature_names[i]);
        const Variant value = features[feature_names[i]];
        if (!tag || (value.get_type() != Variant::INT && value.get_type() != Variant::BOOL)) return Ref<RiveFont>();
        const int64_t number = value;
        if (number < 0 || number > UINT32_MAX) return Ref<RiveFont>();
        options.push_back({tag, static_cast<uint32_t>(number)});
    }
    auto configured = font->withOptions(rive::Span<const rive::Font::Coord>(coords.data(), coords.size()),
            rive::Span<const rive::Font::Feature>(options.data(), options.size()));
    if (!configured) return Ref<RiveFont>();
    Ref<RiveFont> result; result.instantiate(); result->font = std::move(configured);
    return result;
}

bool RiveFont::set_fallback_fonts(const Array &fonts) {
    if (fonts.size() > 32) return false;
    std::vector<rive::rcp<rive::Font>> candidates;
    for (int64_t i = 0; i < fonts.size(); ++i) {
        Ref<RiveFont> wrapped = fonts[i];
        if (wrapped.is_valid() && wrapped->is_loaded()) { candidates.push_back(wrapped->font); continue; }
        Ref<FontFile> file = fonts[i];
        if (file.is_null()) return false;
        const PackedByteArray bytes = file->get_data();
        auto decoded = HBFont::Decode(rive::Span<const uint8_t>(bytes.ptr(), bytes.size()));
        if (!decoded) return false;
        candidates.push_back(std::move(decoded));
    }
    std::lock_guard<std::mutex> lock(fallback_mutex);
    fallback_fonts = std::move(candidates);
    rive::Font::gFallbackProc = fallback_fonts.empty() ? nullptr : fallback_font;
    return true;
}

void RiveFont::clear_fallback_fonts() { set_fallback_fonts(Array()); }

void RiveText::set_direction(int value) {
    if (value < -1 || value > 1) return;
    direction = value; _ensure_raw_text(); _apply_props();
}
void RiveText::set_logical_align(int value) {
    if (value < 0 || value > 2) return;
    logical_align = value; _ensure_raw_text(); _apply_props();
}
void RiveText::set_wrap(int value) {
    if (value < 0 || value > 1) return;
    wrap = value; _ensure_raw_text(); _apply_props();
}
void RiveText::set_word_break(int value) {
    if (value < 0 || value > 2) return;
    word_break = value; _ensure_raw_text(); _apply_props();
}
Array RiveText::get_lines() {
    _ensure_raw_text(); _refresh_run_paints();
    Array result;
    if (!raw_text) return result;
    for (uint32_t i = 0; i < raw_text->lineCount(); ++i) {
        Dictionary line;
        line["index"] = i; line["top"] = raw_text->lineTop(i);
        line["direction"] = static_cast<int>(raw_text->lineDirection(i));
        line["glyph_count"] = raw_text->lineGlyphCount(i);
        result.push_back(line);
    }
    return result;
}

void RiveRendererWrapper::modulate_opacity(float opacity) {
    if (renderer && std::isfinite(opacity) && opacity >= 0 && opacity <= 1) renderer->modulateOpacity(opacity);
}
void RiveRendererWrapper::modulate_color(Color color, bool replace) {
    if (!renderer || !std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) || !std::isfinite(color.a)) return;
    color = color.clamp();
    const uint32_t value = (uint32_t(color.a * 255) << 24) | (uint32_t(color.r * 255) << 16) |
            (uint32_t(color.g * 255) << 8) | uint32_t(color.b * 255);
    renderer->modulateColor(value, replace);
}
void RiveRendererWrapper::clip_stroke(Ref<RivePath> path, float thickness, int join, int cap, int position) {
    if (!renderer || path.is_null() || !std::isfinite(thickness) || thickness <= 0 || join < 0 || join > 2 || cap < 0 || cap > 2 || position < 0 || position > 2) return;
    auto *factory = RiveRenderRegistry::get_singleton()->get_factory();
    if (!factory) return;
    rive::StrokeParams params;
    params.thickness = thickness; params.join = static_cast<rive::StrokeJoin>(join);
    params.cap = static_cast<rive::StrokeCap>(cap); params.position = static_cast<rive::StrokePosition>(position);
    renderer->clipStroke(path->get_render_path(factory), params);
}
