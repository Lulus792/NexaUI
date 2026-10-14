#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4115) /* Windows SDK forward declaration in d3dcompiler.h. */
#endif
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <dxgi1_4.h>

#include "renderer.h"
#include "window_internal.h"

#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NX_D3D12_BUFFER_COUNT 3u
#define NX_D3D12_TEXTURE_POOL_MAX 256u
#define NX_D3D12_TEXTURE_POOL_BYTES (64u * 1024u * 1024u)
#define NX_D3D12_OPAQUE_BATCH_MAX 64u
#define NX_D3D12_TIMESTAMPS_PER_FRAME 3u

typedef struct nx_d3d12_bitmap_gpu {
    nx_bitmap_view source;
    ID3D12Resource *texture;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    int pooled;
    int needs_copy_transition;
} nx_d3d12_bitmap_gpu;

typedef struct nx_d3d12_texture_pool_entry {
    ID3D12Resource *texture;
    UINT width, height;
    DXGI_FORMAT format;
    UINT64 bytes;
    int used;
    int shader_state;
} nx_d3d12_texture_pool_entry;

struct nx_d3d12_renderer {
    HWND hwnd;
    DWORD owner_thread;
    IDXGIFactory4 *factory;
    ID3D12Device *device;
    ID3D12CommandQueue *queue;
    IDXGISwapChain3 *swapchain;
    ID3D12DescriptorHeap *rtv_heap;
    ID3D12Resource *targets[NX_D3D12_BUFFER_COUNT];
    ID3D12CommandAllocator *allocators[NX_D3D12_BUFFER_COUNT];
    ID3D12GraphicsCommandList *list;
    ID3D12Fence *fence;
    ID3D12RootSignature *root_signature;
    ID3D12PipelineState *pipeline;
    ID3D12RootSignature *composite_root;
    ID3D12PipelineState *composite_pipeline;
    ID3D12RootSignature *shape_root;
    ID3D12PipelineState *shape_pipeline;
    ID3D12DescriptorHeap *srv_heap;
    ID3D12Resource *copy_texture;
    ID3D12RootSignature *bitmap_root;
    ID3D12PipelineState *bitmap_pipeline;
    ID3D12Resource *bitmap_upload;
    UINT64 bitmap_upload_capacity;
    ID3D12Resource *vertex_ring[NX_D3D12_BUFFER_COUNT];
    UINT64 vertex_ring_capacity[NX_D3D12_BUFFER_COUNT];
    UINT64 vertex_ring_head[NX_D3D12_BUFFER_COUNT];
    UINT64 vertex_ring_allocations, vertex_ring_wraps;
    UINT64 frame_fence[NX_D3D12_BUFFER_COUNT];
    UINT last_frame;
    ID3D12QueryHeap *timestamp_heap;
    ID3D12Resource *timestamp_readback[NX_D3D12_BUFFER_COUNT];
    UINT64 timestamp_frequency;
    UINT64 timestamp_frame[NX_D3D12_BUFFER_COUNT];
    UINT64 timestamp_sequence;
    UINT64 last_timed_frame;
    double last_gpu_upload_ms, last_gpu_render_ms, last_gpu_frame_ms;
    ID3D12DescriptorHeap *bitmap_srv_heap;
    UINT bitmap_srv_capacity;
    nx_d3d12_texture_pool_entry texture_pool[NX_D3D12_TEXTURE_POOL_MAX];
    size_t texture_pool_count;
    UINT64 texture_pool_bytes;
    nx_d3d12_bitmap_gpu *pending_bitmaps;
    size_t pending_bitmap_count;
    ID3D12Resource *pending_readback;
    HANDLE fence_event;
    UINT64 fence_value;
    UINT rtv_stride;
    UINT width, height;
    UINT64 submitted_frames;
    UINT64 recording_draw_calls, last_draw_calls, last_bitmap_uploads;
    UINT64 recording_copy_texels, last_copy_texels;
    UINT64 texture_creations;
    int using_warp;
    int device_lost;
    int work_pending;
#ifdef NEXAUI_D3D12_TEST_HOOKS
    int test_force_warp;
    int test_fail_next_rebuild;
#endif
};

static int nx_d3d12_valid_color(nx_color c) {
    return isfinite(c.r) && isfinite(c.g) && isfinite(c.b) &&
           isfinite(c.a) && c.r >= 0 && c.r <= 1 && c.g >= 0 &&
           c.g <= 1 && c.b >= 0 && c.b <= 1 && c.a >= 0 && c.a <= 1;
}

static nx_result nx_d3d12_failure(nx_d3d12_renderer *renderer, HRESULT hr) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
        hr == DXGI_ERROR_DEVICE_HUNG ||
        hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR ||
        (renderer->device && FAILED(ID3D12Device_GetDeviceRemovedReason(
            renderer->device)))) {
        renderer->device_lost = 1;
        return NX_ERROR_STATE;
    }
    return NX_ERROR_PLATFORM;
}

typedef struct nx_d3d12_vertex {
    float x, y;
    float rgba[4];
} nx_d3d12_vertex;

static const char nx_d3d12_shader[] =
    "struct V { float2 pos : POSITION; float4 color : COLOR; };\n"
    "struct P { float4 pos : SV_POSITION; float4 color : COLOR; };\n"
    "P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1);"
    " p.color = v.color; return p; }\n"
    "float4 PSMain(P p) : SV_TARGET { return p.color; }\n";

static const char nx_d3d12_composite_shader[] =
    "Texture2D<float4> previous : register(t0);\n"
    "struct V { float2 pos : POSITION; float4 color : COLOR; };\n"
    "struct P { float4 pos : SV_POSITION; float4 color : COLOR; };\n"
    "P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1);"
    " p.color = v.color; return p; }\n"
    "float4 PSMain(P p) : SV_TARGET {\n"
    "  uint4 s = (uint4)round(saturate(p.color) * 255.0);\n"
    "  uint4 d = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);\n"
    "  if (s.a == 0) return float4(d) / 255.0;\n"
    "  if (s.a == 255 || d.a == 0) return float4(s) / 255.0;\n"
    "  uint inv = 255 - s.a;\n"
    "  uint denom = s.a * 255 + d.a * inv;\n"
    "  uint3 rgb = (s.rgb * s.a * 255 + d.rgb * d.a * inv + denom / 2) / denom;\n"
    "  uint alpha = (denom + 127) / 255;\n"
    "  return float4(rgb, alpha) / 255.0;\n"
    "}\n";

static const char nx_d3d12_shape_shader[] =
    "Texture2D<float4> previous : register(t0);\n"
    "cbuffer Shape : register(b0) { float4 bounds; float radius; uint kind; };\n"
    "struct V { float2 pos : POSITION; float4 color : COLOR; };\n"
    "struct P { float4 pos : SV_POSITION; float4 color : COLOR; };\n"
    "P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1);"
    " p.color = v.color; return p; }\n"
    "float4 PSMain(P p) : SV_TARGET {\n"
    "  float2 diff;\n"
    "  if (kind == 0) {\n"
    "    float2 a = bounds.xy, d = bounds.zw - a;\n"
    "    float len2 = dot(d, d);\n"
    "    float along = len2 > 0 ? saturate(dot(p.pos.xy - a, d) / len2) : 0;\n"
    "    diff = p.pos.xy - (a + along * d);\n"
    "  } else {\n"
    "    float2 nearest = clamp(p.pos.xy, bounds.xy + radius, bounds.zw - radius);\n"
    "    diff = p.pos.xy - nearest;\n"
    "  }\n"
    "  if (dot(diff, diff) > radius * radius) discard;\n"
    "  uint4 s = (uint4)round(saturate(p.color) * 255.0);\n"
    "  uint4 old = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);\n"
    "  if (s.a == 0) return float4(old) / 255.0;\n"
    "  if (s.a == 255 || old.a == 0) return float4(s) / 255.0;\n"
    "  uint inv = 255 - s.a;\n"
    "  uint denom = s.a * 255 + old.a * inv;\n"
    "  uint3 rgb = (s.rgb * s.a * 255 + old.rgb * old.a * inv + denom / 2) / denom;\n"
    "  uint alpha = (denom + 127) / 255;\n"
    "  return float4(rgb, alpha) / 255.0;\n"
    "}\n";

static const char nx_d3d12_bitmap_shader[] =
    "Texture2D<float4> previous : register(t0);\n"
    "Texture2D<float4> source : register(t1);\n"
    "cbuffer Bitmap : register(b0) { float4 bounds; uint source_format; };\n"
    "struct V { float2 pos : POSITION; float4 color : COLOR; };\n"
    "struct P { float4 pos : SV_POSITION; float4 color : COLOR; };\n"
    "P VSMain(V v) { P p; p.pos = float4(v.pos, 0, 1);"
    " p.color = v.color; return p; }\n"
    "float4 PSMain(P p) : SV_TARGET {\n"
    "  uint width, height; source.GetDimensions(width, height);\n"
    "  float2 uv = (p.pos.xy - bounds.xy) / bounds.zw;\n"
    "  uint2 xy = min((uint2)max(floor(uv * float2(width, height)), 0.0),"
    " uint2(width - 1, height - 1));\n"
    "  uint4 sample = (uint4)round(saturate(source.Load(int3(xy, 0))) * 255.0);\n"
    "  uint4 tint = (uint4)round(saturate(p.color) * 255.0);\n"
    "  uint4 s;\n"
    "  if (source_format == 2) { s.rgb = tint.rgb;"
    " s.a = (sample.r * tint.a + 127) / 255; }\n"
    "  else { s = (sample * tint + 127) / 255; }\n"
    "  uint4 d = (uint4)round(saturate(previous.Load(int3(int2(p.pos.xy), 0))) * 255.0);\n"
    "  if (s.a == 0) return float4(d) / 255.0;\n"
    "  if (s.a == 255 || d.a == 0) return float4(s) / 255.0;\n"
    "  uint inv = 255 - s.a;\n"
    "  uint denom = s.a * 255 + d.a * inv;\n"
    "  uint3 rgb = (s.rgb * s.a * 255 + d.rgb * d.a * inv + denom / 2) / denom;\n"
    "  uint alpha = (denom + 127) / 255;\n"
    "  return float4(rgb, alpha) / 255.0;\n"
    "}\n";

