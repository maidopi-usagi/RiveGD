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
#include <vector>

using namespace godot;
using namespace rive;
using namespace rive::gpu;
using Microsoft::WRL::ComPtr;

namespace rive_integration {

struct RiveD3D12FrameState {
	ComPtr<ID3D12CommandAllocator> command_allocator;
	ComPtr<ID3D12Resource> intermediate_texture;
	uint32_t intermediate_width = 0;
	uint32_t intermediate_height = 0;
	DXGI_FORMAT intermediate_format = DXGI_FORMAT_UNKNOWN;
	uint64_t fence_value = 0;
	uint64_t frame_number = 0;
};

static rive::gpu::RenderContext *g_rive_context = nullptr;
static ComPtr<ID3D12CommandQueue> g_command_queue;
static ComPtr<ID3D12Fence> g_fence;
static HANDLE g_fence_event = nullptr;
static uint64_t g_fence_value = 0;
static uint64_t g_frame_idx = 0;

static std::vector<RiveD3D12FrameState> g_frame_states;
static uint32_t g_current_frame_index = 0;
static const uint32_t kMaxFramesInFlight = 2;
static ComPtr<ID3D12GraphicsCommandList> g_command_list;

static constexpr DWORD kFenceWaitTimeoutMs = 30000;

void cleanup_d3d12_context();

static bool wait_for_fence(uint64_t fence_value, const char *operation) {
	if (!g_fence || !g_fence_event) {
		UtilityFunctions::printerr("Rive: Cannot wait for D3D12 fence while ", operation, ".");
		return false;
	}
	if (g_fence->GetCompletedValue() >= fence_value) {
		return true;
	}
	if (FAILED(g_fence->SetEventOnCompletion(fence_value, g_fence_event))) {
		UtilityFunctions::printerr("Rive: Failed to arm D3D12 fence while ", operation, ".");
		return false;
	}
	const DWORD wait_result = WaitForSingleObject(g_fence_event, kFenceWaitTimeoutMs);
	if (wait_result != WAIT_OBJECT_0) {
		UtilityFunctions::printerr("Rive: Timed out or failed waiting for D3D12 GPU while ", operation, ".");
		return false;
	}
	return true;
}

static bool ensure_intermediate_texture(RiveD3D12FrameState &frame_state, ID3D12Device *device, uint32_t width, uint32_t height, DXGI_FORMAT format) {
	if (frame_state.intermediate_texture && frame_state.intermediate_width == width &&
		frame_state.intermediate_height == height && frame_state.intermediate_format == format) {
		return true;
	}
	frame_state.intermediate_texture.Reset();
	frame_state.intermediate_width = 0;
	frame_state.intermediate_height = 0;
	frame_state.intermediate_format = DXGI_FORMAT_UNKNOWN;

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

	const HRESULT result = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&frame_state.intermediate_texture));
	if (FAILED(result) || !frame_state.intermediate_texture) {
		UtilityFunctions::printerr("Rive: Failed to create the D3D12 intermediate texture.");
		return false;
	}

	frame_state.intermediate_width = width;
	frame_state.intermediate_height = height;
	frame_state.intermediate_format = format;
	return true;
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
	cleanup_d3d12_context();
	auto fail = []() {
		cleanup_d3d12_context();
		return false;
	};

	ID3D12Device* device = (ID3D12Device*)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE, RID(), 0);
	if (!device) {
		UtilityFunctions::printerr("Rive: Failed to retrieve D3D12 device.");
		return false;
	}
	ID3D12CommandQueue *godot_queue = (ID3D12CommandQueue *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_COMMAND_QUEUE, RID(), 0);
	if (!godot_queue) {
		UtilityFunctions::printerr("Rive: Failed to retrieve Godot's D3D12 command queue.");
		return false;
	}
	g_command_queue = godot_queue;

	g_fence_value = 0;
	g_frame_idx = 0;
	g_current_frame_index = 0;

	g_frame_states.resize(kMaxFramesInFlight);
	for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frame_states[i].command_allocator)))) {
			UtilityFunctions::printerr("Rive: Failed to create command allocator for frame ", i);
			return fail();
		}
		g_frame_states[i].fence_value = 0;
		g_frame_states[i].frame_number = 0;
	}

	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frame_states[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(&g_command_list)))) {
		UtilityFunctions::printerr("Rive: Failed to create command list");
		return fail();
	}
	rive::gpu::D3DContextOptions options;
	
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
			g_command_list.Get(),
			options);

	if (!ctx) {
		UtilityFunctions::printerr("Rive: Failed to create RenderContext");
		return fail();
	}
	g_rive_context = ctx.release();

	if (FAILED(g_command_list->Close())) {
		UtilityFunctions::printerr("Rive: Failed to close the D3D12 initialization command list.");
		return fail();
	}

	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
		UtilityFunctions::printerr("Rive: Failed to create fence");
		return fail();
	}
	g_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!g_fence_event) {
		UtilityFunctions::printerr("Rive: Failed to create D3D12 fence event.");
		return fail();
	}

	ID3D12CommandList* ppCommandLists[] = { g_command_list.Get() };
	g_command_queue->ExecuteCommandLists(1, ppCommandLists);

	g_fence_value++;
	if (FAILED(g_command_queue->Signal(g_fence.Get(), g_fence_value))) {
		UtilityFunctions::printerr("Rive: Failed to signal the D3D12 initialization fence.");
		return fail();
	}
	if (!wait_for_fence(g_fence_value, "initializing the renderer")) {
		return fail();
	}

	RiveRenderRegistry::get_singleton()->set_factory(g_rive_context);

	return true;
}

