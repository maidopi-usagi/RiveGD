#include "rive_render_registry.h"
#include "rive_renderer.h"
#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include <algorithm>
#include <cstring>

enum class RiveRecordedCommandType {
    SAVE,
    RESTORE,
    TRANSFORM,
    DRAW_PATH,
    CLIP_PATH,
    CLIP_STROKE,
    DRAW_IMAGE,
    DRAW_IMAGE_MESH,
    DRAW_IMAGE_MESH_INSTANCED,
    APPLY_LAYER_MASK,
    MODULATE_OPACITY,
    MODULATE_COLOR,
};

struct RiveRecordedCommand {
    RiveRecordedCommandType type = RiveRecordedCommandType::SAVE;
    rive::Mat2D transform;
    rive::rcp<rive::RenderPath> path;
    rive::rcp<rive::RenderPaint> paint;
    rive::rcp<rive::RenderImage> image;
    rive::ImageSampler sampler = rive::ImageSampler::LinearClamp();
    rive::BlendMode blend_mode = rive::BlendMode::srcOver;
    float opacity = 1.0f;
    float additiveness = 0.0f;
    rive::StrokeParams stroke;
    rive::LayerMaskMode mask_mode = rive::LayerMaskMode::alpha;
    rive::ColorInt color = 0xffffffff;
    bool replace_color = false;
    rive::rcp<rive::RenderBuffer> vertices;
    rive::rcp<rive::RenderBuffer> uv_coords;
    rive::rcp<rive::RenderBuffer> indices;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    std::vector<float> vertex_data;
    std::vector<float> uv_data;
    std::vector<uint16_t> index_data;
    std::vector<rive::ImageMeshInstanceData> instance_data;
    rive::rcp<rive::ImageMeshInstances> instance_source;
};

class RiveCommandRecording : public rive::Renderer, public RiveCommandRecordingTarget {
    struct RenderState {
        rive::Mat2D transform;
        float opacity = 1.0f;
    };
    std::vector<RenderState> render_states{1};

public:
    std::vector<RiveRecordedCommand> commands;

