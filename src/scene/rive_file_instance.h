#ifndef RIVE_FILE_INSTANCE_H
#define RIVE_FILE_INSTANCE_H

#include "rive_node.h"
#include "../resources/rive_file.h"
#include "rive_player.h"

using namespace godot;

class RiveFileInstance : public RiveNode {
    GDCLASS(RiveFileInstance, RiveNode);

private:
    Ref<RiveFile> rive_file_resource;
    Ref<RivePlayer> rive_player;
    
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
    Rect2 get_rive_bounds() const override;
    
    bool hit_test(Vector2 point);

    void pointer_down(Vector2 position) override;
    void pointer_up(Vector2 position) override;
    void pointer_move(Vector2 position) override;

    Ref<RiveViewModelInstance> get_view_model_instance() const;
};

#endif
