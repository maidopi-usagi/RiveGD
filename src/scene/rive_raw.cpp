#include "rive_raw.h"
#include <godot_cpp/core/class_db.hpp>

void RiveRaw::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_bounds", "bounds"), &RiveRaw::set_bounds);
    ClassDB::bind_method(D_METHOD("get_bounds"), &RiveRaw::get_bounds);
    ADD_PROPERTY(PropertyInfo(Variant::RECT2, "bounds"), "set_bounds", "get_bounds");
    ADD_SIGNAL(MethodInfo("draw_rive", PropertyInfo(Variant::OBJECT, "renderer", PROPERTY_HINT_RESOURCE_TYPE, "RiveRendererWrapper")));
}

RiveRaw::RiveRaw() {
    renderer_wrapper.instantiate();
}

RiveRaw::~RiveRaw() {}

void RiveRaw::set_bounds(const Rect2 &p_bounds) {
    bounds = p_bounds;
    queue_redraw();
}

void RiveRaw::draw(rive::Renderer* renderer) {
    // A connected script can queue/free this node while the signal is being
    // emitted. Keep the wrapper itself alive until the callback returns.
    Ref<RiveRendererWrapper> wrapper = renderer_wrapper;
    if (wrapper.is_valid()) {
        wrapper->set_renderer(renderer);
        
        renderer->save();
        Transform2D xform = get_transform();
        rive::Mat2D rive_transform(
            xform.columns[0].x, xform.columns[0].y,
            xform.columns[1].x, xform.columns[1].y,
            xform.columns[2].x, xform.columns[2].y
        );
        renderer->transform(rive_transform);
        
        emit_signal("draw_rive", wrapper);
        
        renderer->restore();
        wrapper->set_renderer(nullptr);
    }
}
