#include "rive_types.h"
#include "../renderer/rive_renderer.h"
#include <cstring>
#include <cmath>
#include <algorithm>

void RiveMesh::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_geometry", "vertices", "uvs", "indices"), &RiveMesh::set_geometry);
    ClassDB::bind_method(D_METHOD("get_vertex_count"), &RiveMesh::get_vertex_count);
    ClassDB::bind_method(D_METHOD("get_index_count"), &RiveMesh::get_index_count);
}

RiveMesh::~RiveMesh() {
    auto release = [&]() {
        vertex_buffer.reset();
        uv_buffer.reset();
        index_buffer.reset();
    };
    if (!rive_integration::run_on_rive_render_thread(release)) release();
}

bool RiveMesh::set_geometry(PackedVector2Array p_vertices, PackedVector2Array p_uvs, PackedInt32Array p_indices) {
    if (p_vertices.is_empty() || p_vertices.size() > 65535 || p_uvs.size() != p_vertices.size() ||
            p_indices.is_empty() || p_indices.size() % 3 != 0 || p_indices.size() > 3 * 1024 * 1024) return false;
    for (int64_t i = 0; i < p_vertices.size(); ++i) {
        if (!p_vertices[i].is_finite() || !p_uvs[i].is_finite()) return false;
    }
    for (int64_t i = 0; i < p_indices.size(); ++i) {
        if (p_indices[i] < 0 || p_indices[i] >= p_vertices.size()) return false;
    }
    vertices = p_vertices;
    uvs = p_uvs;
    indices = p_indices;
    dirty = true;
    return true;
}

bool RiveMesh::ensure_buffers(rive::Factory *factory) {
    if (!dirty) return vertex_buffer && uv_buffer && index_buffer;
    if (!factory || vertices.is_empty()) return false;
    const size_t vertex_bytes = vertices.size() * 2 * sizeof(float);
    auto new_vertex_buffer = factory->makeRenderBuffer(rive::RenderBufferType::vertex, rive::RenderBufferFlags::mappedOnceAtInitialization, vertex_bytes);
    auto new_uv_buffer = factory->makeRenderBuffer(rive::RenderBufferType::vertex, rive::RenderBufferFlags::mappedOnceAtInitialization, vertex_bytes);
    auto new_index_buffer = factory->makeRenderBuffer(rive::RenderBufferType::index, rive::RenderBufferFlags::mappedOnceAtInitialization, indices.size() * sizeof(uint16_t));
    if (!new_vertex_buffer || !new_uv_buffer || !new_index_buffer) return false;
    auto *mapped_vertices = static_cast<float *>(new_vertex_buffer->map());
    auto *mapped_uvs = static_cast<float *>(new_uv_buffer->map());
    auto *mapped_indices = static_cast<uint16_t *>(new_index_buffer->map());
    if (mapped_vertices && mapped_uvs && mapped_indices) {
        for (int64_t i = 0; i < vertices.size(); ++i) {
            mapped_vertices[i * 2] = vertices[i].x;
            mapped_vertices[i * 2 + 1] = vertices[i].y;
            mapped_uvs[i * 2] = uvs[i].x;
            mapped_uvs[i * 2 + 1] = uvs[i].y;
        }
        for (int64_t i = 0; i < indices.size(); ++i) mapped_indices[i] = indices[i];
    }
    if (mapped_vertices) new_vertex_buffer->unmap();
    if (mapped_uvs) new_uv_buffer->unmap();
    if (mapped_indices) new_index_buffer->unmap();
    if (!mapped_vertices || !mapped_uvs || !mapped_indices) return false;
    vertex_buffer = std::move(new_vertex_buffer);
    uv_buffer = std::move(new_uv_buffer);
    index_buffer = std::move(new_index_buffer);
    dirty = false;
    return true;
}

int RiveRendererWrapper::make_image_sampler(int filter, int wrap_x, int wrap_y) {
    if (filter < FILTER_LINEAR || filter > FILTER_NEAREST || wrap_x < WRAP_CLAMP || wrap_x > WRAP_MIRROR ||
            wrap_y < WRAP_CLAMP || wrap_y > WRAP_MIRROR) return -1;
    return filter * 9 + wrap_y * 3 + wrap_x;
}

bool RiveRendererWrapper::draw_image_mesh_instanced(Ref<RiveImage> image, Ref<RiveMesh> mesh,
        Array transforms, PackedFloat32Array opacities, int sampler_key) {
    if (!renderer || image.is_null() || !image->is_loaded() || mesh.is_null() || transforms.is_empty() || transforms.size() > 65536 ||
            (!opacities.is_empty() && opacities.size() != transforms.size()) || sampler_key < 0 || sampler_key > 17) return false;
    std::vector<rive::ImageMeshInstanceData> instance_data(transforms.size());
    for (int64_t i = 0; i < transforms.size(); ++i) {
        if (transforms[i].get_type() != Variant::TRANSFORM2D) return false;
        const Transform2D transform = transforms[i];
        if (!transform.is_finite()) return false;
        const float opacity = opacities.is_empty() ? 1.0f : opacities[i];
        if (!std::isfinite(opacity) || opacity < 0 || opacity > 1) return false;
        instance_data[i].transform = rive::Mat2D(transform[0].x, transform[0].y,
                transform[1].x, transform[1].y, transform[2].x, transform[2].y);
        instance_data[i].opacity = opacity;
    }
    bool drawn = false;
    rive_integration::run_on_rive_render_thread([&]() {
        auto *factory = RiveRenderRegistry::get_singleton()->get_factory();
        if (!mesh->ensure_buffers(factory)) return;
        auto instances = factory->makeImageMeshInstances(instance_data.size());
        if (!instances) return;
        auto target = instances->edit();
        std::copy(instance_data.begin(), instance_data.end(), target.begin());
        instances->endEdit();
        renderer->drawImageMeshInstanced(image->get_render_image(), rive::ImageSampler::SamplerFromKey(sampler_key),
            mesh->vertex_buffer, mesh->uv_buffer, mesh->index_buffer,
            mesh->vertices.size(), mesh->indices.size(), std::move(instances));
        drawn = true;
    });
    return drawn;
}
