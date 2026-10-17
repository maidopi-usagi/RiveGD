#include "rive_player.h"
#include "../renderer/rive_renderer.h"
#include <cmath>

bool RivePlayer::set_animation_layers(const Array &p_layers) {
    if (!artboard || state_machine || !animation || p_layers.size() > 32) return false;
    bool valid = false;
    rive_integration::run_on_rive_render_thread([&]() {
        std::vector<AnimationLayer> candidates;
        for (int64_t i = 0; i < p_layers.size(); ++i) {
            if (p_layers[i].get_type() != Variant::DICTIONARY) return;
            const Dictionary data = p_layers[i];
            if (!data.has("name") || data["name"].get_type() != Variant::STRING) return;
            const String name = data["name"];
            const Variant weight_value = data.get("weight", 1.0);
            const Variant speed_value = data.get("speed", 1.0);
            const Variant time_value = data.get("time", 0.0);
            for (const Variant *value : {&weight_value, &speed_value, &time_value}) {
                if (value->get_type() != Variant::FLOAT && value->get_type() != Variant::INT) return;
            }
            const double weight = weight_value, speed = speed_value, time = time_value;
            const Variant loop_value = data.get("loop", LOOP_AUTHORED);
            const Variant direction_value = data.get("direction", 1);
            if (loop_value.get_type() != Variant::INT || direction_value.get_type() != Variant::INT) return;
            const int64_t loop = loop_value, direction = direction_value;
            if (!std::isfinite(weight) || weight < 0 || weight > 1 || !std::isfinite(speed) || speed < 0 || speed > 100 ||
                    !std::isfinite(time) || time < 0 || time > 1e9 || loop < LOOP_AUTHORED || loop > LOOP_PING_PONG || (direction != -1 && direction != 1)) return;
            AnimationLayer layer;
            layer.instance = artboard->animationNamed(name.utf8().get_data());
            if (!layer.instance) return;
            layer.instance->reset(direction);
            if (data.has("time")) layer.instance->time(time);
            layer.instance->direction(direction);
            layer.instance->loopValue(loop);
            layer.instance->addNestedEventListener(this);
            layer.weight = weight;
            layer.speed = speed;
            layer.initial_direction = direction;
            candidates.push_back(std::move(layer));
        }
        animation_layers = std::move(candidates);
        valid = true;
    });
    if (valid) emit_signal("changed");
    return valid;
}

Array RivePlayer::get_animation_layers() const {
    Array result;
    rive_integration::run_on_rive_render_thread([&]() {
        for (const auto &layer : animation_layers) {
            Dictionary data;
            data["name"] = String::utf8(layer.instance->name().c_str());
            data["weight"] = layer.weight;
            data["speed"] = layer.speed;
            data["time"] = layer.instance->time();
            data["loop"] = layer.instance->loopValue();
            data["direction"] = layer.instance->direction() < 0 ? -1 : 1;
            result.push_back(data);
        }
    });
    return result;
}

bool RivePlayer::cross_fade_animation(const String &p_name, double p_seconds) {
    if (!artboard || !animation || state_machine || !std::isfinite(p_seconds) || p_seconds < 0 || p_seconds > 60) return false;
    bool valid = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto next = artboard->animationNamed(p_name.utf8().get_data());
        if (!next) return;
        next->loopValue(animation_loop);
        next->reset(animation_direction);
        next->direction(animation_direction);
        next->addNestedEventListener(this);
        fading_animation = p_seconds > 0 ? std::move(animation) : nullptr;
        animation = std::move(next);
        animation_layers.clear();
        fade_duration = p_seconds;
        fade_elapsed = 0;
        animation_completed = false;
        requested_animation = current_animation = p_name;
        if (p_seconds == 0) animation->advanceAndApply(0);
        valid = true;
    });
    if (valid) emit_signal("changed");
    return valid;
}