static nx_result nx_d3d12_ensure_pipeline(nx_d3d12_renderer *r) {
    if (r->pipeline) return NX_OK;
    ID3DBlob *signature = NULL, *vs = NULL, *ps = NULL, *errors = NULL;
    ID3D12RootSignature *root = NULL;
    ID3D12PipelineState *pipeline = NULL;
    D3D12_ROOT_SIGNATURE_DESC root_desc = {0};
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    HRESULT hr = D3D12SerializeRootSignature(&root_desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
    if (FAILED(hr)) goto fail;
    hr = ID3D12Device_CreateRootSignature(r->device, 0,
        ID3D10Blob_GetBufferPointer(signature),
        ID3D10Blob_GetBufferSize(signature),
        &IID_ID3D12RootSignature, (void **)&root);
    if (FAILED(hr)) goto fail;
    hr = D3DCompile(nx_d3d12_shader, sizeof(nx_d3d12_shader) - 1u,
        "nexaui-d3d12", NULL, NULL, "VSMain", "vs_5_0", 0, 0,
        &vs, &errors);
    if (FAILED(hr)) goto fail;
    if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
    hr = D3DCompile(nx_d3d12_shader, sizeof(nx_d3d12_shader) - 1u,
        "nexaui-d3d12", NULL, NULL, "PSMain", "ps_5_0", 0, 0,
        &ps, &errors);
    if (FAILED(hr)) goto fail;
    D3D12_INPUT_ELEMENT_DESC layout[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {0};
    desc.pRootSignature = root;
    desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
    desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
    desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
    desc.InputLayout.pInputElementDescs = layout;
    desc.InputLayout.NumElements = 2;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC *blend = &desc.BlendState.RenderTarget[0];
    blend->BlendEnable = TRUE;
    blend->SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend->DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend->BlendOp = D3D12_BLEND_OP_ADD;
    blend->SrcBlendAlpha = D3D12_BLEND_ONE;
    blend->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    hr = ID3D12Device_CreateGraphicsPipelineState(r->device, &desc,
        &IID_ID3D12PipelineState, (void **)&pipeline);
    if (FAILED(hr)) goto fail;
    r->root_signature = root;
    r->pipeline = pipeline;
    root = NULL;
    pipeline = NULL;
fail:
    if (errors) ID3D10Blob_Release(errors);
    if (ps) ID3D10Blob_Release(ps);
    if (vs) ID3D10Blob_Release(vs);
    if (signature) ID3D10Blob_Release(signature);
    if (pipeline) ID3D12PipelineState_Release(pipeline);
    if (root) ID3D12RootSignature_Release(root);
    return FAILED(hr) ? nx_d3d12_failure(r, hr) : NX_OK;
}

static nx_result nx_d3d12_ensure_composite(nx_d3d12_renderer *r,
                                             UINT frame) {
    if (!r->composite_pipeline) {
        ID3DBlob *signature = NULL, *vs = NULL, *ps = NULL, *errors = NULL;
        ID3D12RootSignature *root = NULL;
        ID3D12PipelineState *pipeline = NULL;
        ID3D12DescriptorHeap *srv_heap = NULL;
        D3D12_DESCRIPTOR_RANGE range = {0};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER parameter = {0};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC root_desc = {0};
        root_desc.NumParameters = 1;
        root_desc.pParameters = &parameter;
        root_desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        HRESULT hr = D3D12SerializeRootSignature(&root_desc,
            D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr)) goto fail;
        hr = ID3D12Device_CreateRootSignature(r->device, 0,
            ID3D10Blob_GetBufferPointer(signature),
            ID3D10Blob_GetBufferSize(signature),
            &IID_ID3D12RootSignature, (void **)&root);
        if (FAILED(hr)) goto fail;
        hr = D3DCompile(nx_d3d12_composite_shader,
            sizeof(nx_d3d12_composite_shader) - 1u, "nexaui-composite",
            NULL, NULL, "VSMain", "vs_5_0", 0, 0, &vs, &errors);
        if (FAILED(hr)) goto fail;
        if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
        hr = D3DCompile(nx_d3d12_composite_shader,
            sizeof(nx_d3d12_composite_shader) - 1u, "nexaui-composite",
            NULL, NULL, "PSMain", "ps_5_0", 0, 0, &ps, &errors);
        if (FAILED(hr)) goto fail;
        D3D12_INPUT_ELEMENT_DESC layout[2] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {0};
        desc.pRootSignature = root;
        desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
        desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
        desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
        desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
        desc.InputLayout.pInputElementDescs = layout;
        desc.InputLayout.NumElements = 2;
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        hr = ID3D12Device_CreateGraphicsPipelineState(r->device, &desc,
            &IID_ID3D12PipelineState, (void **)&pipeline);
        if (FAILED(hr)) goto fail;
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {0};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.NumDescriptors = 1;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = ID3D12Device_CreateDescriptorHeap(r->device, &heap_desc,
            &IID_ID3D12DescriptorHeap, (void **)&srv_heap);
        if (FAILED(hr)) goto fail;
        r->composite_root = root;
        r->composite_pipeline = pipeline;
        r->srv_heap = srv_heap;
        root = NULL;
        pipeline = NULL;
        srv_heap = NULL;
fail:
        if (errors) ID3D10Blob_Release(errors);
        if (ps) ID3D10Blob_Release(ps);
        if (vs) ID3D10Blob_Release(vs);
        if (signature) ID3D10Blob_Release(signature);
        if (srv_heap) ID3D12DescriptorHeap_Release(srv_heap);
        if (pipeline) ID3D12PipelineState_Release(pipeline);
        if (root) ID3D12RootSignature_Release(root);
        if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    }
    if (!r->copy_texture) {
        D3D12_RESOURCE_DESC desc;
        ID3D12Resource_GetDesc(r->targets[frame], &desc);
        D3D12_HEAP_PROPERTIES heap = {0};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        HRESULT hr = ID3D12Device_CreateCommittedResource(r->device,
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, NULL,
            &IID_ID3D12Resource, (void **)&r->copy_texture);
        if (FAILED(hr)) return nx_d3d12_failure(r, hr);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = {0};
        ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(r->srv_heap,
                                                                  &handle);
        ID3D12Device_CreateShaderResourceView(r->device, r->copy_texture,
                                               NULL, handle);
    }
    return NX_OK;
}

static nx_result nx_d3d12_ensure_shape_pipeline(nx_d3d12_renderer *r) {
    if (r->shape_pipeline) return NX_OK;
    ID3DBlob *signature = NULL, *vs = NULL, *ps = NULL, *errors = NULL;
    ID3D12RootSignature *root = NULL;
    ID3D12PipelineState *pipeline = NULL;
    D3D12_DESCRIPTOR_RANGE range = {0};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[2] = {0};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    parameters[0].DescriptorTable.pDescriptorRanges = &range;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.ShaderRegister = 0;
    parameters[1].Constants.Num32BitValues = 6;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root_desc = {0};
    root_desc.NumParameters = 2;
    root_desc.pParameters = parameters;
    root_desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    HRESULT hr = D3D12SerializeRootSignature(&root_desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
    if (FAILED(hr)) goto fail;
    hr = ID3D12Device_CreateRootSignature(r->device, 0,
        ID3D10Blob_GetBufferPointer(signature),
        ID3D10Blob_GetBufferSize(signature),
        &IID_ID3D12RootSignature, (void **)&root);
    if (FAILED(hr)) goto fail;
    hr = D3DCompile(nx_d3d12_shape_shader,
        sizeof(nx_d3d12_shape_shader) - 1u, "nexaui-shape", NULL, NULL,
        "VSMain", "vs_5_0", 0, 0, &vs, &errors);
    if (FAILED(hr)) goto fail;
    if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
    hr = D3DCompile(nx_d3d12_shape_shader,
        sizeof(nx_d3d12_shape_shader) - 1u, "nexaui-shape", NULL, NULL,
        "PSMain", "ps_5_0", 0, 0, &ps, &errors);
    if (FAILED(hr)) goto fail;
    D3D12_INPUT_ELEMENT_DESC layout[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {0};
    desc.pRootSignature = root;
    desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
    desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
    desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
    desc.InputLayout.pInputElementDescs = layout;
    desc.InputLayout.NumElements = 2;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    hr = ID3D12Device_CreateGraphicsPipelineState(r->device, &desc,
        &IID_ID3D12PipelineState, (void **)&pipeline);
    if (FAILED(hr)) goto fail;
    r->shape_root = root;
    r->shape_pipeline = pipeline;
    root = NULL;
    pipeline = NULL;
fail:
    if (errors) ID3D10Blob_Release(errors);
    if (ps) ID3D10Blob_Release(ps);
    if (vs) ID3D10Blob_Release(vs);
    if (signature) ID3D10Blob_Release(signature);
    if (pipeline) ID3D12PipelineState_Release(pipeline);
    if (root) ID3D12RootSignature_Release(root);
    return FAILED(hr) ? nx_d3d12_failure(r, hr) : NX_OK;
}

static nx_result nx_d3d12_ensure_bitmap_pipeline(nx_d3d12_renderer *r) {
    if (r->bitmap_pipeline) return NX_OK;
    ID3DBlob *signature = NULL, *vs = NULL, *ps = NULL, *errors = NULL;
    ID3D12RootSignature *root = NULL;
    ID3D12PipelineState *pipeline = NULL;
    D3D12_DESCRIPTOR_RANGE range = {0};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 2;
    range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameters[2] = {0};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    parameters[0].DescriptorTable.pDescriptorRanges = &range;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants.Num32BitValues = 5;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root_desc = {0};
    root_desc.NumParameters = 2;
    root_desc.pParameters = parameters;
    root_desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    HRESULT hr = D3D12SerializeRootSignature(&root_desc,
        D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
    if (FAILED(hr)) goto fail;
    hr = ID3D12Device_CreateRootSignature(r->device, 0,
        ID3D10Blob_GetBufferPointer(signature),
        ID3D10Blob_GetBufferSize(signature),
        &IID_ID3D12RootSignature, (void **)&root);
    if (FAILED(hr)) goto fail;
    hr = D3DCompile(nx_d3d12_bitmap_shader,
        sizeof(nx_d3d12_bitmap_shader) - 1u, "nexaui-bitmap", NULL, NULL,
        "VSMain", "vs_5_0", 0, 0, &vs, &errors);
    if (FAILED(hr)) goto fail;
    if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
    hr = D3DCompile(nx_d3d12_bitmap_shader,
        sizeof(nx_d3d12_bitmap_shader) - 1u, "nexaui-bitmap", NULL, NULL,
        "PSMain", "ps_5_0", 0, 0, &ps, &errors);
    if (FAILED(hr)) goto fail;
    D3D12_INPUT_ELEMENT_DESC layout[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {0};
    desc.pRootSignature = root;
    desc.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs);
    desc.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
    desc.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps);
    desc.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
    desc.InputLayout.pInputElementDescs = layout;
    desc.InputLayout.NumElements = 2;
    desc.SampleMask = UINT_MAX;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    hr = ID3D12Device_CreateGraphicsPipelineState(r->device, &desc,
        &IID_ID3D12PipelineState, (void **)&pipeline);
    if (FAILED(hr)) goto fail;
    r->bitmap_root = root;
    r->bitmap_pipeline = pipeline;
    root = NULL;
    pipeline = NULL;
fail:
    if (errors) ID3D10Blob_Release(errors);
    if (ps) ID3D10Blob_Release(ps);
    if (vs) ID3D10Blob_Release(vs);
    if (signature) ID3D10Blob_Release(signature);
    if (pipeline) ID3D12PipelineState_Release(pipeline);
    if (root) ID3D12RootSignature_Release(root);
    return FAILED(hr) ? nx_d3d12_failure(r, hr) : NX_OK;
}

static int nx_d3d12_emit_vertex(nx_d3d12_vertex *vertex,
                                 double x, double y, nx_color color,
                                 double scale, UINT width, UINT height) {
    vertex->x = (float)(2.0 * (x * scale) / width - 1.0);
    vertex->y = (float)(1.0 - 2.0 * (y * scale) / height);
    vertex->rgba[0] = floorf(color.r * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[1] = floorf(color.g * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[2] = floorf(color.b * 255.0f + 0.5f) / 255.0f;
    vertex->rgba[3] = floorf(color.a * 255.0f + 0.5f) / 255.0f;
    return isfinite(vertex->x) && isfinite(vertex->y);
}

static LONG nx_d3d12_first_clip_pixel(double edge, double scale,
                                      LONG limit) {
    double pixel = ceil(edge * scale - 0.5);
    if (pixel <= 0) return 0;
    if (pixel >= limit) return limit;
    return (LONG)pixel;
}

static D3D12_RECT nx_d3d12_intersect_clip(D3D12_RECT a, D3D12_RECT b) {
    D3D12_RECT out;
    out.left = a.left > b.left ? a.left : b.left;
    out.top = a.top > b.top ? a.top : b.top;
    out.right = a.right < b.right ? a.right : b.right;
    out.bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
    if (out.right < out.left) out.right = out.left;
    if (out.bottom < out.top) out.bottom = out.top;
    return out;
}

static D3D12_RECT nx_d3d12_command_clip(const nx_command *command,
                                         double scale, UINT width,
                                         UINT height) {
    double left = command->rect.x, top = command->rect.y;
    double right = left + command->rect.width;
    double bottom = top + command->rect.height;
    return (D3D12_RECT){
        nx_d3d12_first_clip_pixel(left, scale, (LONG)width),
        nx_d3d12_first_clip_pixel(top, scale, (LONG)height),
        nx_d3d12_first_clip_pixel(right, scale, (LONG)width),
        nx_d3d12_first_clip_pixel(bottom, scale, (LONG)height)
    };
}

static void nx_d3d12_rect_vertices(nx_point *positions, double left,
    double top, double right, double bottom) {
    positions[0] = (nx_point){(float)left, (float)top};
    positions[1] = (nx_point){(float)right, (float)top};
    positions[2] = (nx_point){(float)left, (float)bottom};
    positions[3] = (nx_point){(float)right, (float)top};
    positions[4] = (nx_point){(float)right, (float)bottom};
    positions[5] = (nx_point){(float)left, (float)bottom};
}

static UINT nx_d3d12_draw_vertex_count(nx_command_type type) {
    if (type == NX_COMMAND_FILL_TRIANGLE) return 3u;
    if (type == NX_COMMAND_STROKE_RECT) return 24u;
    return 6u;
}

static nx_result nx_d3d12_reserve_vertex_ring(nx_d3d12_renderer *r,
                                                UINT frame, UINT64 bytes,
                                                UINT64 *out_offset) {
    if (!r->vertex_ring[frame] || r->vertex_ring_capacity[frame] < bytes) {
        UINT64 capacity = r->vertex_ring_capacity[frame] ?
            r->vertex_ring_capacity[frame] : (UINT64)1024u * 1024u;
        while (capacity < bytes) {
            if (capacity > UINT64_MAX / 2u) { capacity = bytes; break; }
            capacity *= 2u;
        }
        D3D12_HEAP_PROPERTIES heap = {0};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc = {0};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = capacity;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource *replacement = NULL;
        HRESULT hr = ID3D12Device_CreateCommittedResource(r->device,
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
            &IID_ID3D12Resource, (void **)&replacement);
        if (FAILED(hr)) return nx_d3d12_failure(r, hr);
        if (r->vertex_ring[frame])
            ID3D12Resource_Release(r->vertex_ring[frame]);
        r->vertex_ring[frame] = replacement;
        r->vertex_ring_capacity[frame] = capacity;
        r->vertex_ring_head[frame] = 0;
        ++r->vertex_ring_allocations;
    }
    UINT64 head = r->vertex_ring_head[frame];
    if (head > UINT64_MAX - 15u) head = 0;
    else head = (head + 15u) & ~UINT64_C(15);
    if (head > r->vertex_ring_capacity[frame] - bytes) {
        head = 0;
        ++r->vertex_ring_wraps;
    }
    *out_offset = head;
    return NX_OK;
}

static nx_result nx_d3d12_upload_vertices(nx_d3d12_renderer *r,
    UINT frame, const nx_command *commands, size_t count, double scale,
    UINT64 *out_offset, UINT *out_count) {
    *out_offset = 0;
    *out_count = 0;
    size_t vertex_count = 0;
    for (size_t i = 0; i < count; ++i) {
        size_t add;
        if (commands[i].type == NX_COMMAND_STROKE_RECT) add = 24;
        else if (commands[i].type == NX_COMMAND_FILL_RECT ||
            commands[i].type == NX_COMMAND_FILL_ROUND_RECT ||
            commands[i].type == NX_COMMAND_BITMAP) add = 6;
        else if (commands[i].type == NX_COMMAND_LINE) add = 6;
        else if (commands[i].type == NX_COMMAND_FILL_TRIANGLE) add = 3;
        else if (commands[i].type == NX_COMMAND_PUSH_CLIP ||
                 commands[i].type == NX_COMMAND_POP_CLIP) add = 0;
        else return NX_ERROR_UNSUPPORTED;
        if (vertex_count > UINT_MAX / sizeof(nx_d3d12_vertex) - add)
            return NX_ERROR_LIMIT;
        vertex_count += add;
    }
    if (!vertex_count) return NX_OK;
    nx_result result = nx_d3d12_ensure_pipeline(r);
    if (result != NX_OK) return result;
    UINT64 bytes = (UINT64)vertex_count * sizeof(nx_d3d12_vertex);
    UINT64 offset = 0;
    result = nx_d3d12_reserve_vertex_ring(r, frame, bytes, &offset);
    if (result != NX_OK) return result;
    uint8_t *mapped_bytes = NULL;
    D3D12_RANGE no_read = {0, 0};
    HRESULT hr = ID3D12Resource_Map(r->vertex_ring[frame], 0, &no_read,
                                    (void **)&mapped_bytes);
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    nx_d3d12_vertex *mapped = (nx_d3d12_vertex *)(mapped_bytes + offset);
    size_t at = 0;
    for (size_t i = 0; i < count; ++i) {
        const nx_command *cmd = &commands[i];
        if (cmd->type == NX_COMMAND_PUSH_CLIP ||
            cmd->type == NX_COMMAND_POP_CLIP) continue;
        nx_point positions[24];
        size_t n = 3;
        if (cmd->type == NX_COMMAND_LINE) {
            double radius = (double)cmd->thickness / 2.0;
            double ax = cmd->points[0].x, ay = cmd->points[0].y;
            double bx = cmd->points[1].x, by = cmd->points[1].y;
            nx_d3d12_rect_vertices(positions,
                fmin(ax, bx) - radius, fmin(ay, by) - radius,
                fmax(ax, bx) + radius, fmax(ay, by) + radius);
            n = 6;
        } else if (cmd->type == NX_COMMAND_STROKE_RECT) {
            double left = cmd->rect.x, top = cmd->rect.y;
            double right = left + cmd->rect.width;
            double bottom = top + cmd->rect.height;
            double t = cmd->thickness;
            if (2.0 * t >= cmd->rect.width ||
                2.0 * t >= cmd->rect.height) {
                nx_d3d12_rect_vertices(positions, left, top, right, bottom);
                for (unsigned strip = 1; strip < 4; ++strip)
                    nx_d3d12_rect_vertices(positions + strip * 6u,
                                            left, top, left, top);
            } else {
                nx_d3d12_rect_vertices(positions, left, top, right, top + t);
                nx_d3d12_rect_vertices(positions + 6u, left,
                                        bottom - t, right, bottom);
                nx_d3d12_rect_vertices(positions + 12u, left,
                                        top + t, left + t, bottom - t);
                nx_d3d12_rect_vertices(positions + 18u, right - t,
                                        top + t, right, bottom - t);
            }
            n = 24;
        } else if (cmd->type == NX_COMMAND_FILL_RECT ||
            cmd->type == NX_COMMAND_FILL_ROUND_RECT ||
            cmd->type == NX_COMMAND_BITMAP) {
            double left = cmd->rect.x, top = cmd->rect.y;
            double right = left + cmd->rect.width;
            double bottom = top + cmd->rect.height;
            nx_d3d12_rect_vertices(positions, left, top, right, bottom);
            n = 6;
        } else {
            memcpy(positions, cmd->points, 3 * sizeof(nx_point));
        }
        for (size_t v = 0; v < n; ++v) {
            if (!nx_d3d12_emit_vertex(&mapped[at++], positions[v].x,
                positions[v].y, cmd->color, scale, r->width, r->height)) {
                ID3D12Resource_Unmap(r->vertex_ring[frame], 0, &no_read);
                return NX_ERROR_LIMIT;
            }
        }
    }
    D3D12_RANGE written = {(SIZE_T)offset, (SIZE_T)(offset + bytes)};
    ID3D12Resource_Unmap(r->vertex_ring[frame], 0, &written);
    r->vertex_ring_head[frame] = offset + bytes;
    *out_offset = offset;
    *out_count = (UINT)vertex_count;
    return NX_OK;
}

static void nx_d3d12_release_bitmaps(nx_d3d12_bitmap_gpu *bitmaps,
                                       size_t count) {
    if (!bitmaps) return;
    for (size_t i = 0; i < count; ++i) {
        if (bitmaps[i].texture && !bitmaps[i].pooled)
            ID3D12Resource_Release(bitmaps[i].texture);
    }
    free(bitmaps);
}

static nx_result nx_d3d12_acquire_bitmap_texture(nx_d3d12_renderer *r,
    const D3D12_RESOURCE_DESC *desc, nx_d3d12_bitmap_gpu *out) {
    for (size_t i = 0; i < r->texture_pool_count; ++i) {
        nx_d3d12_texture_pool_entry *entry = &r->texture_pool[i];
        if (!entry->used && entry->width == desc->Width &&
            entry->height == desc->Height && entry->format == desc->Format) {
            entry->used = 1;
            out->texture = entry->texture;
            out->pooled = 1;
            out->needs_copy_transition = entry->shader_state;
            return NX_OK;
        }
    }
    D3D12_RESOURCE_ALLOCATION_INFO allocation = {0};
    ID3D12Device_GetResourceAllocationInfo(r->device, &allocation,
                                             0, 1, desc);
    size_t slot = r->texture_pool_count;
    UINT64 previous_bytes = 0;
    if (allocation.SizeInBytes <= NX_D3D12_TEXTURE_POOL_BYTES) {
        if (slot == NX_D3D12_TEXTURE_POOL_MAX ||
            r->texture_pool_bytes + allocation.SizeInBytes >
                NX_D3D12_TEXTURE_POOL_BYTES) {
            for (size_t i = 0; i < r->texture_pool_count; ++i) {
                nx_d3d12_texture_pool_entry *entry = &r->texture_pool[i];
                if (!entry->used &&
                    r->texture_pool_bytes - entry->bytes +
                        allocation.SizeInBytes <= NX_D3D12_TEXTURE_POOL_BYTES) {
                    slot = i;
                    previous_bytes = entry->bytes;
                    break;
                }
            }
        }
    }
    D3D12_HEAP_PROPERTIES heap = {0};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    ID3D12Resource *texture = NULL;
    HRESULT hr = ID3D12Device_CreateCommittedResource(r->device, &heap,
        D3D12_HEAP_FLAG_NONE, desc, D3D12_RESOURCE_STATE_COPY_DEST,
        NULL, &IID_ID3D12Resource, (void **)&texture);
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    ++r->texture_creations;
    if (allocation.SizeInBytes <= NX_D3D12_TEXTURE_POOL_BYTES &&
        slot < NX_D3D12_TEXTURE_POOL_MAX &&
        r->texture_pool_bytes - previous_bytes + allocation.SizeInBytes <=
            NX_D3D12_TEXTURE_POOL_BYTES) {
        nx_d3d12_texture_pool_entry *entry = &r->texture_pool[slot];
        if (slot == r->texture_pool_count) ++r->texture_pool_count;
        if (entry->texture) ID3D12Resource_Release(entry->texture);
        *entry = (nx_d3d12_texture_pool_entry){texture, (UINT)desc->Width,
            desc->Height, desc->Format, allocation.SizeInBytes, 1, 0};
        r->texture_pool_bytes = r->texture_pool_bytes - previous_bytes +
                                allocation.SizeInBytes;
        out->pooled = 1;
    }
    out->texture = texture;
    return NX_OK;
}

static nx_result nx_d3d12_create_bitmap_texture(nx_d3d12_renderer *r,
    nx_d3d12_bitmap_gpu *out, UINT64 *out_bytes) {
    const nx_bitmap_view *bitmap = &out->source;
    size_t channels = bitmap->format == NX_BITMAP_RGBA8 ? 4u : 1u;
    size_t row_bytes = (size_t)bitmap->width * channels;
    if (row_bytes > UINT_MAX) return NX_ERROR_LIMIT;
    D3D12_RESOURCE_DESC texture = {0};
    texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width = bitmap->width;
    texture.Height = bitmap->height;
    texture.DepthOrArraySize = 1;
    texture.MipLevels = 1;
    texture.Format = bitmap->format == NX_BITMAP_RGBA8 ?
        DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8_UNORM;
    texture.SampleDesc.Count = 1;
    texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    UINT64 row_size = 0, upload_bytes = 0;
    ID3D12Device_GetCopyableFootprints(r->device, &texture, 0, 1, 0,
        &out->footprint, NULL, &row_size, &upload_bytes);
    if (row_size != row_bytes || upload_bytes == 0 ||
        upload_bytes > SIZE_MAX || out->footprint.Footprint.RowPitch < row_bytes)
        return NX_ERROR_LIMIT;
    nx_result result = nx_d3d12_acquire_bitmap_texture(r, &texture, out);
    if (result != NX_OK) return result;
    *out_bytes = upload_bytes;
    return NX_OK;
}

static nx_result nx_d3d12_ensure_bitmap_upload(nx_d3d12_renderer *r,
                                                UINT64 required) {
    if (r->bitmap_upload && r->bitmap_upload_capacity >= required)
        return NX_OK;
    UINT64 capacity = r->bitmap_upload_capacity ?
        r->bitmap_upload_capacity : 65536u;
    while (capacity < required) {
        if (capacity > UINT64_MAX / 2u) { capacity = required; break; }
        capacity *= 2u;
    }
    D3D12_RESOURCE_DESC buffer = {0};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = capacity;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap = {0};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    ID3D12Resource *replacement = NULL;
    HRESULT hr = ID3D12Device_CreateCommittedResource(r->device, &heap,
        D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL, &IID_ID3D12Resource, (void **)&replacement);
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    if (r->bitmap_upload) ID3D12Resource_Release(r->bitmap_upload);
    r->bitmap_upload = replacement;
    r->bitmap_upload_capacity = capacity;
    return NX_OK;
}

static nx_result nx_d3d12_ensure_bitmap_heap(nx_d3d12_renderer *r,
                                              size_t unique_count) {
    if (unique_count > UINT_MAX / 2u) return NX_ERROR_LIMIT;
    UINT required = (UINT)unique_count * 2u;
    if (r->bitmap_srv_heap && r->bitmap_srv_capacity >= required)
        return NX_OK;
    UINT capacity = r->bitmap_srv_capacity ? r->bitmap_srv_capacity : 32u;
    while (capacity < required) {
        if (capacity > UINT_MAX / 2u) { capacity = required; break; }
        capacity *= 2u;
    }
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {0};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = capacity;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ID3D12DescriptorHeap *replacement = NULL;
    HRESULT hr = ID3D12Device_CreateDescriptorHeap(r->device, &heap_desc,
        &IID_ID3D12DescriptorHeap, (void **)&replacement);
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    if (r->bitmap_srv_heap) ID3D12DescriptorHeap_Release(r->bitmap_srv_heap);
    r->bitmap_srv_heap = replacement;
    r->bitmap_srv_capacity = capacity;
    return NX_OK;
}

static size_t nx_d3d12_bitmap_hash(nx_bitmap_view bitmap) {
    uint64_t hash = (uint64_t)(uintptr_t)bitmap.pixels;
    hash ^= (uint64_t)bitmap.width << 32u | bitmap.height;
    hash ^= (uint64_t)bitmap.stride_bytes;
    hash ^= (uint64_t)bitmap.format << 48u;
    hash ^= hash >> 33u;
    hash *= UINT64_C(0xff51afd7ed558ccd);
    hash ^= hash >> 33u;
    return (size_t)hash;
}

static int nx_d3d12_same_bitmap(nx_bitmap_view a, nx_bitmap_view b) {
    return a.pixels == b.pixels && a.width == b.width &&
           a.height == b.height && a.stride_bytes == b.stride_bytes &&
           a.format == b.format;
}

static nx_result nx_d3d12_prepare_bitmaps(nx_d3d12_renderer *r,
    const nx_command *commands, size_t command_count, size_t bitmap_count,
    nx_d3d12_bitmap_gpu **out_bitmaps, size_t *out_unique_count,
    size_t **out_command_map) {
    if (bitmap_count > SIZE_MAX / sizeof(nx_d3d12_bitmap_gpu) ||
        bitmap_count > SIZE_MAX / sizeof(size_t) ||
        bitmap_count > SIZE_MAX / 2u) return NX_ERROR_LIMIT;
    nx_d3d12_bitmap_gpu *bitmaps = calloc(bitmap_count, sizeof(*bitmaps));
    size_t *command_map = malloc(bitmap_count * sizeof(*command_map));
    size_t slot_count = 1u;
    while (slot_count < bitmap_count * 2u) {
        if (slot_count > SIZE_MAX / 2u) {
            free(command_map);
            free(bitmaps);
            return NX_ERROR_LIMIT;
        }
        slot_count *= 2u;
    }
    size_t *slots = calloc(slot_count, sizeof(*slots));
    if (!bitmaps || !command_map || !slots) {
        free(slots);
        free(command_map);
        free(bitmaps);
        return NX_ERROR_MEMORY;
    }
    *out_bitmaps = bitmaps;
    *out_command_map = command_map;
    size_t unique_count = 0, at = 0;
    for (size_t i = 0; i < command_count; ++i) {
        if (commands[i].type != NX_COMMAND_BITMAP) continue;
        nx_bitmap_view source = commands[i].bitmap;
        size_t slot = nx_d3d12_bitmap_hash(source) & (slot_count - 1u);
        while (slots[slot] && !nx_d3d12_same_bitmap(
            bitmaps[slots[slot] - 1u].source, source))
            slot = (slot + 1u) & (slot_count - 1u);
        if (!slots[slot]) {
            bitmaps[unique_count].source = source;
            slots[slot] = ++unique_count;
        }
        command_map[at++] = slots[slot] - 1u;
    }
    free(slots);
    *out_unique_count = unique_count;
    for (size_t i = 0; i < r->texture_pool_count; ++i)
        r->texture_pool[i].used = 0;
    UINT64 total_bytes = 0;
    for (size_t i = 0; i < unique_count; ++i) {
        UINT64 bytes = 0;
        nx_result result = nx_d3d12_create_bitmap_texture(r, &bitmaps[i],
                                                            &bytes);
        if (result != NX_OK) return result;
        if (total_bytes > UINT64_MAX - 511u) return NX_ERROR_LIMIT;
        total_bytes = (total_bytes + 511u) & ~UINT64_C(511);
        if (bytes > UINT64_MAX - total_bytes ||
            total_bytes + bytes > SIZE_MAX) return NX_ERROR_LIMIT;
        bitmaps[i].footprint.Offset = total_bytes;
        total_bytes += bytes;
    }
    nx_result result = nx_d3d12_ensure_bitmap_upload(r, total_bytes);
    if (result != NX_OK) return result;
    result = nx_d3d12_ensure_bitmap_heap(r, unique_count);
    if (result != NX_OK) return result;
    uint8_t *mapped = NULL;
    D3D12_RANGE no_read = {0, 0};
    HRESULT hr = ID3D12Resource_Map(r->bitmap_upload, 0, &no_read,
                                     (void **)&mapped);
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        r->bitmap_srv_heap, &handle);
    UINT stride = ID3D12Device_GetDescriptorHandleIncrementSize(r->device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for (size_t i = 0; i < unique_count; ++i) {
        const nx_bitmap_view *source = &bitmaps[i].source;
        size_t channels = source->format == NX_BITMAP_RGBA8 ? 4u : 1u;
        size_t row_bytes = (size_t)source->width * channels;
        for (uint32_t y = 0; y < source->height; ++y) {
            memcpy(mapped + bitmaps[i].footprint.Offset +
                (size_t)y * bitmaps[i].footprint.Footprint.RowPitch,
                source->pixels + (size_t)y * source->stride_bytes,
                row_bytes);
        }
        ID3D12Device_CreateShaderResourceView(r->device, r->copy_texture,
                                               NULL, handle);
        handle.ptr += stride;
        ID3D12Device_CreateShaderResourceView(r->device, bitmaps[i].texture,
                                               NULL, handle);
        handle.ptr += stride;
    }
    D3D12_RANGE written = {0, (SIZE_T)total_bytes};
    ID3D12Resource_Unmap(r->bitmap_upload, 0, &written);
    return NX_OK;
}

static nx_result nx_d3d12_create_readback(nx_d3d12_renderer *r, UINT frame,
    ID3D12Resource **out_resource,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT *out_footprint,
    UINT64 *out_bytes) {
    *out_resource = NULL;
    D3D12_RESOURCE_DESC texture;
    ID3D12Resource_GetDesc(r->targets[frame], &texture);
    ID3D12Device_GetCopyableFootprints(r->device, &texture, 0, 1, 0,
        out_footprint, NULL, NULL, out_bytes);
    if (*out_bytes == 0 || *out_bytes > SIZE_MAX) return NX_ERROR_LIMIT;
    D3D12_HEAP_PROPERTIES heap = {0};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC buffer = {0};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = *out_bytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    HRESULT hr = ID3D12Device_CreateCommittedResource(r->device,
        &heap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, NULL,
        &IID_ID3D12Resource, (void **)out_resource);
    return FAILED(hr) ? nx_d3d12_failure(r, hr) : NX_OK;
}

static void nx_d3d12_release_completed_uploads(nx_d3d12_renderer *renderer) {
    nx_d3d12_release_bitmaps(renderer->pending_bitmaps,
                              renderer->pending_bitmap_count);
    renderer->pending_bitmaps = NULL;
    renderer->pending_bitmap_count = 0;
}

static void nx_d3d12_release_timing(nx_d3d12_renderer *r) {
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i) {
        if (r->timestamp_readback[i])
            ID3D12Resource_Release(r->timestamp_readback[i]);
        r->timestamp_readback[i] = NULL;
        r->timestamp_frame[i] = 0;
    }
    if (r->timestamp_heap) ID3D12QueryHeap_Release(r->timestamp_heap);
    r->timestamp_heap = NULL;
    r->timestamp_frequency = 0;
    r->last_timed_frame = 0;
    r->last_gpu_upload_ms = 0;
    r->last_gpu_render_ms = 0;
    r->last_gpu_frame_ms = 0;
}

static void nx_d3d12_create_timing(nx_d3d12_renderer *r) {
    UINT64 frequency = 0;
    if (FAILED(ID3D12CommandQueue_GetTimestampFrequency(r->queue,
                                                         &frequency)) ||
        !frequency) return;
    D3D12_QUERY_HEAP_DESC query = {0};
    query.Count = NX_D3D12_BUFFER_COUNT * NX_D3D12_TIMESTAMPS_PER_FRAME;
    query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    if (FAILED(ID3D12Device_CreateQueryHeap(r->device, &query,
        &IID_ID3D12QueryHeap, (void **)&r->timestamp_heap))) return;
    D3D12_HEAP_PROPERTIES heap = {0};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc = {0};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = NX_D3D12_TIMESTAMPS_PER_FRAME * sizeof(UINT64);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i) {
        if (FAILED(ID3D12Device_CreateCommittedResource(r->device, &heap,
            D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
            NULL, &IID_ID3D12Resource,
            (void **)&r->timestamp_readback[i]))) {
            nx_d3d12_release_timing(r);
            return;
        }
    }
    r->timestamp_frequency = frequency;
}

static nx_result nx_d3d12_collect_timing(nx_d3d12_renderer *r,
                                          UINT frame, UINT64 completed) {
    if (!r->timestamp_frame[frame] ||
        completed < r->frame_fence[frame]) return NX_OK;
    D3D12_RANGE read_range = {0,
        NX_D3D12_TIMESTAMPS_PER_FRAME * sizeof(UINT64)};
    const UINT64 *ticks = NULL;
    HRESULT hr = ID3D12Resource_Map(r->timestamp_readback[frame], 0,
                                    &read_range, (void **)&ticks);
    if (FAILED(hr)) {
        nx_result result = nx_d3d12_failure(r, hr);
        r->timestamp_frame[frame] = 0;
        return r->device_lost ? result : NX_OK;
    }
    UINT64 start = ticks[0], uploaded = ticks[1], end = ticks[2];
    D3D12_RANGE no_write = {0, 0};
    ID3D12Resource_Unmap(r->timestamp_readback[frame], 0, &no_write);
    if (start <= uploaded && uploaded <= end &&
        r->timestamp_frame[frame] > r->last_timed_frame) {
        double milliseconds = 1000.0 / (double)r->timestamp_frequency;
        r->last_gpu_upload_ms = (double)(uploaded - start) * milliseconds;
        r->last_gpu_render_ms = (double)(end - uploaded) * milliseconds;
        r->last_gpu_frame_ms = (double)(end - start) * milliseconds;
        r->last_timed_frame = r->timestamp_frame[frame];
    }
    r->timestamp_frame[frame] = 0;
    return NX_OK;
}

static nx_result nx_d3d12_wait_for_value(nx_d3d12_renderer *renderer,
                                         UINT64 value) {
    if (renderer->device_lost) return NX_ERROR_STATE;
    if (!value || !renderer->fence) return NX_OK;
    UINT64 completed = ID3D12Fence_GetCompletedValue(renderer->fence);
    if (completed == UINT64_MAX) {
        renderer->device_lost = 1;
        return NX_ERROR_STATE;
    }
    if (completed >= value) return NX_OK;
    if (!renderer->fence_event) return NX_ERROR_PLATFORM;
    HRESULT hr = ID3D12Fence_SetEventOnCompletion(renderer->fence, value,
                                          renderer->fence_event);
    if (FAILED(hr)) return nx_d3d12_failure(renderer, hr);
    DWORD wait = WaitForSingleObject(renderer->fence_event, 5000);
    completed = ID3D12Fence_GetCompletedValue(renderer->fence);
    if (completed == UINT64_MAX) {
        renderer->device_lost = 1;
        return NX_ERROR_STATE;
    }
    if (wait != WAIT_OBJECT_0 || completed < value)
        return NX_ERROR_PLATFORM;
    return NX_OK;
}

static nx_result nx_d3d12_signal_gpu(nx_d3d12_renderer *renderer,
                                     UINT64 *out_value) {
    if (renderer->fence_value == UINT64_MAX) {
        renderer->device_lost = 1;
        return NX_ERROR_STATE;
    }
    UINT64 value = renderer->fence_value + 1u;
    HRESULT hr = ID3D12CommandQueue_Signal(renderer->queue,
                                           renderer->fence, value);
    if (FAILED(hr)) {
        (void)nx_d3d12_failure(renderer, hr);
        renderer->device_lost = 1; /* Submitted work cannot be tracked safely. */
        return NX_ERROR_STATE;
    }
    renderer->fence_value = value;
    *out_value = value;
    return NX_OK;
}

static nx_result nx_d3d12_wait_gpu(nx_d3d12_renderer *renderer) {
    if (renderer->device_lost) return NX_ERROR_STATE;
    if (!renderer->queue || !renderer->fence) return NX_OK;
    UINT64 value = 0;
    nx_result result = nx_d3d12_signal_gpu(renderer, &value);
    if (result != NX_OK) return result;
    result = nx_d3d12_wait_for_value(renderer, value);
    if (result != NX_OK) return result;
    renderer->work_pending = 0;
    nx_d3d12_release_completed_uploads(renderer);
    return NX_OK;
}

static void nx_d3d12_release_targets(nx_d3d12_renderer *renderer) {
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i) {
        if (renderer->targets[i]) {
            ID3D12Resource_Release(renderer->targets[i]);
            renderer->targets[i] = NULL;
        }
    }
}

static nx_result nx_d3d12_create_targets(nx_d3d12_renderer *renderer) {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(
        renderer->rtv_heap, &handle);
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i) {
        HRESULT hr = IDXGISwapChain3_GetBuffer(renderer->swapchain, i,
            &IID_ID3D12Resource, (void **)&renderer->targets[i]);
        if (FAILED(hr)) {
            nx_d3d12_release_targets(renderer);
            return nx_d3d12_failure(renderer, hr);
        }
        ID3D12Device_CreateRenderTargetView(renderer->device,
            renderer->targets[i], NULL, handle);
        handle.ptr += renderer->rtv_stride;
    }
    return NX_OK;
}

static nx_result nx_d3d12_resize(nx_d3d12_renderer *renderer,
                                  UINT width, UINT height) {
    if (renderer->width == width && renderer->height == height)
        return NX_OK;
    nx_result result = nx_d3d12_wait_gpu(renderer);
    if (result != NX_OK) return result;
    if (renderer->copy_texture) {
        ID3D12Resource_Release(renderer->copy_texture);
        renderer->copy_texture = NULL;
    }
    nx_d3d12_release_targets(renderer);
    renderer->width = renderer->height = 0;
    HRESULT hr = IDXGISwapChain3_ResizeBuffers(renderer->swapchain,
        NX_D3D12_BUFFER_COUNT, width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) return nx_d3d12_failure(renderer, hr);
    result = nx_d3d12_create_targets(renderer);
    if (result != NX_OK) return result;
    renderer->width = width;
    renderer->height = height;
    return NX_OK;
}

static void nx_d3d12_release_objects(nx_d3d12_renderer *renderer) {
    nx_d3d12_release_targets(renderer);
    nx_d3d12_release_completed_uploads(renderer);
    nx_d3d12_release_timing(renderer);
    for (size_t i = 0; i < renderer->texture_pool_count; ++i)
        if (renderer->texture_pool[i].texture)
            ID3D12Resource_Release(renderer->texture_pool[i].texture);
    if (renderer->pending_readback)
        ID3D12Resource_Release(renderer->pending_readback);
    if (renderer->copy_texture)
        ID3D12Resource_Release(renderer->copy_texture);
    if (renderer->bitmap_upload)
        ID3D12Resource_Release(renderer->bitmap_upload);
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i)
        if (renderer->vertex_ring[i])
            ID3D12Resource_Release(renderer->vertex_ring[i]);
    if (renderer->bitmap_srv_heap)
        ID3D12DescriptorHeap_Release(renderer->bitmap_srv_heap);
    if (renderer->srv_heap) ID3D12DescriptorHeap_Release(renderer->srv_heap);
    if (renderer->composite_pipeline)
        ID3D12PipelineState_Release(renderer->composite_pipeline);
    if (renderer->composite_root)
        ID3D12RootSignature_Release(renderer->composite_root);
    if (renderer->shape_pipeline)
        ID3D12PipelineState_Release(renderer->shape_pipeline);
    if (renderer->shape_root)
        ID3D12RootSignature_Release(renderer->shape_root);
    if (renderer->bitmap_pipeline)
        ID3D12PipelineState_Release(renderer->bitmap_pipeline);
    if (renderer->bitmap_root)
        ID3D12RootSignature_Release(renderer->bitmap_root);
    if (renderer->pipeline) ID3D12PipelineState_Release(renderer->pipeline);
    if (renderer->root_signature)
        ID3D12RootSignature_Release(renderer->root_signature);
    if (renderer->fence_event) CloseHandle(renderer->fence_event);
    if (renderer->fence) ID3D12Fence_Release(renderer->fence);
    if (renderer->list) ID3D12GraphicsCommandList_Release(renderer->list);
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i)
        if (renderer->allocators[i])
            ID3D12CommandAllocator_Release(renderer->allocators[i]);
    if (renderer->rtv_heap) ID3D12DescriptorHeap_Release(renderer->rtv_heap);
    if (renderer->swapchain) IDXGISwapChain3_Release(renderer->swapchain);
    if (renderer->queue) ID3D12CommandQueue_Release(renderer->queue);
    if (renderer->device) ID3D12Device_Release(renderer->device);
    if (renderer->factory) IDXGIFactory4_Release(renderer->factory);
    renderer->factory = NULL;
    renderer->device = NULL;
    renderer->queue = NULL;
    renderer->swapchain = NULL;
    renderer->rtv_heap = NULL;
    memset(renderer->allocators, 0, sizeof(renderer->allocators));
    renderer->list = NULL;
    renderer->fence = NULL;
    renderer->root_signature = NULL;
    renderer->pipeline = NULL;
    renderer->pending_readback = NULL;
    renderer->copy_texture = NULL;
    renderer->bitmap_upload = NULL;
    renderer->bitmap_upload_capacity = 0;
    memset(renderer->vertex_ring, 0, sizeof(renderer->vertex_ring));
    memset(renderer->vertex_ring_capacity, 0,
           sizeof(renderer->vertex_ring_capacity));
    memset(renderer->vertex_ring_head, 0,
           sizeof(renderer->vertex_ring_head));
    renderer->vertex_ring_allocations = 0;
    renderer->vertex_ring_wraps = 0;
    memset(renderer->frame_fence, 0, sizeof(renderer->frame_fence));
    renderer->last_frame = 0;
    renderer->timestamp_sequence = 0;
    renderer->bitmap_srv_heap = NULL;
    renderer->bitmap_srv_capacity = 0;
    memset(renderer->texture_pool, 0, sizeof(renderer->texture_pool));
    renderer->texture_pool_count = 0;
    renderer->texture_pool_bytes = 0;
    renderer->recording_draw_calls = 0;
    renderer->last_draw_calls = 0;
    renderer->last_bitmap_uploads = 0;
    renderer->recording_copy_texels = 0;
    renderer->last_copy_texels = 0;
    renderer->texture_creations = 0;
    renderer->srv_heap = NULL;
    renderer->composite_pipeline = NULL;
    renderer->composite_root = NULL;
    renderer->shape_pipeline = NULL;
    renderer->shape_root = NULL;
    renderer->bitmap_pipeline = NULL;
    renderer->bitmap_root = NULL;
    renderer->fence_event = NULL;
    renderer->fence_value = 0;
    renderer->rtv_stride = 0;
    renderer->width = renderer->height = 0;
    renderer->using_warp = 0;
    renderer->work_pending = 0;
}

