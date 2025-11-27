#ifndef RIVE_RENDER_REGISTRY_H
#define RIVE_RENDER_REGISTRY_H

#include <vector>
#include <mutex>

namespace rive {
    class Renderer;
    class Factory;
}

class RendererState {
public:
    virtual ~RendererState() {}
};

class RiveDrawable {
public:
    virtual ~RiveDrawable() { delete renderer_state; }
    virtual void draw(rive::Renderer* renderer) = 0;
    
    RendererState* renderer_state = nullptr;
};

class RiveRenderRegistry {
    std::vector<RiveDrawable*> drawables;
    std::mutex mutex;
    rive::Factory* factory = nullptr;

public:
    static RiveRenderRegistry* get_singleton();

    void set_factory(rive::Factory* p_factory) { factory = p_factory; }
    rive::Factory* get_factory() { return factory; }

    void add_drawable(RiveDrawable* drawable);
    void remove_drawable(RiveDrawable* drawable);
    
    void draw_all(rive::Renderer* renderer);
};

#endif // RIVE_RENDER_REGISTRY_H
