#include "rive_texture_factory.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef RIVE_DESKTOP_GL
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#endif

using namespace godot;

#if defined(__APPLE__)
// Forward declaration for Metal implementation
rive::rcp<rive::RenderImage> RiveTextureFactoryMetal_make_image(godot::Ref<godot::Texture2D> texture);
#endif

namespace rive_integration {

rive::rcp<rive::RenderImage> RiveTextureFactory::make_image(Ref<Texture2D> texture) {
    if (texture.is_null()) return nullptr;

    auto registry = RiveRenderRegistry::get_singleton();
    if (!registry) return nullptr;
    
    rive::Factory* factory = registry->get_factory();
    if (!factory) return nullptr;

    RenderingServer* rs = RenderingServer::get_singleton();
    String api = rs->get_current_rendering_driver_name();

#if defined(__APPLE__)
    if (api == "metal") {
        return RiveTextureFactoryMetal_make_image(texture);
    }
#endif

    int width = texture->get_width();
    int height = texture->get_height();

#ifdef RIVE_DESKTOP_GL
    if (api == "opengl3") {
        // We need to cast factory to RenderContext.
        // Since we know we are using Rive Renderer, the factory IS the RenderContext.
        rive::gpu::RenderContext* ctx = static_cast<rive::gpu::RenderContext*>(factory);
        
        if (ctx) {
            rive::gpu::RenderContextGLImpl* gl_ctx = ctx->static_impl_cast<rive::gpu::RenderContextGLImpl>();
            if (gl_ctx) {
                RID rid = texture->get_rid();
                int64_t native_handle = rs->texture_get_native_handle(rid);
                GLuint source_texture_id = (GLuint)native_handle;
                
                if (source_texture_id != 0) {
                    // Create a new texture that Rive can own and delete
                    GLuint new_texture_id = 0;
                    glGenTextures(1, &new_texture_id);
                    
                    if (new_texture_id != 0) {
                        GLint prev_texture = 0;
                        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);
                        
                        glBindTexture(GL_TEXTURE_2D, new_texture_id);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        
                        // Save FBO state
                        GLint prev_read_fbo = 0;
                        GLint prev_draw_fbo = 0;
                        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
                        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
                        
                        // Create temporary FBOs for blitting
                        GLuint fbos[2];
                        glGenFramebuffers(2, fbos);
                        
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0]);
                        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, source_texture_id, 0);
                        
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1]);
                        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, new_texture_id, 0);
                        
                        GLenum status_read = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
                        GLenum status_draw = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
                        
                        bool blit_success = false;
                        if (status_read == GL_FRAMEBUFFER_COMPLETE && status_draw == GL_FRAMEBUFFER_COMPLETE) {
                            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                            blit_success = true;
                        }
                        
                        // Cleanup
                        glDeleteFramebuffers(2, fbos);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fbo);
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw_fbo);
                        glBindTexture(GL_TEXTURE_2D, prev_texture);
                        
                        if (blit_success) {
                            auto gpu_texture = gl_ctx->adoptImageTexture(width, height, new_texture_id);
                            if (gpu_texture) {
                                return rive::make_rcp<rive::RiveRenderImage>(std::move(gpu_texture));
                            }
                        } else {
                            // If blit failed, delete the texture we created
                            glDeleteTextures(1, &new_texture_id);
                        }
                    }
                }
            }
        }
    }
#endif

    // Fallback to PNG encoding/decoding
    Ref<Image> img = texture->get_image();
    if (img.is_valid()) {
        PackedByteArray png_data = img->save_png_to_buffer();
        if (!png_data.is_empty()) {
            rive::Span<const uint8_t> bytes(png_data.ptr(), png_data.size());
            return factory->decodeImage(bytes);
        }
    }

    return nullptr;
}

}
