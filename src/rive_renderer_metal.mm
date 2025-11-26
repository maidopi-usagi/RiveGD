#include "rive_renderer.h"
#include "rive_render_registry.h"
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#if defined(__APPLE__)

#import <Metal/Metal.h>
#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"

using namespace godot;
using namespace rive;

namespace rive_integration {

static rive::gpu::RenderContext *g_rive_context = nullptr;

class MetalRendererState : public RendererState {
public:
    rive::rcp<rive::gpu::RenderTargetMetal> renderTarget;
    uint32_t width = 0;
    uint32_t height = 0;
    MTLPixelFormat pixelFormat = MTLPixelFormatInvalid;
};

bool create_metal_context(RenderingDevice* rd) {
    if (!rd) return false;
    
    void* device_ptr = (void*)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
    if (!device_ptr) return false;
    
    id<MTLDevice> device = (__bridge id<MTLDevice>)(device_ptr);

    rive::gpu::RenderContextMetalImpl::ContextOptions options;
    std::unique_ptr<rive::gpu::RenderContext> ctx = rive::gpu::RenderContextMetalImpl::MakeContext(device, options);
    if (!ctx) return false;

    if (g_rive_context) delete g_rive_context;
    g_rive_context = ctx.release();

    RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);
    UtilityFunctions::print_verbose("RIVE: create_metal_context succeeded");
    return true;
}

void render_texture_metal(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
    if (!g_rive_context || !rd || !drawable) return;
    
    @autoreleasepool {
        void* queue_ptr = (void*)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
        if (!queue_ptr) return;
        
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)(queue_ptr);
        id<MTLCommandBuffer> cmd_buffer = [queue commandBuffer];
        
        if (!cmd_buffer) return;
        
        void* texture_ptr = (void*)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, texture_rid, 0);
        if (!texture_ptr) return;
        
        id<MTLTexture> texture = (__bridge id<MTLTexture>)(texture_ptr);
        
        rive::gpu::RenderContext::FrameDescriptor fd;
        fd.renderTargetWidth = width;
        fd.renderTargetHeight = height;
        fd.loadAction = rive::gpu::LoadAction::clear;
        fd.clearColor = 0x00000000;

        g_rive_context->beginFrame(fd);
        
        MetalRendererState* state = static_cast<MetalRendererState*>(drawable->renderer_state);
        if (!state) {
            state = new MetalRendererState();
            drawable->renderer_state = state;
        }

        rive::gpu::RenderContextMetalImpl *impl = g_rive_context->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
        
        if (!state->renderTarget || state->width != width || state->height != height || state->pixelFormat != texture.pixelFormat) {
             state->renderTarget = impl->makeRenderTarget(texture.pixelFormat, width, height);
             state->width = width;
             state->height = height;
             state->pixelFormat = texture.pixelFormat;
        }
        
        rive::rcp<rive::gpu::RenderTargetMetal> rtarget = state->renderTarget;
        
        if (rtarget) {
            rtarget->setTargetTexture(texture);
            
            rive::gpu::RenderContext::FlushResources fr;
            fr.renderTarget = rtarget.get();
            fr.externalCommandBuffer = (__bridge void*)cmd_buffer;
            
            static uint64_t frame_idx = 0;
            frame_idx++;
            fr.currentFrameNumber = frame_idx;
            fr.safeFrameNumber = (frame_idx > 2) ? frame_idx - 2 : 0;
            
            {
                rive::RiveRenderer renderer(g_rive_context);
                drawable->draw(&renderer);
            }
            
            g_rive_context->flush(fr);
        }
        
        [cmd_buffer commit];
        [cmd_buffer waitUntilCompleted];
    }
}

}
#endif
