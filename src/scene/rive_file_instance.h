#ifndef RIVE_FILE_INSTANCE_H
#define RIVE_FILE_INSTANCE_H

#include "rive_node.h"
#include "../resources/rive_file.h"
#include <rive/artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/linear_animation_instance.hpp>

using namespace godot;

class RiveFileInstance : public RiveNode {
    GDCLASS(RiveFileInstance, RiveNode);

private:
    Ref<RiveFile> rive_file_resource;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    std::unique_ptr<rive::LinearAnimationInstance> animation;
    
    String artboard_name;
    String state_machine_name;
    String animation_name;
    
    bool auto_play = true;

    void _load_artboard();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveFileInstance();
    ~RiveFileInstance();

    void set_rive_file(const Ref<RiveFile> &p_file);
    Ref<RiveFile> get_rive_file() const;

    void set_artboard_name(const String &p_name);
    String get_artboard_name() const;

    void set_state_machine_name(const String &p_name);
    String get_state_machine_name() const;
    
    void set_animation_name(const String &p_name);
    String get_animation_name() const;
    
    void set_auto_play(bool p_auto);
    bool get_auto_play() const;

    void advance(double delta) override;
    void draw(rive::Renderer *renderer) override;
    
    bool hit_test(Vector2 point);

    void pointer_down(Vector2 position) override;
    void pointer_up(Vector2 position) override;
    void pointer_move(Vector2 position) override;
};

#endif
