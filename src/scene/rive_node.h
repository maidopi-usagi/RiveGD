#ifndef RIVE_NODE_H
#define RIVE_NODE_H

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "rive_player.h"
#include <rive/renderer.hpp>

using namespace godot;

class RiveNode : public Node2D {
    GDCLASS(RiveNode, Node2D);

public:
    enum UpdateMode { UPDATE_ALWAYS, UPDATE_WHEN_NEEDED, UPDATE_MANUAL };

private:
    UpdateMode update_mode = UPDATE_ALWAYS;
    bool playing = true;
    double speed_scale = 1.0;
    double volume = 1.0;

    bool rive_update_requested = true;
    ObjectID reported_view_model_id;

    void _on_player_changed() {
        request_rive_update();
        const Ref<RivePlayer> player = get_rive_player();
        const Ref<RiveViewModelInstance> vm = player.is_valid() ? player->get_rive_view_model_instance() : Ref<RiveViewModelInstance>();
        const ObjectID id = vm.is_valid() ? ObjectID(vm->get_instance_id()) : ObjectID();
        if (id != reported_view_model_id) {
            reported_view_model_id = id;
            emit_signal("view_model_changed");
        }
    }

    void _on_player_event(const String &p_name, const Dictionary &p_payload) {
        request_rive_update();
        emit_signal("rive_event", p_name, p_payload);
    }
    void _on_player_finished(const String &p_name) {
        request_rive_update();
        emit_signal("animation_finished", p_name);
    }
    void _on_player_looped(const String &p_name) {
        request_rive_update();
        emit_signal("animation_looped", p_name);
    }
    void _on_player_state_changed(const String &p_name) {
        request_rive_update();
        emit_signal("state_changed", p_name);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("request_rive_update"), &RiveNode::request_rive_update);
        ClassDB::bind_method(D_METHOD("set_update_mode", "mode"), &RiveNode::set_update_mode);
        ClassDB::bind_method(D_METHOD("get_update_mode"), &RiveNode::get_update_mode);
        ClassDB::bind_method(D_METHOD("set_playing", "playing"), &RiveNode::set_playing);
        ClassDB::bind_method(D_METHOD("is_playing"), &RiveNode::is_playing);
        ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"), &RiveNode::set_speed_scale);
        ClassDB::bind_method(D_METHOD("get_speed_scale"), &RiveNode::get_speed_scale);
        ClassDB::bind_method(D_METHOD("play"), &RiveNode::play);
        ClassDB::bind_method(D_METHOD("pause"), &RiveNode::pause);
        ClassDB::bind_method(D_METHOD("stop"), &RiveNode::stop);
        ClassDB::bind_method(D_METHOD("reset"), &RiveNode::reset);
        ClassDB::bind_method(D_METHOD("advance", "delta"), &RiveNode::advance);
        ClassDB::bind_method(D_METHOD("seek", "time"), &RiveNode::seek);
        ClassDB::bind_method(D_METHOD("get_time"), &RiveNode::get_time);
        ClassDB::bind_method(D_METHOD("get_duration"), &RiveNode::get_duration);
        ClassDB::bind_method(D_METHOD("get_player"), &RiveNode::get_player);
        ClassDB::bind_method(D_METHOD("set_volume", "volume"), &RiveNode::set_volume);
        ClassDB::bind_method(D_METHOD("get_volume"), &RiveNode::get_volume);
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_volume", "get_volume");
        ClassDB::bind_method(D_METHOD("bind_view_model", "instance"), &RiveNode::bind_view_model);
        ClassDB::bind_method(D_METHOD("_on_player_changed"), &RiveNode::_on_player_changed);
        ADD_SIGNAL(MethodInfo("view_model_changed"));
        ADD_PROPERTY(PropertyInfo(Variant::INT, "update_mode", PROPERTY_HINT_ENUM, "Always,When Needed,Manual"), "set_update_mode", "get_update_mode");
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing", "is_playing");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_speed_scale", "get_speed_scale");
        BIND_ENUM_CONSTANT(UPDATE_ALWAYS);
        BIND_ENUM_CONSTANT(UPDATE_WHEN_NEEDED);
        BIND_ENUM_CONSTANT(UPDATE_MANUAL);
        ClassDB::bind_method(D_METHOD("_on_player_event", "name", "payload"), &RiveNode::_on_player_event);
        ClassDB::bind_method(D_METHOD("_on_player_finished", "name"), &RiveNode::_on_player_finished);
        ClassDB::bind_method(D_METHOD("_on_player_looped", "name"), &RiveNode::_on_player_looped);
        ClassDB::bind_method(D_METHOD("_on_player_state_changed", "name"), &RiveNode::_on_player_state_changed);
        ADD_SIGNAL(MethodInfo("rive_event", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "payload")));
        ADD_SIGNAL(MethodInfo("animation_finished", PropertyInfo(Variant::STRING, "name")));
        ADD_SIGNAL(MethodInfo("animation_looped", PropertyInfo(Variant::STRING, "name")));
        ADD_SIGNAL(MethodInfo("state_changed", PropertyInfo(Variant::STRING, "name")));
    }

