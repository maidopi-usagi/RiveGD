#ifndef RIVE_MULTI_INSTANCE_H
#define RIVE_MULTI_INSTANCE_H

#include "rive_node.h"
#include "../resources/rive_file.h"
#include <rive/artboard.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/linear_animation_instance.hpp>

using namespace godot;

class RiveMultiInstance : public RiveNode {
    GDCLASS(RiveMultiInstance, RiveNode);

private:
    Ref<RiveFile> rive_file_resource;
    std::unique_ptr<rive::ArtboardInstance> artboard;
    std::unique_ptr<rive::StateMachineInstance> state_machine;
    std::unique_ptr<rive::LinearAnimationInstance> animation;
    
    String artboard_name;
    String state_machine_name;
    String animation_name;
    bool auto_play = true;

    PackedVector2Array positions;
    PackedFloat32Array scales;
    PackedFloat32Array rotations;
    // Or just a list of Transforms, but Godot usually exposes Arrays.
    // For simplicity, let's use a PackedVector2Array for positions first, 
    // maybe later full transforms.
    // Let's use Transform2D array for maximum flexibility.
    Array transforms; 

    void _load_artboard();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    RiveMultiInstance();
    ~RiveMultiInstance();

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

    void set_transforms(const Array &p_transforms);
    Array get_transforms() const;

    void advance(double delta) override;
    void draw(rive::Renderer *renderer) override;
    Rect2 get_rive_bounds() const override;
};

#endif
