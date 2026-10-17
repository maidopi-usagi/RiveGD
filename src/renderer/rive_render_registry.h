#ifndef RIVE_RENDER_REGISTRY_H
#define RIVE_RENDER_REGISTRY_H

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>

namespace rive {
    class Renderer;
    class Factory;
    class RenderImage;
}

class RendererState {
public:
    virtual ~RendererState() {}
};

class RiveCommandRecording;

class RiveCommandRecordingTarget {
public:
    virtual ~RiveCommandRecordingTarget() = default;
    virtual void record_image_mesh(const rive::RenderImage *p_image,
                                   const float *p_vertices,
                                   const float *p_uv_coords,
                                   const uint16_t *p_indices,
                                   uint32_t p_vertex_count,
                                   uint32_t p_index_count,
                                   int p_blend_mode,
                                   float p_opacity, int p_sampler_key = 0) = 0;
};

class RiveDrawable {
public:
    virtual ~RiveDrawable();
    virtual void draw(rive::Renderer* renderer) = 0;
    void record_draw_commands();
    void replay_draw_commands(rive::Renderer* renderer) const;
    void clear_recorded_commands();

    void clear_renderer_state() {
        delete renderer_state;
        renderer_state = nullptr;
    }
    
    RendererState* renderer_state = nullptr;

private:
    std::shared_ptr<RiveCommandRecording> recorded_commands;
};

class RiveRenderRegistry {
    std::vector<RiveDrawable*> drawables;
    mutable std::mutex mutex;
    rive::Factory* factory = nullptr;

public:
    static RiveRenderRegistry* get_singleton();

    void set_factory(rive::Factory* p_factory);
    rive::Factory* get_factory() const;

    void add_drawable(RiveDrawable* drawable);
    void remove_drawable(RiveDrawable* drawable);
    
    void draw_all(rive::Renderer* renderer);
    void clear_renderer_states();
};

#endif