    void save() override {
        render_states.push_back(render_states.back());
        commands.push_back({ RiveRecordedCommandType::SAVE });
    }
    void restore() override {
        if (render_states.size() <= 1) return;
        render_states.pop_back();
        commands.push_back({ RiveRecordedCommandType::RESTORE });
    }
    void transform(const rive::Mat2D &p_transform) override {
        render_states.back().transform *= p_transform;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::TRANSFORM;
        command.transform = p_transform;
        commands.push_back(std::move(command));
    }
    void drawPath(rive::RenderPath *p_path, rive::RenderPaint *p_paint) override {
        if (!p_path || !p_paint) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::DRAW_PATH;
        command.path = rive::ref_rcp(p_path);
        command.paint = rive::ref_rcp(p_paint);
        commands.push_back(std::move(command));
    }
    void clipPath(rive::RenderPath *p_path) override {
        if (!p_path) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::CLIP_PATH;
        command.path = rive::ref_rcp(p_path);
        commands.push_back(std::move(command));
    }
    void clipStroke(rive::RenderPath *p_path, const rive::StrokeParams &p_stroke) override {
        if (!p_path) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::CLIP_STROKE;
        command.path = rive::ref_rcp(p_path);
        command.stroke = p_stroke;
        commands.push_back(std::move(command));
    }
    void drawImage(const rive::RenderImage *p_image, rive::ImageSampler p_sampler,
                   rive::BlendMode p_blend_mode, float p_opacity) override {
        drawImage(p_image, p_sampler, p_blend_mode, p_opacity, 0.0f);
    }
    void drawImage(const rive::RenderImage *p_image, rive::ImageSampler p_sampler,
                   rive::BlendMode p_blend_mode, float p_opacity, float p_additiveness) override {
        if (!p_image) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::DRAW_IMAGE;
        command.image = rive::ref_rcp(const_cast<rive::RenderImage *>(p_image));
        command.sampler = p_sampler;
        command.blend_mode = p_blend_mode;
        command.opacity = p_opacity;
        command.additiveness = p_additiveness;
        commands.push_back(std::move(command));
    }
    void drawImageMesh(const rive::RenderImage *p_image, rive::ImageSampler p_sampler,
                       rive::rcp<rive::RenderBuffer> p_vertices,
                       rive::rcp<rive::RenderBuffer> p_uv_coords,
                       rive::rcp<rive::RenderBuffer> p_indices,
                       uint32_t p_vertex_count, uint32_t p_index_count,
                       rive::BlendMode p_blend_mode, float p_opacity) override {
        drawImageMesh(p_image, p_sampler, std::move(p_vertices), std::move(p_uv_coords),
                      std::move(p_indices), p_vertex_count, p_index_count,
                      p_blend_mode, p_opacity, 0.0f);
    }
    void drawImageMesh(const rive::RenderImage *p_image, rive::ImageSampler p_sampler,
                       rive::rcp<rive::RenderBuffer> p_vertices,
                       rive::rcp<rive::RenderBuffer> p_uv_coords,
                       rive::rcp<rive::RenderBuffer> p_indices,
                       uint32_t p_vertex_count, uint32_t p_index_count,
                       rive::BlendMode p_blend_mode, float p_opacity,
                       float p_additiveness) override {
        if (!p_image || !p_vertices || !p_uv_coords || !p_indices) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::DRAW_IMAGE_MESH;
        command.image = rive::ref_rcp(const_cast<rive::RenderImage *>(p_image));
        command.sampler = p_sampler;
        command.vertices = std::move(p_vertices);
        command.uv_coords = std::move(p_uv_coords);
        command.indices = std::move(p_indices);
        command.vertex_count = p_vertex_count;
        command.index_count = p_index_count;
        command.blend_mode = p_blend_mode;
        command.opacity = p_opacity;
        command.additiveness = p_additiveness;
        commands.push_back(std::move(command));
    }

    void drawImageMeshInstanced(const rive::RenderImage *p_image, rive::ImageSampler p_sampler,
                                rive::rcp<rive::RenderBuffer> p_vertices,
                                rive::rcp<rive::RenderBuffer> p_uv_coords,
                                rive::rcp<rive::RenderBuffer> p_indices,
                                uint32_t p_vertex_count, uint32_t p_index_count,
                                rive::rcp<rive::ImageMeshInstances> p_instances) override {
        if (!p_image || !p_vertices || !p_uv_coords || !p_indices || !p_instances ||
            p_instances->count() == 0) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::DRAW_IMAGE_MESH_INSTANCED;
        command.image = rive::ref_rcp(const_cast<rive::RenderImage *>(p_image));
        command.sampler = p_sampler;
        command.vertices = std::move(p_vertices);
        command.uv_coords = std::move(p_uv_coords);
        command.indices = std::move(p_indices);
        command.vertex_count = p_vertex_count;
        command.index_count = p_index_count;
        // Instances are edited in place by particle simulation. Copy the data
        // now so later edits cannot alter this recording's UVs or transforms.
        const auto data = p_instances->instanceData();
        command.instance_data.assign(data.begin(), data.end());
        // Keep any factory-specific resource wrapper alive until commands
        // are cleared on the render thread; replay uses the immutable copy.
        command.instance_source = std::move(p_instances);
        commands.push_back(std::move(command));
    }

    void applyLayerMask(const rive::RenderImage *p_mask, rive::ImageSampler p_sampler,
                        rive::LayerMaskMode p_mode) override {
        if (!p_mask) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::APPLY_LAYER_MASK;
        command.image = rive::ref_rcp(const_cast<rive::RenderImage *>(p_mask));
        command.sampler = p_sampler;
        command.mask_mode = p_mode;
        commands.push_back(std::move(command));
    }

    void modulateOpacity(float p_opacity) override {
        render_states.back().opacity = std::max(0.0f, render_states.back().opacity * p_opacity);
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::MODULATE_OPACITY;
        command.opacity = p_opacity;
        commands.push_back(std::move(command));
    }

