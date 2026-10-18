/* Compile the exact backend with named D3D12 calls wrapped for failure injection.
   Rendering and COM objects remain real; no replacement device/vtable is used. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer.h"

enum fault { FAULT_NONE, FAULT_ROOT, FAULT_PSO, FAULT_BYTECODE, FAULT_DEVICE };
static enum fault injected;
static unsigned root_calls,pso_calls,root_refs,pso_refs;

static HRESULT root_create(ID3D12Device *device,UINT node,const void *blob,SIZE_T bytes,REFIID iid,void **out) {
    ++root_calls;
    if (injected==FAULT_ROOT) { injected=FAULT_NONE; *out=NULL; return E_OUTOFMEMORY; }
    HRESULT hr=ID3D12Device_CreateRootSignature(device,node,blob,bytes,iid,out);
    if (SUCCEEDED(hr)) ++root_refs;
    return hr;
}
static HRESULT pipeline_create(ID3D12Device *device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC *source,REFIID iid,void **out) {
    ++pso_calls;
    enum fault fault=injected; injected=FAULT_NONE;
    if (fault==FAULT_PSO) { *out=NULL; return E_OUTOFMEMORY; }
    if (fault==FAULT_DEVICE) {
        ID3D12Device5 *device5=NULL;
        HRESULT hr=ID3D12Device_QueryInterface(device,&IID_ID3D12Device5,(void **)&device5);
        if (FAILED(hr)) { *out=NULL; return E_NOINTERFACE; }
        ID3D12Device5_RemoveDevice(device5); ID3D12Device5_Release(device5);
        *out=NULL; return DXGI_ERROR_DEVICE_REMOVED;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc=*source;
    static const unsigned char invalid_dxbc[4]={'D','X','B','C'};
    if (fault==FAULT_BYTECODE) desc.PS=(D3D12_SHADER_BYTECODE){invalid_dxbc,sizeof(invalid_dxbc)};
    HRESULT hr=ID3D12Device_CreateGraphicsPipelineState(device,&desc,iid,out);
    if (SUCCEEDED(hr)) ++pso_refs;
    return hr;
}
static ULONG root_release(ID3D12RootSignature *root) {
    if (!root_refs) abort();
    --root_refs; return ID3D12RootSignature_Release(root);
}
static ULONG pipeline_release(ID3D12PipelineState *pipeline) {
    if (!pso_refs) abort();
    --pso_refs; return ID3D12PipelineState_Release(pipeline);
}
#undef ID3D12Device_CreateRootSignature
#define ID3D12Device_CreateRootSignature root_create
#undef ID3D12Device_CreateGraphicsPipelineState
#define ID3D12Device_CreateGraphicsPipelineState pipeline_create
#undef ID3D12RootSignature_Release
#define ID3D12RootSignature_Release root_release
#undef ID3D12PipelineState_Release
#define ID3D12PipelineState_Release pipeline_release
#include "../renderers/d3d12/renderer.c"

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"pipeline:%d: %s\n",__LINE__,#condition); return 0; } } while (0)

static nx_result prepare(nx_d3d12_renderer *renderer,unsigned kind) {
    switch (kind) {
    case 0:return nx_d3d12_ensure_pipeline(renderer);
    case 1:return nx_d3d12_ensure_composite(renderer,0);
    case 2:return nx_d3d12_ensure_shape_pipeline(renderer);
    default:return nx_d3d12_ensure_bitmap_pipeline(renderer);
    }
}
static int failed_attempt(nx_window *window,unsigned kind,enum fault fault) {
    nx_d3d12_renderer *renderer=NULL;
    CHECK(!root_refs && !pso_refs && nx_d3d12_renderer_create(window,&renderer)==NX_OK);
    injected=fault;
    CHECK(prepare(renderer,kind)==NX_ERROR_PLATFORM);
    CHECK(injected==FAULT_NONE && !root_refs && !pso_refs && !renderer->device_lost && !renderer->submitted_frames);
    CHECK(!renderer->pipeline && !renderer->root_signature && !renderer->composite_pipeline && !renderer->composite_root &&
          !renderer->shape_pipeline && !renderer->shape_root && !renderer->bitmap_pipeline && !renderer->bitmap_root);
    CHECK(prepare(renderer,kind)==NX_OK && root_refs==1u && pso_refs==1u);
    unsigned roots=root_calls,pipelines=pso_calls;
    CHECK(prepare(renderer,kind)==NX_OK && root_calls==roots && pso_calls==pipelines);
    nx_d3d12_renderer_destroy(renderer);
    CHECK(!root_refs && !pso_refs);
    return 1;
}
static int removed_during_pipeline(nx_window *window) {
    nx_d3d12_renderer *renderer=NULL;
    CHECK(nx_d3d12_renderer_create(window,&renderer)==NX_OK);
    ID3D12Device5 *supported=NULL;
    HRESULT hr=ID3D12Device_QueryInterface(renderer->device,&IID_ID3D12Device5,(void **)&supported);
    if (FAILED(hr)) {
        puts("ID3D12Device5 unavailable: creation-time removal case not executed");
        nx_d3d12_renderer_destroy(renderer); return 1;
    }
    ID3D12Device5_Release(supported);
    injected=FAULT_DEVICE;
    CHECK(prepare(renderer,3)==NX_ERROR_STATE && renderer->device_lost && !root_refs && !pso_refs);
    int presented=99;
    CHECK(nx_d3d12_clear_present(renderer,(nx_color){0,0,0,1},&presented)==NX_ERROR_STATE && !presented);
    CHECK(nx_d3d12_test_fail_next_rebuild(renderer)==NX_OK);
    CHECK(nx_d3d12_renderer_recover(renderer)==NX_ERROR_PLATFORM && renderer->device_lost && !root_refs && !pso_refs);
    CHECK(nx_d3d12_renderer_recover(renderer)==NX_OK);
    for (unsigned kind=0;kind<4;++kind) CHECK(prepare(renderer,kind)==NX_OK);
    CHECK(root_refs==4u && pso_refs==4u);
    CHECK(nx_d3d12_clear_present(renderer,(nx_color){0,0,0,1},&presented)==NX_OK && presented);
    nx_d3d12_renderer_destroy(renderer);
    CHECK(!root_refs && !pso_refs);
    puts("Actual creation-time device removal, partial rebuild failure and retry passed");
    return 1;
}
int main(void) {
    nx_window_desc desc={"NexaUI D3D12 pipeline failure tests",96,64,NX_WINDOW_HIDDEN,{0}};
    nx_window *window=NULL;
    if (nx_window_create(&desc,&window)!=NX_OK) return 1;
    for (unsigned kind=0;kind<4;++kind) {
        for (enum fault fault=FAULT_ROOT;fault<=FAULT_BYTECODE;++fault)
            if (!failed_attempt(window,kind,fault)) return 1;
    }
    if (!removed_during_pipeline(window)) return 1;
    nx_window_destroy(window);
    puts("All four native pipelines: root/PSO/invalid-shader rollback, retry and idempotence passed");
    return 0;
}