void nx_d3d12_renderer_destroy(nx_d3d12_renderer *renderer) {
    if (!renderer || GetCurrentThreadId() != renderer->owner_thread) return;
    while (renderer->work_pending && !renderer->device_lost) {
        nx_result result = nx_d3d12_wait_gpu(renderer);
        if (result == NX_OK || result == NX_ERROR_STATE) break;
    }
    nx_d3d12_release_objects(renderer);
    free(renderer);
}

static nx_result nx_d3d12_build_objects(nx_d3d12_renderer *r) {
    IDXGISwapChain1 *base_swapchain = NULL;
    IDXGIAdapter *warp = NULL;
    RECT client;
    if (!GetClientRect(r->hwnd, &client)) goto fail;
    r->width = (UINT)(client.right - client.left);
    r->height = (UINT)(client.bottom - client.top);
    if (!r->width) r->width = 1;
    if (!r->height) r->height = 1;
    if (FAILED(CreateDXGIFactory2(0, &IID_IDXGIFactory4,
                                   (void **)&r->factory))) goto fail;
    HRESULT hr;
#ifdef NEXAUI_D3D12_TEST_HOOKS
    hr = r->test_force_warp ? E_FAIL : D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
                                   &IID_ID3D12Device, (void **)&r->device);
#else
    hr = D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
                                   &IID_ID3D12Device, (void **)&r->device);