    void modulateColor(rive::ColorInt p_color, bool p_replace = false) override {
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::MODULATE_COLOR;
        command.color = p_color;
        command.replace_color = p_replace;
        commands.push_back(std::move(command));
    }

    bool currentTransform(rive::Mat2D *p_out) const override {
        if (!p_out) return false;
        *p_out = render_states.back().transform;
        return true;
    }

    bool currentModulatedOpacity(float *p_out) const override {
        if (!p_out) return false;
        *p_out = render_states.back().opacity;
        return true;
    }

    void record_image_mesh(const rive::RenderImage *p_image,
                           const float *p_vertices,
                           const float *p_uv_coords,
                           const uint16_t *p_indices,
                           uint32_t p_vertex_count,
                           uint32_t p_index_count,
                           int p_blend_mode,
                           float p_opacity) override {
        if (!p_image || !p_vertices || !p_uv_coords || !p_indices ||
            p_vertex_count == 0 || p_index_count == 0) return;
        RiveRecordedCommand command;
        command.type = RiveRecordedCommandType::DRAW_IMAGE_MESH;
        command.image = rive::ref_rcp(const_cast<rive::RenderImage *>(p_image));
        command.sampler = rive::ImageSampler::LinearClamp();
        command.vertex_count = p_vertex_count;
        command.index_count = p_index_count;
        command.blend_mode = static_cast<rive::BlendMode>(p_blend_mode);
        command.opacity = p_opacity;
        command.vertex_data.assign(p_vertices, p_vertices + p_vertex_count * 2);
        command.uv_data.assign(p_uv_coords, p_uv_coords + p_vertex_count * 2);
        command.index_data.assign(p_indices, p_indices + p_index_count);
        commands.push_back(std::move(command));
    }
};

RiveDrawable::~RiveDrawable() {
    if (recorded_commands) {
        std::shared_ptr<rive::Renderer> old_recording = std::move(recorded_commands);
        auto release_old = [&]() { old_recording.reset(); };
        if (!rive_integration::run_on_rive_render_thread(release_old)) {
            release_old();
        }
    }
    delete renderer_state;
}

void RiveDrawable::record_draw_commands() {
    std::shared_ptr<RiveCommandRecording> recording = std::make_shared<RiveCommandRecording>();
    draw(recording.get());
    recorded_commands = std::move(recording);
}

