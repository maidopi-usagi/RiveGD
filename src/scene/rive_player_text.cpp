#include "rive_player.h"
#include "../renderer/rive_renderer.h"
#include <rive/text/text_selection_controller.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_input.hpp>
#include <rive/input/focus_manager.hpp>
#include <rive/input/focus_node.hpp>
#include <rive/focus_data.hpp>

namespace {
rive::TextInput *focused_text(rive::StateMachineInstance *state) {
    auto *manager = state ? state->focusManager() : nullptr;
    auto focus = manager ? manager->primaryFocus() : nullptr;
    for (auto *node = focus.get(); node; node = node->parent()) {
        if (auto *input = dynamic_cast<rive::TextInput *>(node->focusable())) return input;
        if (auto *data = dynamic_cast<rive::FocusData *>(node->focusable())) {
            auto *parent = data->parent();
            if (parent && parent->is<rive::TextInput>()) return parent->as<rive::TextInput>();
        }
    }
    return nullptr;
}
}

bool RivePlayer::update_text_composition(const String &p_text, Vector2i p_selection) {
    if (p_text.length() > 65536 || p_selection.x < 0 || p_selection.y < 0 || p_selection.x > p_text.length() ||
            int64_t(p_selection.x) + p_selection.y > p_text.length()) return false;
    for (int64_t i = 0; i < p_text.length(); ++i) if (p_text[i] == 0) return false;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *input = focused_text(state_machine.get());
        if (!input) return;
        input->rawTextInput()->composition(p_text.utf8().get_data(), p_selection.x, p_selection.y);
        input->markShapeDirty();
        artboard->advance(0);
        handled = true;
    });
    if (handled) emit_signal("changed");
    return handled;
}

void RivePlayer::cancel_text_composition() {
    bool changed = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *input = focused_text(state_machine.get());
        if (input && input->rawTextInput()->isComposing()) {
            input->rawTextInput()->cancelComposition(); input->markShapeDirty(); artboard->advance(0); changed = true;
        }
    });
    if (changed) emit_signal("changed");
}

bool RivePlayer::set_text_selectable(const String &p_name, bool p_enabled, const String &p_path) {
    if (!artboard || p_name.is_empty() || p_path.length() > 4096) return false;
    bool applied = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *nested = p_path.is_empty() ? nullptr : artboard->nestedArtboardAtPath(p_path.utf8().get_data());
        rive::Artboard *board = p_path.is_empty() ? artboard.get() : nested ? nested->artboardInstance() : nullptr;
        auto *text = board ? board->find<rive::Text>(p_name.utf8().get_data()) : nullptr;
        if (!text) return;
        const uint16_t flag = static_cast<uint16_t>(rive::DrawableFlag::Selectable);
        text->drawableFlags(p_enabled ? text->drawableFlags() | flag : text->drawableFlags() & ~flag);
        if (!text_selection) text_selection = std::make_unique<rive::TextSelectionController>();
        text_selection->synchronize(artboard.get());
        applied = true;
    });
    if (applied) emit_signal("changed");
    return applied;
}

Dictionary RivePlayer::hit_test_selectable_text(Vector2 p_position) {
    Dictionary result;
    if (!artboard || !p_position.is_finite()) return result;
    rive_integration::run_on_rive_render_thread([&]() {
        if (!text_selection) text_selection = std::make_unique<rive::TextSelectionController>();
        text_selection->synchronize(artboard.get());
        const auto targets = text_selection->targetIds();
        for (auto i = targets.rbegin(); i != targets.rend(); ++i) {
            auto *text = text_selection->target(*i);
            uint32_t offset = 0; float distance = 0;
            if (text_selection->hitTest(text, rive::Vec2D(p_position.x, p_position.y), false, offset, distance)) {
                result["id"] = static_cast<int64_t>(*i); result["offset"] = static_cast<int64_t>(offset);
                return;
            }
        }
    });
    return result;
}