void render_texture_d3d12(RenderingDevice *rd, RID texture_rid, RiveDrawable *drawable, uint32_t width, uint32_t height) {
	if (!g_rive_context || !g_command_queue || !g_fence || !g_fence_event || !g_command_list || g_frame_states.size() != kMaxFramesInFlight) {
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
	ID3D12Resource *image = (ID3D12Resource *)rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, texture_rid, 0);
	if (!image) {
		UtilityFunctions::printerr("Rive: Failed to retrieve the D3D12 target texture.");
		return;
	}

	RiveD3D12FrameState &frame_state = g_frame_states[g_current_frame_index];

	if (frame_state.fence_value != 0 && !wait_for_fence(frame_state.fence_value, "reusing a frame allocator")) {
		return;
	}

	if (FAILED(frame_state.command_allocator->Reset())) {
		UtilityFunctions::printerr("Rive: Failed to reset command allocator");
		return;
	}
	if (FAILED(g_command_list->Reset(frame_state.command_allocator.Get(), nullptr))) {
		UtilityFunctions::printerr("Rive: Failed to reset command list");
		return;
	}

	if (image) {
		D3D12_RESOURCE_DESC desc = image->GetDesc();
		bool needs_workaround = (desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
		ID3D12Resource *target_resource = image;

		if (needs_workaround) {
			if (!ensure_intermediate_texture(frame_state, device, width, height, desc.Format)) {
				g_command_list->Close();
				return;
			}
			if (frame_state.intermediate_texture) {
				transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
				transition_resource(g_command_list.Get(), frame_state.intermediate_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
				g_command_list->CopyResource(frame_state.intermediate_texture.Get(), image);
				transition_resource(g_command_list.Get(), frame_state.intermediate_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
				transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				target_resource = frame_state.intermediate_texture.Get();
			}
		} else {
			transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
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
			rtarget->setTargetTexture(ComPtr<ID3D12Resource>(target_resource)
#if defined(RIVE_GODOT_D3D12_EXTERNAL_STATE)
			                         , D3D12_RESOURCE_STATE_COMMON
#endif
			);

			{
				rive::RiveRenderer renderer(g_rive_context);
				drawable->replay_draw_commands(&renderer);
			}

			g_frame_idx++;

			rive::gpu::RenderContextD3D12Impl::CommandLists command_lists;
			command_lists.copyComandList = g_command_list.Get();
			command_lists.directComandList = g_command_list.Get();

			rive::gpu::RenderContext::FlushResources fr;
			fr.renderTarget = rtarget.get();
			fr.externalCommandBuffer = &command_lists;
			fr.currentFrameNumber = g_frame_idx;
			fr.safeFrameNumber = frame_state.frame_number;

			g_rive_context->flush(fr);
		}

		if (needs_workaround && frame_state.intermediate_texture) {
			transition_resource(g_command_list.Get(), frame_state.intermediate_texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
			transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			g_command_list->CopyResource(image, frame_state.intermediate_texture.Get());
			transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			transition_resource(g_command_list.Get(), frame_state.intermediate_texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
		} else {
			transition_resource(g_command_list.Get(), image, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	if (FAILED(g_command_list->Close())) {
		UtilityFunctions::printerr("Rive: Failed to close the D3D12 render command list.");
		return;
	}

	ID3D12CommandList *ppCommandLists[] = { g_command_list.Get() };
	g_command_queue->ExecuteCommandLists(1, ppCommandLists);

	g_fence_value++;
	if (FAILED(g_command_queue->Signal(g_fence.Get(), g_fence_value))) {
		UtilityFunctions::printerr("Rive: Failed to signal the D3D12 render fence.");
		return;
	}

	frame_state.fence_value = g_fence_value;
	frame_state.frame_number = g_frame_idx;

	g_current_frame_index = (g_current_frame_index + 1) % kMaxFramesInFlight;
}

void cleanup_d3d12_context() {
	if (g_command_queue && g_fence && g_fence_event) {
		g_fence_value++;
		if (SUCCEEDED(g_command_queue->Signal(g_fence.Get(), g_fence_value))) {
			wait_for_fence(g_fence_value, "shutting down the renderer");
		}
	}

	RiveRenderRegistry::get_singleton()->clear_renderer_states();
	RiveRenderRegistry::get_singleton()->set_factory(nullptr);
	if (g_rive_context) {
		rive::gpu::RenderContextD3D12Impl *impl =
			g_rive_context->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
		if (impl && impl->manager()) {
			impl->manager()->shutdown();
		}
		delete g_rive_context;
		g_rive_context = nullptr;
	}

	g_command_list.Reset();
	g_frame_states.clear();
	g_command_queue.Reset();
	g_fence.Reset();
	g_fence_value = 0;
	g_frame_idx = 0;
	g_current_frame_index = 0;

	if (g_fence_event) {
		CloseHandle(g_fence_event);
		g_fence_event = nullptr;
	}
}

}