public:
    virtual Ref<RivePlayer> get_rive_player() const { return Ref<RivePlayer>(); }
    Ref<RivePlayer> get_player() const { return get_rive_player(); }
    bool bind_view_model(const Ref<RiveViewModelInstance> &p_instance) {
        const Ref<RivePlayer> player = get_rive_player();
        return player.is_valid() && player->bind_view_model(p_instance);
    }
    void set_update_mode(UpdateMode p_mode) {
        ERR_FAIL_INDEX(p_mode, UPDATE_MANUAL + 1);
        if (update_mode == p_mode) return;
        update_mode = p_mode;
        request_rive_update();
    }
    UpdateMode get_update_mode() const { return update_mode; }
    void set_playing(bool p_playing) {
        if (playing == p_playing) return;
        playing = p_playing;
        request_rive_update();
    }
    bool is_playing() const { return playing; }
    void set_speed_scale(double p_speed) {
        ERR_FAIL_COND_MSG(!Math::is_finite(p_speed) || p_speed < 0.0, "Rive speed_scale must be finite and non-negative.");
        speed_scale = p_speed;
        request_rive_update();
    }
    double get_speed_scale() const { return speed_scale; }
    void set_volume(double p_volume) {
        if (!Math::is_finite(p_volume) || p_volume < 0.0 || p_volume > 1.0) return;
        volume = p_volume;
        const Ref<RivePlayer> player = get_rive_player();
        if (player.is_valid()) player->set_volume(volume);
    }
    double get_volume() const { return volume; }
    void play() { set_playing(true); }
    void pause() { set_playing(false); }
    void stop() { set_playing(false); reset(); }
    void reset() {
        request_rive_update();
        const Ref<RivePlayer> player = get_rive_player();
        if (player.is_valid()) player->reset();
    }
    bool seek(double p_time) {
        request_rive_update();
        const Ref<RivePlayer> player = get_rive_player();
        return player.is_valid() && player->seek(p_time);
    }
    double get_time() const {
        const Ref<RivePlayer> player = get_rive_player();
        return player.is_valid() ? player->get_time() : 0.0;
    }
    double get_duration() const {
        const Ref<RivePlayer> player = get_rive_player();
        return player.is_valid() ? player->get_duration() : 0.0;
    }
    void connect_rive_player_signals(Object *p_player, bool p_connect = true) {
        if (!p_player) return;
        const char *signals[] = { "changed", "rive_event", "animation_finished", "animation_looped", "state_changed" };
        const char *methods[] = { "_on_player_changed", "_on_player_event", "_on_player_finished", "_on_player_looped", "_on_player_state_changed" };
        if (p_connect) {
            const Ref<RivePlayer> player = get_rive_player();
            if (player.is_valid()) player->set_volume(volume);
            const Ref<RiveViewModelInstance> vm = player.is_valid() ? player->get_rive_view_model_instance() : Ref<RiveViewModelInstance>();
            reported_view_model_id = vm.is_valid() ? ObjectID(vm->get_instance_id()) : ObjectID();
        }
        for (int i = 0; i < 5; ++i) {
            if (!p_player->has_signal(signals[i])) continue;
            const Callable callback(this, methods[i]);
            const bool connected = p_player->is_connected(signals[i], callback);
            if (p_connect && !connected) p_player->connect(signals[i], callback);
            else if (!p_connect && connected) p_player->disconnect(signals[i], callback);
        }
    }
    bool can_advance_rive() const {
        return is_inside_tree() && is_visible_in_tree() && can_process();
    }
    void request_rive_update() {
        rive_update_requested = true;
        queue_redraw();
    }
    bool consume_rive_update() {
        const bool requested = rive_update_requested;
        rive_update_requested = false;
        return requested;
    }
    bool is_rive_update_requested() const { return rive_update_requested; }
    virtual bool wants_continuous_redraw() const { return update_mode != UPDATE_MANUAL; }
    PackedStringArray _get_configuration_warnings() const override {
        PackedStringArray warnings;
        Node *ancestor = get_parent();
        while (ancestor && !ancestor->is_class("RiveCanvas2D")) ancestor = ancestor->get_parent();
        if (!ancestor) warnings.push_back("Rive nodes must be descendants of a RiveCanvas2D to render and advance.");
        if (!get_transform().is_finite()) warnings.push_back("The Rive node transform must be finite.");
        return warnings;
    }
    virtual void draw(rive::Renderer* renderer) {}
    virtual void advance(double delta) {}
    virtual void advance_rive_automatically(double delta) {
        if (update_mode != UPDATE_MANUAL) advance(delta);
    }
    virtual Rect2 get_rive_bounds() const { return Rect2(); }
    virtual void pointer_down(Vector2 position) {}
    virtual void pointer_up(Vector2 position) {}
    virtual void pointer_move(Vector2 position) {}
};

VARIANT_ENUM_CAST(RiveNode::UpdateMode);

#endif