Array RivePlayer::get_selectable_texts() {
    Array result;
    if (!artboard) return result;
    rive_integration::run_on_rive_render_thread([&]() {
        if (!text_selection) text_selection = std::make_unique<rive::TextSelectionController>();
        text_selection->synchronize(artboard.get());
        for (auto id : text_selection->targetIds()) {
            auto *text = text_selection->target(id);
            if (!text) continue;
            const auto bounds = text_selection->rootBounds(text);
            Dictionary data;
            data["id"] = static_cast<int64_t>(id);
            data["name"] = String::utf8(text->name().c_str());
            data["text"] = String::utf8(text_selection->sourceText(text).c_str());
            data["bounds"] = Rect2(bounds.minX, bounds.minY, bounds.width(), bounds.height());
            result.push_back(data);
        }
    });
    return result;
}

bool RivePlayer::set_text_selection(int64_t p_anchor_id, int64_t p_start, int64_t p_extent_id, int64_t p_end) {
    if (!text_selection || !artboard || p_anchor_id <= 0 || p_anchor_id > UINT32_MAX || p_extent_id <= 0 || p_extent_id > UINT32_MAX ||
            p_start < 0 || p_start > UINT32_MAX || p_end < 0 || p_end > UINT32_MAX) return false;
    bool selected = false;
    rive_integration::run_on_rive_render_thread([&]() {
        text_selection->synchronize(artboard.get());
        auto *anchor = text_selection->target(p_anchor_id);
        auto *extent = text_selection->target(p_extent_id);
        if (!anchor || !extent) return;
        if (p_start > String::utf8(text_selection->sourceText(anchor).c_str()).length() ||
                p_end > String::utf8(text_selection->sourceText(extent).c_str()).length()) return;
        selected = text_selection->select(anchor, p_start, extent, p_end);
    });
    if (selected) emit_signal("changed");
    return selected;
}

bool RivePlayer::selection_pointer(int64_t p_target_id, Vector2 p_position, int p_phase, int p_pointer_id, bool p_extend) {
    if (!text_selection || !artboard || !p_position.is_finite() || p_pointer_id < 0 || p_phase < 0 || p_phase > 3 || p_target_id < 0 || p_target_id > UINT32_MAX) return false;
    bool handled = false;
    rive_integration::run_on_rive_render_thread([&]() {
        text_selection->synchronize(artboard.get());
        const rive::Vec2D point(p_position.x, p_position.y);
        if (p_phase == 0) {
            auto *text = text_selection->target(p_target_id);
            if (text) handled = text_selection->pointerDown(text, point, p_pointer_id, p_extend);
        } else if (p_phase == 1) handled = text_selection->pointerUp(point, p_pointer_id);
        else if (p_phase == 2) handled = text_selection->pointerMove(point, p_pointer_id);
        else handled = text_selection->pointerCancel(p_pointer_id);
    });
    if (handled) emit_signal("changed");
    return handled;
}

void RivePlayer::clear_text_selection() {
    rive_integration::run_on_rive_render_thread([&]() { if (text_selection) text_selection->clear(); });
    emit_signal("changed");
}

Dictionary RivePlayer::get_text_input_state() const {
    Dictionary result;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *input = focused_text(state_machine.get());
        if (!input) return;
        auto *raw = input->rawTextInput();
        const auto cursor = raw->committedCursor();
        result["text"] = input->obscured() ? String() : String::utf8(raw->committedText().c_str());
        result["composing"] = raw->isComposing();
        result["display_text"] = input->obscured() ? String() : String::utf8(raw->text().c_str());
        result["obscured"] = input->obscured();
        result["multiline"] = input->multiline();
        result["selection_from"] = static_cast<int64_t>(cursor.first().codePointIndex());
        result["selection_to"] = static_cast<int64_t>(cursor.last().codePointIndex());
        const auto caret = raw->cursorVisualPosition();
        if (caret.found()) {
            const auto bounds = input->worldTransform().mapBoundingBox(rive::AABB(caret.x(), caret.top(), caret.x() + 1, caret.bottom()));
            rive::AABB root_bounds = rive::AABB::forExpansion();
            for (auto point : {rive::Vec2D(bounds.minX, bounds.minY), rive::Vec2D(bounds.maxX, bounds.minY),
                    rive::Vec2D(bounds.maxX, bounds.maxY), rive::Vec2D(bounds.minX, bounds.maxY)}) {
                const auto root = input->artboard()->rootTransform(point);
                rive::AABB::expandTo(root_bounds, root);
            }
            result["caret_rect"] = Rect2(root_bounds.minX, root_bounds.minY, root_bounds.width(), root_bounds.height());
        }
    });
    return result;
}