#endif
    if (FAILED(hr)) {
        if (FAILED(IDXGIFactory4_EnumWarpAdapter(r->factory,
            &IID_IDXGIAdapter, (void **)&warp))) goto fail;
        hr = D3D12CreateDevice((IUnknown *)warp, D3D_FEATURE_LEVEL_11_0,
                               &IID_ID3D12Device, (void **)&r->device);
        IDXGIAdapter_Release(warp);
        warp = NULL;
        if (FAILED(hr)) goto fail;
        r->using_warp = 1;
    }
    D3D12_COMMAND_QUEUE_DESC queue = {0};
    queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(ID3D12Device_CreateCommandQueue(r->device, &queue,
        &IID_ID3D12CommandQueue, (void **)&r->queue))) goto fail;
    D3D12_DESCRIPTOR_HEAP_DESC heap = {0};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.NumDescriptors = NX_D3D12_BUFFER_COUNT;
    if (FAILED(ID3D12Device_CreateDescriptorHeap(r->device, &heap,
        &IID_ID3D12DescriptorHeap, (void **)&r->rtv_heap))) goto fail;
    r->rtv_stride = ID3D12Device_GetDescriptorHandleIncrementSize(r->device,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i)
        if (FAILED(ID3D12Device_CreateCommandAllocator(r->device,
            D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator,
            (void **)&r->allocators[i]))) goto fail;
    if (FAILED(ID3D12Device_CreateCommandList(r->device, 0,
        D3D12_COMMAND_LIST_TYPE_DIRECT, r->allocators[0], NULL,
        &IID_ID3D12GraphicsCommandList, (void **)&r->list))) goto fail;
    if (FAILED(ID3D12GraphicsCommandList_Close(r->list))) goto fail;
    DXGI_SWAP_CHAIN_DESC1 swap_desc = {0};
    swap_desc.Width = r->width;
    swap_desc.Height = r->height;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = NX_D3D12_BUFFER_COUNT;
    swap_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (FAILED(IDXGIFactory4_CreateSwapChainForHwnd(r->factory,
        (IUnknown *)r->queue, r->hwnd, &swap_desc, NULL, NULL,
        &base_swapchain))) goto fail;
    if (FAILED(IDXGISwapChain1_QueryInterface(base_swapchain,
        &IID_IDXGISwapChain3, (void **)&r->swapchain))) goto fail;
    IDXGISwapChain1_Release(base_swapchain);
    base_swapchain = NULL;
    (void)IDXGIFactory4_MakeWindowAssociation(r->factory, r->hwnd,
                                               DXGI_MWA_NO_ALT_ENTER);
