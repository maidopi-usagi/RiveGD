#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive_render_registry.h"

#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using namespace godot;
using namespace rive;
using namespace rive::gpu;
using Microsoft::WRL::ComPtr;

namespace rive_integration {

static rive::gpu::RenderContext *g_rive_context = nullptr;
static ComPtr<ID3D12CommandQueue> g_command_queue;
static ComPtr<ID3D12Fence> g_fence;
static HANDLE g_fence_event = nullptr;
static uint64_t g_fence_value = 0;
static uint64_t g_frame_idx = 0;

static ComPtr<ID3D12Resource> g_intermediate_texture;
static uint32_t g_intermediate_width = 0;
static uint32_t g_intermediate_height = 0;

void ensure_intermediate_texture(ID3D12Device *device, uint32_t width, uint32_t height, DXGI_FORMAT format) {
	if (g_intermediate_texture && g_intermediate_width == width && g_intermediate_height == height) {
		return;
	}
	g_intermediate_texture.Reset();

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if (format == DXGI_FORMAT_R8G8B8A8_TYPELESS) {
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if (format == DXGI_FORMAT_B8G8R8A8_TYPELESS) {
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	}

	device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&g_intermediate_texture));

	g_intermediate_width = width;
	g_intermediate_height = height;
}

void transition_resource(ID3D12GraphicsCommandList *cmd_list, ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	if (before == after) {
		return;
	}
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd_list->ResourceBarrier(1, &barrier);
}

bool create_d3d12_context(RenderingDevice* rd) {
	if (!rd) return false;

	ID3D12Device* device = (ID3D12Device*)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
	if (!device) {
		UtilityFunctions::printerr("Rive: Failed to retrieve D3D12 device.");
		return false;
	}

	g_intermediate_texture.Reset();
	g_intermediate_width = 0;
	g_intermediate_height = 0;
	g_fence_value = 0;
	g_frame_idx = 2; // Start at 2 to match Fiddle logic, maybe unneccesory.

	// Temp command list for intialization
	ComPtr<ID3D12CommandAllocator> allocator;
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
	ComPtr<ID3D12GraphicsCommandList> command_list;
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&command_list));

	rive::gpu::D3DContextOptions options;
	
	// Detect Intel GPU
	bool is_intel = false;
	LUID luid = device->GetAdapterLuid();
	ComPtr<IDXGIFactory4> factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
		ComPtr<IDXGIAdapter> adapter;
		if (SUCCEEDED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))) {
			DXGI_ADAPTER_DESC desc;
			if (SUCCEEDED(adapter->GetDesc(&desc))) {
				if (desc.VendorId == 0x8086) {
					is_intel = true;
				}
			}
		}
	}
	options.isIntel = is_intel;

	std::unique_ptr<rive::gpu::RenderContext> ctx = rive::gpu::RenderContextD3D12Impl::MakeContext(
			ComPtr<ID3D12Device>(device),
			command_list.Get(),
			options);

	if (!ctx) {
		UtilityFunctions::printerr("Rive: Failed to create RenderContext");
		return false;
	}

	command_list->Close();

	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&g_command_queue)))) {
		UtilityFunctions::printerr("Rive: Failed to create command queue");
		return false;
	}

	ID3D12CommandList* ppCommandLists[] = { command_list.Get() };
	g_command_queue->ExecuteCommandLists(1, ppCommandLists);
	
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
		UtilityFunctions::printerr("Rive: Failed to create fence");
		return false;
	}
	g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	g_fence_value++;
	g_command_queue->Signal(g_fence.Get(), g_fence_value);
	if (g_fence->GetCompletedValue() < g_fence_value) {
		g_fence->SetEventOnCompletion(g_fence_value, g_fence_event);
		WaitForSingleObject(g_fence_event, INFINITE);
	}

	g_rive_context = ctx.release();
	RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);

	return true;
}

void render_texture_d3d12(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
	if (!g_rive_context || !g_command_queue) {
		return;
	}
	if (!rd || !drawable) {
		return;
	}
	if (width == 0 || height == 0) {
		return;
	}

	ID3D12Device *device = (ID3D12Device *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
	if (!device) {
		return;
	}

	// Create fresh allocator and list for this frame to avoid state issues
	ComPtr<ID3D12CommandAllocator> command_allocator;
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)))) {
		UtilityFunctions::printerr("Rive: Failed to create command allocator");
		return;
	}

	ComPtr<ID3D12GraphicsCommandList> command_list;
	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)))) {
		UtilityFunctions::printerr("Rive: Failed to create command list");
		return;
	}

	ID3D12Resource *image = (ID3D12Resource *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, texture_rid, 0);

	if (image) {
		D3D12_RESOURCE_DESC desc = image->GetDesc();
		bool needs_workaround = (desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
		ID3D12Resource *target_resource = image;

		if (needs_workaround) {
			ensure_intermediate_texture(device, width, height, desc.Format);
			if (g_intermediate_texture) {
				transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
				transition_resource(command_list.Get(), g_intermediate_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				command_list->CopyResource(g_intermediate_texture.Get(), image);
				transition_resource(command_list.Get(), g_intermediate_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
				transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				target_resource = g_intermediate_texture.Get();
			}
		} else {
			transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
		}

		rive::gpu::RenderContext::FrameDescriptor fd;
		fd.renderTargetWidth = width;
		fd.renderTargetHeight = height;
		fd.loadAction = rive::gpu::LoadAction::clear;
		fd.clearColor = 0x00000000;

		g_rive_context->beginFrame(fd);

		rive::gpu::RenderContextD3D12Impl *impl = g_rive_context->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
		rive::rcp<rive::gpu::RenderTargetD3D12> rtarget = impl->makeRenderTarget(width, height);

		if (rtarget) {
			rtarget->setTargetTexture(ComPtr<ID3D12Resource>(target_resource));

			{
				rive::RiveRenderer renderer(g_rive_context);
				drawable->draw(&renderer);
			}

			g_frame_idx++;

			rive::gpu::RenderContextD3D12Impl::CommandLists command_lists;
			command_lists.copyComandList = command_list.Get();
			command_lists.directComandList = command_list.Get();

			rive::gpu::RenderContext::FlushResources fr;
			fr.renderTarget = rtarget.get();
			fr.externalCommandBuffer = &command_lists;
			fr.currentFrameNumber = g_frame_idx;
			fr.safeFrameNumber = (g_frame_idx > 2) ? g_frame_idx - 2 : 0;

			g_rive_context->flush(fr);
		}

		if (needs_workaround && g_intermediate_texture) {
			transition_resource(command_list.Get(), g_intermediate_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
			transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			command_list->CopyResource(image, g_intermediate_texture.Get());
			transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			transition_resource(command_list.Get(), g_intermediate_texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
		} else {
			transition_resource(command_list.Get(), image, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	command_list->Close();

	ID3D12CommandList *ppCommandLists[] = { command_list.Get() };
	g_command_queue->ExecuteCommandLists(1, ppCommandLists);

	g_fence_value++;
	g_command_queue->Signal(g_fence.Get(), g_fence_value);

	if (g_fence->GetCompletedValue() < g_fence_value) {
		g_fence->SetEventOnCompletion(g_fence_value, g_fence_event);
		WaitForSingleObject(g_fence_event, INFINITE);
	}
}

} // namespace rive_integration