void RiveDrawable::replay_draw_commands(rive::Renderer *renderer) const {
    if (!renderer || !recorded_commands) return;
    for (const RiveRecordedCommand &command : recorded_commands->commands) {
        switch (command.type) {
            case RiveRecordedCommandType::SAVE: renderer->save(); break;
            case RiveRecordedCommandType::RESTORE: renderer->restore(); break;
            case RiveRecordedCommandType::TRANSFORM: renderer->transform(command.transform); break;
            case RiveRecordedCommandType::DRAW_PATH: renderer->drawPath(command.path.get(), command.paint.get()); break;
            case RiveRecordedCommandType::CLIP_PATH: renderer->clipPath(command.path.get()); break;
            case RiveRecordedCommandType::CLIP_STROKE:
                renderer->clipStroke(command.path.get(), command.stroke);
                break;
            case RiveRecordedCommandType::DRAW_IMAGE:
                renderer->drawImage(command.image.get(), command.sampler, command.blend_mode,
                                    command.opacity, command.additiveness);
                break;
            case RiveRecordedCommandType::DRAW_IMAGE_MESH:
                if (command.vertices && command.uv_coords && command.indices) {
                    renderer->drawImageMesh(command.image.get(), command.sampler, command.vertices,
                                            command.uv_coords, command.indices, command.vertex_count,
                                            command.index_count, command.blend_mode, command.opacity,
                                            command.additiveness);
                    break;
                }
                if (rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory()) {
                    const size_t vertex_bytes = command.vertex_data.size() * sizeof(float);
                    const size_t index_bytes = command.index_data.size() * sizeof(uint16_t);
                    rive::rcp<rive::RenderBuffer> vertices = factory->makeRenderBuffer(
                        rive::RenderBufferType::vertex,
                        rive::RenderBufferFlags::mappedOnceAtInitialization,
                        vertex_bytes);
                    rive::rcp<rive::RenderBuffer> uv_coords = factory->makeRenderBuffer(
                        rive::RenderBufferType::vertex,
                        rive::RenderBufferFlags::mappedOnceAtInitialization,
                        vertex_bytes);
                    rive::rcp<rive::RenderBuffer> indices = factory->makeRenderBuffer(
                        rive::RenderBufferType::index,
                        rive::RenderBufferFlags::mappedOnceAtInitialization,
                        index_bytes);
                    if (!vertices || !uv_coords || !indices) break;

                    void *vertex_map = vertices->map();
                    void *uv_map = uv_coords->map();
                    void *index_map = indices->map();
                    if (!vertex_map || !uv_map || !index_map) {
                        if (vertex_map) vertices->unmap();
                        if (uv_map) uv_coords->unmap();
                        if (index_map) indices->unmap();
                        break;
                    }
                    memcpy(vertex_map, command.vertex_data.data(), vertex_bytes);
                    memcpy(uv_map, command.uv_data.data(), vertex_bytes);
                    memcpy(index_map, command.index_data.data(), index_bytes);
                    vertices->unmap();
                    uv_coords->unmap();
                    indices->unmap();
                    renderer->drawImageMesh(command.image.get(), command.sampler,
                                            std::move(vertices), std::move(uv_coords), std::move(indices),
                                            command.vertex_count, command.index_count,
                                            command.blend_mode, command.opacity, command.additiveness);
                }
                break;
            case RiveRecordedCommandType::DRAW_IMAGE_MESH_INSTANCED:
                if (rive::Factory *factory = RiveRenderRegistry::get_singleton()->get_factory()) {
                    rive::rcp<rive::ImageMeshInstances> instances =
                        factory->makeImageMeshInstances(command.instance_data.size());
                    if (!instances) break;
                    auto data = instances->edit();
                    std::copy(command.instance_data.begin(), command.instance_data.end(), data.begin());
                    instances->endEdit();
                    renderer->drawImageMeshInstanced(command.image.get(), command.sampler,
                                                     command.vertices, command.uv_coords, command.indices,
                                                     command.vertex_count, command.index_count,
                                                     std::move(instances));
                }
                break;
            case RiveRecordedCommandType::APPLY_LAYER_MASK:
                renderer->applyLayerMask(command.image.get(), command.sampler, command.mask_mode);
                break;
            case RiveRecordedCommandType::MODULATE_OPACITY:
                renderer->modulateOpacity(command.opacity);
                break;
            case RiveRecordedCommandType::MODULATE_COLOR:
                renderer->modulateColor(command.color, command.replace_color);
                break;
        }
    }
}

void RiveDrawable::clear_recorded_commands() {
    recorded_commands.reset();
}

RiveRenderRegistry* RiveRenderRegistry::get_singleton() {
    static RiveRenderRegistry singleton;
    return &singleton;
}

void RiveRenderRegistry::set_factory(rive::Factory* p_factory) {
    std::lock_guard<std::mutex> lock(mutex);
    factory = p_factory;
}

rive::Factory* RiveRenderRegistry::get_factory() const {
    std::lock_guard<std::mutex> lock(mutex);
    return factory;
}

void RiveRenderRegistry::add_drawable(RiveDrawable* drawable) {
    std::lock_guard<std::mutex> lock(mutex);
    if (drawable && std::find(drawables.begin(), drawables.end(), drawable) == drawables.end()) {
        drawables.push_back(drawable);
    }
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

void RiveRenderRegistry::clear_renderer_states() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto* drawable : drawables) {
        if (drawable) {
            drawable->clear_recorded_commands();
            drawable->clear_renderer_state();
        }
    }
}