#ifdef NEXAUI_D3D12_TEST_HOOKS
    if (r->test_fail_next_rebuild) {
        r->test_fail_next_rebuild = 0;
        goto fail;
    }
#endif
    if (nx_d3d12_create_targets(r) != NX_OK) goto fail;
    if (FAILED(ID3D12Device_CreateFence(r->device, 0,
        D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence,
        (void **)&r->fence))) goto fail;
    r->fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!r->fence_event) goto fail;
    nx_d3d12_create_timing(r);
    return NX_OK;
fail:
    if (warp) IDXGIAdapter_Release(warp);
    if (base_swapchain) IDXGISwapChain1_Release(base_swapchain);
    nx_d3d12_release_objects(r);
    return NX_ERROR_PLATFORM;
}

nx_result nx_d3d12_renderer_create(nx_window *window,
                                    nx_d3d12_renderer **out_renderer) {
    if (!out_renderer) return NX_ERROR_ARGUMENT;
    *out_renderer = NULL;
    HWND hwnd = nx_window_native_hwnd(window);
    if (!hwnd) return NX_ERROR_ARGUMENT;
    nx_d3d12_renderer *r = calloc(1, sizeof(*r));
    if (!r) return NX_ERROR_MEMORY;
    r->hwnd = hwnd;
    r->owner_thread = GetCurrentThreadId();
#ifdef NEXAUI_D3D12_TEST_HOOKS
    r->test_force_warp = GetEnvironmentVariableW(L"NEXAUI_TEST_FORCE_WARP", NULL, 0) != 0;
#endif
    nx_result result = nx_d3d12_build_objects(r);
    if (result != NX_OK) {
        free(r);
        return result;
    }
    *out_renderer = r;
    return NX_OK;
}

