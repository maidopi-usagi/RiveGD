#include "rive_raw.h"
#include <godot_cpp/core/class_db.hpp>

void RiveRaw::_bind_methods() {
    ADD_SIGNAL(MethodInfo("draw_rive", PropertyInfo(Variant::OBJECT, "renderer", PROPERTY_HINT_RESOURCE_TYPE, "RiveRendererWrapper")));
}

RiveRaw::RiveRaw() {
    renderer_wrapper.instantiate();
}

RiveRaw::~RiveRaw() {}

void RiveRaw::draw(rive::Renderer* renderer) {
    if (renderer_wrapper.is_valid()) {
        renderer_wrapper->set_renderer(renderer);
        
        // Apply transform
        renderer->save();
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        renderer->transform(rive_transform);
        
        emit_signal("draw_rive", renderer_wrapper);
        
        renderer->restore();
        renderer_wrapper->set_renderer(nullptr);
    }
}
