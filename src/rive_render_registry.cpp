#include "rive_render_registry.h"
#include <algorithm>

static RiveRenderRegistry* _singleton = nullptr;

RiveRenderRegistry* RiveRenderRegistry::get_singleton() {
    if (!_singleton) {
        _singleton = new RiveRenderRegistry();
    }
    return _singleton;
}

void RiveRenderRegistry::add_drawable(RiveDrawable* drawable) {
    std::lock_guard<std::mutex> lock(mutex);
    drawables.push_back(drawable);
}

void RiveRenderRegistry::remove_drawable(RiveDrawable* drawable) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find(drawables.begin(), drawables.end(), drawable);
    if (it != drawables.end()) {
        drawables.erase(it);
    }
}

void RiveRenderRegistry::draw_all(rive::Renderer* renderer) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto* drawable : drawables) {
        drawable->draw(renderer);
    }
}