nx_result nx_d3d12_renderer_recover(nx_d3d12_renderer *r) {
    if (!r || GetCurrentThreadId() != r->owner_thread ||
        !IsWindow(r->hwnd) || !r->device_lost) return NX_ERROR_STATE;
    nx_d3d12_release_objects(r);
    nx_result result = nx_d3d12_build_objects(r);
    r->device_lost = result != NX_OK;
    return result;
}

static void nx_d3d12_copy_for_composite(nx_d3d12_renderer *r, UINT frame,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_RECT rect) {
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = r->targets[frame];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    D3D12_TEXTURE_COPY_LOCATION source = {0}, destination = {0};
    source.pResource = r->targets[frame];
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.pResource = r->copy_texture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_BOX box = {(UINT)rect.left, (UINT)rect.top, 0,
                     (UINT)rect.right, (UINT)rect.bottom, 1};
    ID3D12GraphicsCommandList_CopyTextureRegion(r->list, &destination,
        (UINT)rect.left, (UINT)rect.top, 0, &source, &box);
    UINT64 texels = (UINT64)(rect.right - rect.left) *
                    (UINT64)(rect.bottom - rect.top);
    if (UINT64_MAX - r->recording_copy_texels < texels)
        r->recording_copy_texels = UINT64_MAX;
    else
        r->recording_copy_texels += texels;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    barrier.Transition.pResource = r->copy_texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    ID3D12GraphicsCommandList_OMSetRenderTargets(r->list, 1, &rtv,
                                                   FALSE, NULL);
}

static void nx_d3d12_finish_composite(nx_d3d12_renderer *r) {
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = r->copy_texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
}

static nx_rect nx_d3d12_shape_bounds(const nx_command *command) {
    if (command->type == NX_COMMAND_LINE) {
        double r = (double)command->thickness / 2.0;
        double ax = command->points[0].x, ay = command->points[0].y;
        double bx = command->points[1].x, by = command->points[1].y;
        double left = fmin(ax, bx) - r, top = fmin(ay, by) - r;
        return (nx_rect){(float)left, (float)top,
            (float)(fmax(ax, bx) + r - left),
            (float)(fmax(ay, by) + r - top)};
    }
    if (command->type != NX_COMMAND_FILL_TRIANGLE) return command->rect;
    float left = command->points[0].x, right = left;
    float top = command->points[0].y, bottom = top;
    for (unsigned i = 1; i < 3; ++i) {
        if (command->points[i].x < left) left = command->points[i].x;
        if (command->points[i].x > right) right = command->points[i].x;
        if (command->points[i].y < top) top = command->points[i].y;
        if (command->points[i].y > bottom) bottom = command->points[i].y;
    }
    return (nx_rect){left, top, right - left, bottom - top};
}

static D3D12_RECT nx_d3d12_composite_copy_rect(const nx_command *cmd,
    double scale, D3D12_RECT clip, UINT width, UINT height) {
    nx_rect bounds = nx_d3d12_shape_bounds(cmd);
    D3D12_RECT area = {
        nx_d3d12_first_clip_pixel(bounds.x, scale, (LONG)width),
        nx_d3d12_first_clip_pixel(bounds.y, scale, (LONG)height),
        nx_d3d12_first_clip_pixel((double)bounds.x + bounds.width,
                                   scale, (LONG)width),
        nx_d3d12_first_clip_pixel((double)bounds.y + bounds.height,
                                   scale, (LONG)height)
    };
    if (area.left > 0) --area.left;
    if (area.top > 0) --area.top;
    if (area.right < (LONG)width) ++area.right;
    if (area.bottom < (LONG)height) ++area.bottom;
    area = nx_d3d12_intersect_clip(area, clip);
    return area.left < area.right && area.top < area.bottom ? area : clip;
}

static int nx_d3d12_bounds_overlap(nx_rect a, nx_rect b) {
    return (double)a.x < (double)b.x + b.width &&
           (double)b.x < (double)a.x + a.width &&
           (double)a.y < (double)b.y + b.height &&
           (double)b.y < (double)a.y + a.height;
}

static void nx_d3d12_flush_opaque_batch(nx_d3d12_renderer *r,
    D3D12_RECT clip, UINT first_vertex, UINT vertex_count) {
    if (!vertex_count) return;
    ID3D12GraphicsCommandList_RSSetScissorRects(r->list, 1, &clip);
    ID3D12GraphicsCommandList_DrawInstanced(r->list, vertex_count, 1,
                                             first_vertex, 0);
    ++r->recording_draw_calls;
}

static void nx_d3d12_flush_alpha_batch(nx_d3d12_renderer *r, UINT frame,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_RECT clip,
    D3D12_RECT copy_rect, UINT first_vertex, UINT vertex_count) {
    if (!vertex_count) return;
    nx_d3d12_copy_for_composite(r, frame, rtv, copy_rect);
    ID3D12DescriptorHeap *heaps[1] = {r->srv_heap};
    ID3D12GraphicsCommandList_SetDescriptorHeaps(r->list, 1, heaps);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(r->list,
                                                        r->composite_root);
    ID3D12GraphicsCommandList_SetPipelineState(r->list,
                                                r->composite_pipeline);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = {0};
    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(r->srv_heap,
                                                             &srv);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(r->list, 0,
                                                               srv);
    ID3D12GraphicsCommandList_RSSetScissorRects(r->list, 1, &clip);
    ID3D12GraphicsCommandList_DrawInstanced(r->list, vertex_count, 1,
                                             first_vertex, 0);
    ++r->recording_draw_calls;
    nx_d3d12_finish_composite(r);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(r->list,
                                                        r->root_signature);
    ID3D12GraphicsCommandList_SetPipelineState(r->list, r->pipeline);
}

