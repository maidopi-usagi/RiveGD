#ifndef RIVE_RAW_H
#define RIVE_RAW_H

#include "rive_node.h"
#include "../resources/rive_types.h"

using namespace godot;

class RiveRaw : public RiveNode {
    GDCLASS(RiveRaw, RiveNode);

private:
    Ref<RiveRendererWrapper> renderer_wrapper;

protected:
    static void _bind_methods();

public:
    RiveRaw();
    ~RiveRaw();

    void draw(rive::Renderer* renderer) override;
};

#endif