static nx_result nx_d3d12_draw_present(nx_d3d12_renderer *r,
    const nx_command *commands, size_t count, double scale,
    nx_color clear_color, int *out_presented,
    uint8_t *pixels, size_t stride_bytes) {
    if (!out_presented) return NX_ERROR_ARGUMENT;
    *out_presented = 0;
    if (!r || !nx_d3d12_valid_color(clear_color) ||
        !isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    nx_result result = nx_render_validate_commands(commands, count);
    if (result != NX_OK) return result;
    if (GetCurrentThreadId() != r->owner_thread || !IsWindow(r->hwnd))
        return NX_ERROR_STATE;
    if (r->device_lost) return NX_ERROR_STATE;
    RECT client;
    if (!GetClientRect(r->hwnd, &client)) return NX_ERROR_PLATFORM;
    LONG width = client.right - client.left;
    LONG height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return NX_OK;
    if (pixels && ((size_t)width > SIZE_MAX / 4u ||
        stride_bytes < (size_t)width * 4u ||
        (size_t)(height - 1) >
            (SIZE_MAX - (size_t)width * 4u) / stride_bytes))
        return NX_ERROR_ARGUMENT;
    result = nx_d3d12_resize(r, (UINT)width, (UINT)height);
    if (result != NX_OK) return result;
    size_t bitmap_count = 0;
    int needs_composite = 0;
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type == NX_COMMAND_BITMAP) {
            ++bitmap_count;
            needs_composite = 1;
        } else if (commands[i].type == NX_COMMAND_LINE ||
                   commands[i].type == NX_COMMAND_FILL_ROUND_RECT) {
            needs_composite = 1;
        } else if (commands[i].type == NX_COMMAND_FILL_RECT ||
                   commands[i].type == NX_COMMAND_STROKE_RECT ||
                   commands[i].type == NX_COMMAND_FILL_TRIANGLE) {
            unsigned alpha = (unsigned)floorf(commands[i].color.a *
                                              255.0f + 0.5f);
            if (alpha > 0 && alpha < 255) needs_composite = 1;
        }
    }
    int async_frame = !needs_composite && !pixels;
    if ((!async_frame || r->pending_bitmaps || r->pending_readback) &&
        r->work_pending) {
        result = nx_d3d12_wait_gpu(r);
        if (result != NX_OK) return result;
    }
    if (r->pending_readback) {
        ID3D12Resource_Release(r->pending_readback);
        r->pending_readback = NULL;
    }
    UINT frame = IDXGISwapChain3_GetCurrentBackBufferIndex(r->swapchain);
    if (frame >= NX_D3D12_BUFFER_COUNT || !r->targets[frame])
        return NX_ERROR_PLATFORM;
    if (async_frame && r->fence_value >= 2u) {
        result = nx_d3d12_wait_for_value(r, r->fence_value - 1u);
        if (result != NX_OK) return result;
    }
    result = nx_d3d12_wait_for_value(r, r->frame_fence[frame]);
    if (result != NX_OK) return result;
    if (r->timestamp_heap) {
        result = nx_d3d12_collect_timing(r, frame,
                                         r->frame_fence[frame]);
        if (result != NX_OK) return result;
    }
    ID3D12Resource *readback = NULL;
    nx_d3d12_bitmap_gpu *bitmaps = NULL;
    size_t *bitmap_command_map = NULL;
    size_t unique_bitmap_count = 0;
    UINT vertex_count = 0;
    UINT64 vertex_offset = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {0};
    UINT64 readback_bytes = 0;
    result = nx_d3d12_upload_vertices(r, frame, commands, count, scale,
                                        &vertex_offset, &vertex_count);
    if (result != NX_OK) return result;
    if (needs_composite) {
        result = nx_d3d12_ensure_composite(r, frame);
        if (result != NX_OK) goto fail_before_submit;
    }
    for (size_t i = 0; i < count; ++i) {
        if (commands[i].type == NX_COMMAND_LINE ||
            commands[i].type == NX_COMMAND_FILL_ROUND_RECT) {
            result = nx_d3d12_ensure_shape_pipeline(r);
            if (result != NX_OK) goto fail_before_submit;
            break;
        }
    }
    if (bitmap_count) {
        result = nx_d3d12_ensure_bitmap_pipeline(r);
        if (result != NX_OK) goto fail_before_submit;
        result = nx_d3d12_prepare_bitmaps(r, commands, count, bitmap_count,
            &bitmaps, &unique_bitmap_count, &bitmap_command_map);
        if (result != NX_OK) goto fail_before_submit;
    }
    if (pixels) {
        result = nx_d3d12_create_readback(r, frame, &readback,
                                           &footprint, &readback_bytes);
        if (result != NX_OK) goto fail_before_submit;
    }
    HRESULT hr = ID3D12CommandAllocator_Reset(r->allocators[frame]);
    if (FAILED(hr)) { result = nx_d3d12_failure(r, hr); goto fail_before_submit; }
    hr = ID3D12GraphicsCommandList_Reset(r->list, r->allocators[frame], NULL);
    if (FAILED(hr)) { result = nx_d3d12_failure(r, hr); goto fail_before_submit; }
    r->recording_draw_calls = 0;
    r->recording_copy_texels = 0;
    if (r->timestamp_heap)
        ID3D12GraphicsCommandList_EndQuery(r->list, r->timestamp_heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            frame * NX_D3D12_TIMESTAMPS_PER_FRAME);
    for (size_t i = 0; i < unique_bitmap_count; ++i) {
        if (bitmaps[i].needs_copy_transition) {
            D3D12_RESOURCE_BARRIER reuse_barrier = {0};
            reuse_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            reuse_barrier.Transition.pResource = bitmaps[i].texture;
            reuse_barrier.Transition.Subresource =
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            reuse_barrier.Transition.StateBefore =
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            reuse_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1,
                                                        &reuse_barrier);
        }
        D3D12_TEXTURE_COPY_LOCATION source = {0}, destination = {0};
        source.pResource = r->bitmap_upload;
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint = bitmaps[i].footprint;
        destination.pResource = bitmaps[i].texture;
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        ID3D12GraphicsCommandList_CopyTextureRegion(r->list, &destination,
            0, 0, 0, &source, NULL);
        D3D12_RESOURCE_BARRIER texture_barrier = {0};
        texture_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        texture_barrier.Transition.pResource = bitmaps[i].texture;
        texture_barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        texture_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        texture_barrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1,
                                                    &texture_barrier);
    }
    if (r->timestamp_heap)
        ID3D12GraphicsCommandList_EndQuery(r->list, r->timestamp_heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            frame * NX_D3D12_TIMESTAMPS_PER_FRAME + 1u);
    D3D12_RESOURCE_BARRIER barrier = {0};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = r->targets[frame];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {0};
    ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(r->rtv_heap,
                                                             &handle);
    handle.ptr += (SIZE_T)frame * r->rtv_stride;
    ID3D12GraphicsCommandList_OMSetRenderTargets(r->list, 1, &handle,
                                                   FALSE, NULL);
    FLOAT rgba[4] = {
        floorf(clear_color.r * 255.0f + 0.5f) / 255.0f,
        floorf(clear_color.g * 255.0f + 0.5f) / 255.0f,
        floorf(clear_color.b * 255.0f + 0.5f) / 255.0f,
        floorf(clear_color.a * 255.0f + 0.5f) / 255.0f
    };
    ID3D12GraphicsCommandList_ClearRenderTargetView(r->list, handle,
                                                     rgba, 0, NULL);
    if (vertex_count) {
        D3D12_VIEWPORT viewport = {0, 0, (float)r->width,
                                   (float)r->height, 0, 1};
        D3D12_RECT clips[NX_MAX_CLIP_DEPTH + 1u] = {0};
        clips[0] = (D3D12_RECT){0, 0, (LONG)r->width, (LONG)r->height};
        unsigned depth = 0;
        UINT first_vertex = 0;
        size_t bitmap_index = 0;
        nx_rect batch_bounds[NX_D3D12_OPAQUE_BATCH_MAX];
        unsigned batch_count = 0;
        UINT batch_first = 0, batch_vertices = 0;
        D3D12_RECT batch_clip = {0};
        nx_rect alpha_bounds[NX_D3D12_OPAQUE_BATCH_MAX];
        unsigned alpha_count = 0;
        UINT alpha_first = 0, alpha_vertices = 0;
        D3D12_RECT alpha_clip = {0}, alpha_copy = {0};
        D3D12_VERTEX_BUFFER_VIEW view = {0};
        view.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(
            r->vertex_ring[frame]) + vertex_offset;
        view.SizeInBytes = vertex_count * (UINT)sizeof(nx_d3d12_vertex);
        view.StrideInBytes = sizeof(nx_d3d12_vertex);
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(r->list,
                                                            r->root_signature);
        ID3D12GraphicsCommandList_SetPipelineState(r->list, r->pipeline);
        ID3D12GraphicsCommandList_RSSetViewports(r->list, 1, &viewport);
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(r->list,
                                           D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12GraphicsCommandList_IASetVertexBuffers(r->list, 0, 1, &view);
        for (size_t i = 0; i < count; ++i) {
            const nx_command *cmd = &commands[i];
            if (cmd->type == NX_COMMAND_PUSH_CLIP) {
                nx_d3d12_flush_opaque_batch(r, batch_clip,
                                              batch_first, batch_vertices);
                batch_count = batch_vertices = 0;
                nx_d3d12_flush_alpha_batch(r, frame, handle, alpha_clip,
                    alpha_copy, alpha_first, alpha_vertices);
                alpha_count = alpha_vertices = 0;
                D3D12_RECT next = nx_d3d12_command_clip(cmd, scale,
                                                         r->width, r->height);
                clips[depth + 1u] = nx_d3d12_intersect_clip(clips[depth],
                                                              next);
                ++depth;
            } else if (cmd->type == NX_COMMAND_POP_CLIP) {
                nx_d3d12_flush_opaque_batch(r, batch_clip,
                                              batch_first, batch_vertices);
                batch_count = batch_vertices = 0;
                nx_d3d12_flush_alpha_batch(r, frame, handle, alpha_clip,
                    alpha_copy, alpha_first, alpha_vertices);
                alpha_count = alpha_vertices = 0;
                --depth;
            } else {
                UINT draw_count = nx_d3d12_draw_vertex_count(cmd->type);
                D3D12_RECT clip = clips[depth];
                unsigned alpha = (unsigned)floorf(cmd->color.a * 255.0f + 0.5f);
                int drawable = alpha && clip.left < clip.right &&
                    clip.top < clip.bottom;
                if (cmd->type == NX_COMMAND_BITMAP &&
                    (!cmd->rect.width || !cmd->rect.height)) drawable = 0;
                if (cmd->type == NX_COMMAND_STROKE_RECT &&
                    (!cmd->rect.width || !cmd->rect.height)) drawable = 0;
                if (cmd->type == NX_COMMAND_FILL_ROUND_RECT &&
                    (!cmd->rect.width || !cmd->rect.height)) drawable = 0;
                if (drawable && alpha == 255 &&
                    cmd->type != NX_COMMAND_BITMAP &&
                    cmd->type != NX_COMMAND_LINE &&
                    cmd->type != NX_COMMAND_FILL_ROUND_RECT) {
                    nx_d3d12_flush_alpha_batch(r, frame, handle, alpha_clip,
                        alpha_copy, alpha_first, alpha_vertices);
                    alpha_count = alpha_vertices = 0;
                    nx_rect bounds = nx_d3d12_shape_bounds(cmd);
                    int overlaps = batch_count == NX_D3D12_OPAQUE_BATCH_MAX;
                    for (unsigned b = 0; b < batch_count && !overlaps; ++b)
                        overlaps = nx_d3d12_bounds_overlap(bounds,
                                                             batch_bounds[b]);
                    if (overlaps) {
                        nx_d3d12_flush_opaque_batch(r, batch_clip,
                            batch_first, batch_vertices);
                        batch_count = batch_vertices = 0;
                    }
                    if (!batch_count) {
                        batch_first = first_vertex;
                        batch_clip = clip;
                    }
                    batch_bounds[batch_count++] = bounds;
                    batch_vertices += draw_count;
                    first_vertex += draw_count;
                    continue;
                }
                if (drawable && cmd->type != NX_COMMAND_BITMAP &&
                    cmd->type != NX_COMMAND_LINE &&
                    cmd->type != NX_COMMAND_FILL_ROUND_RECT &&
                    alpha < 255u) {
                    nx_d3d12_flush_opaque_batch(r, batch_clip,
                        batch_first, batch_vertices);
                    batch_count = batch_vertices = 0;
                    nx_rect bounds = nx_d3d12_shape_bounds(cmd);
                    int overlaps = alpha_count == NX_D3D12_OPAQUE_BATCH_MAX;
                    for (unsigned b = 0; b < alpha_count && !overlaps; ++b)
                        overlaps = nx_d3d12_bounds_overlap(bounds,
                                                            alpha_bounds[b]);
                    if (overlaps) {
                        nx_d3d12_flush_alpha_batch(r, frame, handle,
                            alpha_clip, alpha_copy, alpha_first,
                            alpha_vertices);
                        alpha_count = alpha_vertices = 0;
                    }
                    D3D12_RECT copy = nx_d3d12_composite_copy_rect(cmd,
                        scale, clip, r->width, r->height);
                    if (!alpha_count) {
                        alpha_first = first_vertex;
                        alpha_clip = clip;
                        alpha_copy = copy;
                    } else {
                        if (copy.left < alpha_copy.left)
                            alpha_copy.left = copy.left;
                        if (copy.top < alpha_copy.top)
                            alpha_copy.top = copy.top;
                        if (copy.right > alpha_copy.right)
                            alpha_copy.right = copy.right;
                        if (copy.bottom > alpha_copy.bottom)
                            alpha_copy.bottom = copy.bottom;
                    }
                    alpha_bounds[alpha_count++] = bounds;
                    alpha_vertices += draw_count;
                    first_vertex += draw_count;
                    continue;
                }
                nx_d3d12_flush_opaque_batch(r, batch_clip,
                                              batch_first, batch_vertices);
                batch_count = batch_vertices = 0;
                nx_d3d12_flush_alpha_batch(r, frame, handle, alpha_clip,
                    alpha_copy, alpha_first, alpha_vertices);
                alpha_count = alpha_vertices = 0;
                if (drawable && (cmd->type == NX_COMMAND_LINE ||
                    cmd->type == NX_COMMAND_FILL_ROUND_RECT)) {
                    D3D12_RECT copy_rect = nx_d3d12_composite_copy_rect(
                        cmd, scale, clip, r->width, r->height);
                    nx_d3d12_copy_for_composite(r, frame, handle, copy_rect);
                    ID3D12DescriptorHeap *heaps[1] = {r->srv_heap};
                    ID3D12GraphicsCommandList_SetDescriptorHeaps(r->list, 1,
                                                                   heaps);
                    ID3D12GraphicsCommandList_SetGraphicsRootSignature(
                        r->list, r->shape_root);
                    ID3D12GraphicsCommandList_SetPipelineState(r->list,
                        r->shape_pipeline);
                    D3D12_GPU_DESCRIPTOR_HANDLE srv = {0};
                    ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
                        r->srv_heap, &srv);
                    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
                        r->list, 0, srv);
                    struct { float bounds[4]; float radius; UINT kind; }
                        shape = {0};
                    if (cmd->type == NX_COMMAND_LINE) {
                        shape.bounds[0] = (float)(cmd->points[0].x * scale);
                        shape.bounds[1] = (float)(cmd->points[0].y * scale);
                        shape.bounds[2] = (float)(cmd->points[1].x * scale);
                        shape.bounds[3] = (float)(cmd->points[1].y * scale);
                        shape.radius = (float)(cmd->thickness * scale / 2.0);
                    } else {
                        double radius = fmin(cmd->radius,
                            fmin(cmd->rect.width, cmd->rect.height) / 2.0);
                        shape.bounds[0] = (float)(cmd->rect.x * scale);
                        shape.bounds[1] = (float)(cmd->rect.y * scale);
                        shape.bounds[2] = (float)((cmd->rect.x +
                            cmd->rect.width) * scale);
                        shape.bounds[3] = (float)((cmd->rect.y +
                            cmd->rect.height) * scale);
                        shape.radius = (float)(radius * scale);
                        shape.kind = 1u;
                    }
                    ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(
                        r->list, 1, 6, &shape, 0);
                    ID3D12GraphicsCommandList_RSSetScissorRects(r->list, 1,
                                                                  &clip);
                    ID3D12GraphicsCommandList_DrawInstanced(r->list,
                        draw_count, 1, first_vertex, 0);
                    ++r->recording_draw_calls;
                    nx_d3d12_finish_composite(r);
                    ID3D12GraphicsCommandList_SetGraphicsRootSignature(
                        r->list, r->root_signature);
                    ID3D12GraphicsCommandList_SetPipelineState(r->list,
                        r->pipeline);
                    first_vertex += draw_count;
                    continue;
                }
                if (drawable) {
                    ID3D12GraphicsCommandList_RSSetScissorRects(r->list, 1,
                                                                  &clip);
                    if (cmd->type == NX_COMMAND_BITMAP || alpha < 255) {
                        D3D12_RECT copy_rect =
                            nx_d3d12_composite_copy_rect(cmd, scale, clip,
                                r->width, r->height);
                        nx_d3d12_copy_for_composite(r, frame, handle,
                                                       copy_rect);
                        ID3D12DescriptorHeap *heap = cmd->type ==
                            NX_COMMAND_BITMAP ? r->bitmap_srv_heap :
                            r->srv_heap;
                        ID3D12DescriptorHeap *heaps[1] = {heap};
                        ID3D12GraphicsCommandList_SetDescriptorHeaps(r->list,
                                                                       1, heaps);
                        ID3D12GraphicsCommandList_SetGraphicsRootSignature(
                            r->list, cmd->type == NX_COMMAND_BITMAP ?
                            r->bitmap_root : r->composite_root);
                        ID3D12GraphicsCommandList_SetPipelineState(r->list,
                            cmd->type == NX_COMMAND_BITMAP ?
                            r->bitmap_pipeline : r->composite_pipeline);
                        D3D12_GPU_DESCRIPTOR_HANDLE srv = {0};
                        ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(
                            heap, &srv);
                        if (cmd->type == NX_COMMAND_BITMAP) {
                            UINT stride = ID3D12Device_GetDescriptorHandleIncrementSize(
                                r->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                            srv.ptr += (UINT64)(2u * bitmap_command_map[bitmap_index]) *
                                       stride;
                        }
                        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(
                            r->list, 0, srv);
                        if (cmd->type == NX_COMMAND_BITMAP) {
                            struct {
                                float bounds[4];
                                uint32_t format;
                            } constants = {
                                {(float)(cmd->rect.x * scale),
                                 (float)(cmd->rect.y * scale),
                                 (float)(cmd->rect.width * scale),
                                 (float)(cmd->rect.height * scale)},
                                (uint32_t)cmd->bitmap.format
                            };
                            ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(
                                r->list, 1, 5, &constants, 0);
                        }
                    }
                    ID3D12GraphicsCommandList_DrawInstanced(r->list,
                        draw_count, 1, first_vertex, 0);
                    ++r->recording_draw_calls;
                    if (cmd->type == NX_COMMAND_BITMAP || alpha < 255) {
                        nx_d3d12_finish_composite(r);
                        ID3D12GraphicsCommandList_SetGraphicsRootSignature(
                            r->list, r->root_signature);
                        ID3D12GraphicsCommandList_SetPipelineState(r->list,
                                                                     r->pipeline);
                    }
                }
                first_vertex += draw_count;
                if (cmd->type == NX_COMMAND_BITMAP) ++bitmap_index;
            }
        }
        nx_d3d12_flush_opaque_batch(r, batch_clip,
                                      batch_first, batch_vertices);
        nx_d3d12_flush_alpha_batch(r, frame, handle, alpha_clip,
            alpha_copy, alpha_first, alpha_vertices);
    }
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = pixels ? D3D12_RESOURCE_STATE_COPY_SOURCE
                                           : D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    if (r->timestamp_heap)
        ID3D12GraphicsCommandList_EndQuery(r->list, r->timestamp_heap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            frame * NX_D3D12_TIMESTAMPS_PER_FRAME + 2u);
    if (pixels) {
        D3D12_TEXTURE_COPY_LOCATION source = {0}, destination = {0};
        source.pResource = r->targets[frame];
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = readback;
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        ID3D12GraphicsCommandList_CopyTextureRegion(r->list,
            &destination, 0, 0, 0, &source, NULL);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        ID3D12GraphicsCommandList_ResourceBarrier(r->list, 1, &barrier);
    }
    if (r->timestamp_heap)
        ID3D12GraphicsCommandList_ResolveQueryData(r->list,
            r->timestamp_heap, D3D12_QUERY_TYPE_TIMESTAMP,
            frame * NX_D3D12_TIMESTAMPS_PER_FRAME,
            NX_D3D12_TIMESTAMPS_PER_FRAME,
            r->timestamp_readback[frame], 0);
    hr = ID3D12GraphicsCommandList_Close(r->list);
    if (FAILED(hr)) { result = nx_d3d12_failure(r, hr); goto fail_before_submit; }
    free(bitmap_command_map);
    bitmap_command_map = NULL;
    ID3D12CommandList *lists[1] = {(ID3D12CommandList *)r->list};
    ID3D12CommandQueue_ExecuteCommandLists(r->queue, 1, lists);
    for (size_t i = 0; i < r->texture_pool_count; ++i)
        if (r->texture_pool[i].used) r->texture_pool[i].shader_state = 1;
    r->pending_readback = readback;
    r->pending_bitmaps = bitmaps;
    r->pending_bitmap_count = unique_bitmap_count;
    readback = NULL;
    bitmaps = NULL;
    r->work_pending = 1;
    hr = IDXGISwapChain3_Present(r->swapchain, 1, 0);
    UINT64 frame_value = 0;
    result = nx_d3d12_signal_gpu(r, &frame_value);
    if (result != NX_OK) return result;
    r->frame_fence[frame] = frame_value;
    r->last_frame = frame;
    if (r->timestamp_heap)
        r->timestamp_frame[frame] = ++r->timestamp_sequence;
    if (FAILED(hr)) return nx_d3d12_failure(r, hr);
    if (!async_frame) {
        result = nx_d3d12_wait_gpu(r);
        if (result != NX_OK) return result;
    }
    ++r->submitted_frames;
    r->last_draw_calls = r->recording_draw_calls;
    r->last_bitmap_uploads = unique_bitmap_count;
    r->last_copy_texels = r->recording_copy_texels;
    *out_presented = hr == S_OK;
    if (pixels) {
        D3D12_RANGE read_range = {0, (SIZE_T)readback_bytes};
        const uint8_t *mapped = NULL;
        hr = ID3D12Resource_Map(r->pending_readback, 0,
                                &read_range, (void **)&mapped);
        if (FAILED(hr)) return nx_d3d12_failure(r, hr);
        for (LONG y = 0; y < height; ++y) {
            memcpy(pixels + (size_t)y * stride_bytes,
                mapped + footprint.Offset + (size_t)y * footprint.Footprint.RowPitch,
                (size_t)width * 4u);
        }
        D3D12_RANGE no_write = {0, 0};
        ID3D12Resource_Unmap(r->pending_readback, 0, &no_write);
        ID3D12Resource_Release(r->pending_readback);
        r->pending_readback = NULL;
    }
    return NX_OK;
fail_before_submit:
    free(bitmap_command_map);
    if (readback) ID3D12Resource_Release(readback);
    nx_d3d12_release_bitmaps(bitmaps, unique_bitmap_count);
    return result;
}

nx_result nx_d3d12_clear_present(nx_d3d12_renderer *r,
                                  nx_color clear_color,
                                  int *out_presented) {
    return nx_d3d12_draw_present(r, NULL, 0, 1.0, clear_color,
                                 out_presented, NULL, 0);
}

nx_result nx_d3d12_renderer_get_info(nx_d3d12_renderer *r,
                                     nx_d3d12_renderer_info *out_info) {
    if (!out_info) return NX_ERROR_ARGUMENT;
    *out_info = (nx_d3d12_renderer_info){0};
    if (!r) return NX_ERROR_ARGUMENT;
    if (GetCurrentThreadId() != r->owner_thread || !IsWindow(r->hwnd))
        return NX_ERROR_STATE;
    if (r->timestamp_heap && !r->device_lost) {
        UINT64 completed = ID3D12Fence_GetCompletedValue(r->fence);
        if (completed == UINT64_MAX) {
            r->device_lost = 1;
            return NX_ERROR_STATE;
        }
        for (unsigned i = 0; i < NX_D3D12_BUFFER_COUNT; ++i) {
            nx_result result = nx_d3d12_collect_timing(r, i, completed);
            if (result != NX_OK) return result;
        }
    }
    *out_info = (nx_d3d12_renderer_info){
        .pixel_width = r->width,
        .pixel_height = r->height,
        .submitted_frames = r->submitted_frames,
        .last_draw_calls = r->last_draw_calls,
        .last_bitmap_uploads = r->last_bitmap_uploads,
        .last_composite_copy_texels = r->last_copy_texels,
        .texture_creations = r->texture_creations,
        .vertex_ring_allocations = r->vertex_ring_allocations,
        .vertex_ring_wraps = r->vertex_ring_wraps,
        .vertex_ring_capacity = r->vertex_ring_capacity[r->last_frame],
        .vertex_ring_head = r->vertex_ring_head[r->last_frame],
        .last_timed_frame = r->last_timed_frame,
        .gpu_timestamp_frequency = r->timestamp_frequency,
        .last_gpu_upload_ms = r->last_gpu_upload_ms,
        .last_gpu_render_ms = r->last_gpu_render_ms,
        .last_gpu_frame_ms = r->last_gpu_frame_ms,
        .pooled_textures = (uint32_t)r->texture_pool_count,
        .using_warp = r->using_warp
    };
    return NX_OK;
}

static nx_result nx_d3d12_backend_resize(void *user,
                                         nx_render_target target,
                                         nx_render_outcome *out_outcome) {
    nx_d3d12_renderer *renderer = user;
    *out_outcome = NX_RENDER_SUSPENDED;
    if (renderer->device_lost) {
        *out_outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (!target.pixel_width || !target.pixel_height) {
        *out_outcome = NX_RENDER_READY;
        return NX_OK;
    }
    RECT client;
    if (!GetClientRect(renderer->hwnd, &client)) return NX_ERROR_PLATFORM;
    if ((UINT)(client.right - client.left) != target.pixel_width ||
        (UINT)(client.bottom - client.top) != target.pixel_height)
        return NX_OK; /* A newer window size will be supplied next frame. */
    nx_result result = nx_d3d12_resize(renderer, target.pixel_width,
                                        target.pixel_height);
    if (renderer->device_lost) {
        *out_outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (result == NX_OK) *out_outcome = NX_RENDER_READY;
    return result;
}

static nx_result nx_d3d12_backend_submit(void *user,
                                         nx_render_target target,
                                         const nx_command *commands,
                                         size_t count, nx_color clear,
                                         nx_render_outcome *out_outcome) {
    *out_outcome = NX_RENDER_SUSPENDED;
    nx_d3d12_renderer *renderer = user;
    if (renderer->device_lost) {
        *out_outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    RECT client;
    if (!GetClientRect(renderer->hwnd, &client)) return NX_ERROR_PLATFORM;
    if ((UINT)(client.right - client.left) != target.pixel_width ||
        (UINT)(client.bottom - client.top) != target.pixel_height)
        return NX_OK;
    int presented = 0;
    nx_result result = nx_d3d12_draw_present(renderer, commands, count,
                                              target.scale, clear,
                                              &presented, NULL, 0);
    if (renderer->device_lost) {
        *out_outcome = NX_RENDER_DEVICE_LOST;
        return NX_OK;
    }
    if (result == NX_OK) *out_outcome = NX_RENDER_READY;
    return result;
}

static nx_result nx_d3d12_backend_recover(void *user) {
    return nx_d3d12_renderer_recover(user);
}

static nx_result nx_d3d12_backend_destroy(void *user) {
    nx_d3d12_renderer_destroy(user);
    return NX_OK;
}

nx_result nx_d3d12_driver_open(nx_window *window,
                                nx_renderer_driver *out_driver) {
    if (!out_driver) return NX_ERROR_ARGUMENT;
    nx_d3d12_renderer *renderer = NULL;
    nx_result result = nx_d3d12_renderer_create(window, &renderer);
    if (result != NX_OK) return result;
    nx_render_backend backend = {renderer, nx_d3d12_backend_resize,
        nx_d3d12_backend_submit, nx_d3d12_backend_recover,
        nx_d3d12_backend_destroy};
    result = nx_renderer_open(out_driver, &backend);
    if (result != NX_OK) nx_d3d12_renderer_destroy(renderer);
    return result;
}

#ifdef NEXAUI_D3D12_TEST_HOOKS
nx_result nx_d3d12_test_remove_device(nx_d3d12_renderer *r) {
    if (!r || GetCurrentThreadId() != r->owner_thread || !r->device ||
        r->device_lost) return NX_ERROR_STATE;
    ID3D12Device5 *device5 = NULL;
    HRESULT hr = ID3D12Device_QueryInterface(r->device,
        &IID_ID3D12Device5, (void **)&device5);
    if (FAILED(hr)) return NX_ERROR_UNSUPPORTED;
    /* Deterministic test removal: finish this renderer's prior work before
       faulting the device. WARP can otherwise still be executing on a worker
       when recovery releases its objects. This is not a physical-loss test. */
    nx_result retired = nx_d3d12_wait_gpu(r);
    if (retired != NX_OK) { ID3D12Device5_Release(device5); return retired; }
    ID3D12Device5_RemoveDevice(device5);
    ID3D12Device5_Release(device5);
    return NX_OK;
}

nx_result nx_d3d12_test_fail_next_rebuild(nx_d3d12_renderer *r) {
    if (!r || GetCurrentThreadId() != r->owner_thread || !r->device_lost)
        return NX_ERROR_STATE;
    r->test_fail_next_rebuild = 1;
    return NX_OK;
}

nx_result nx_d3d12_test_render_readback(nx_d3d12_renderer *r,
    const nx_command *commands, size_t count, double scale,
    nx_color clear, uint8_t *pixels, size_t stride_bytes) {
    if (!pixels) return NX_ERROR_ARGUMENT;
    int presented = 0;
    return nx_d3d12_draw_present(r, commands, count, scale, clear,
                                  &presented, pixels, stride_bytes);
}
#endif
