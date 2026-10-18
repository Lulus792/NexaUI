/* Native startup diagnostic: entry, device/PSO work, first composed frame,
   actual owned-HWND input acknowledgment and normal frames. This translation
   unit wraps named APIs in the exact renderer; no production vtable is replaced.
   Run Release builds for timing. The window is briefly visible without activation. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define COBJMACROS
#define UNICODE
#define NEXAUI_D3D12_TEST_HOOKS
#include <windows.h>
#include <d3d12.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4115)
#endif
#include <d3dcompiler.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <dxgi1_4.h>
#include <dwmapi.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "scene.h"
#include "window_internal.h"
typedef struct span_record {
 const char *kind; LONGLONG begin,end,api_ticks; HRESULT hr;
 size_t bytes; uint64_t key;
} span_record;
static span_record spans[32];static unsigned span_count;
static LARGE_INTEGER probe_frequency,entry_tick;

static LONGLONG tick(void){LARGE_INTEGER n;QueryPerformanceCounter(&n);return n.QuadPart;}
static double ms(LONGLONG n){return 1000.0*(double)n/(double)probe_frequency.QuadPart;}
static uint64_t hash_bytes(uint64_t h,const void *bytes,size_t length){
 const unsigned char *p=bytes;for(size_t i=0;i<length;++i)h=(h^p[i])*UINT64_C(1099511628211);return h;
}
static HRESULT WINAPI measured_compile(const void *data,SIZE_T bytes,const char *name,
 const D3D_SHADER_MACRO *defines,ID3DInclude *include,const char *entry,const char *target,
 UINT flags1,UINT flags2,ID3DBlob **code,ID3DBlob **errors){
 LONGLONG begin=tick();HRESULT hr=D3DCompile(data,bytes,name,defines,include,entry,target,flags1,flags2,code,errors);
 if(span_count<32)spans[span_count++]=(span_record){target,begin,tick(),0,hr,0,0};return hr;
}
static HRESULT WINAPI measured_device(IUnknown *adapter,D3D_FEATURE_LEVEL level,REFIID iid,void **out){
 LONGLONG begin=tick();HRESULT hr=D3D12CreateDevice(adapter,level,iid,out);
 if(span_count<32)spans[span_count++]=(span_record){"CreateDevice",begin,tick(),0,hr,0,0};return hr;
}
static HRESULT WINAPI measured_factory(UINT flags,REFIID iid,void **out){
 LONGLONG begin=tick();HRESULT hr=CreateDXGIFactory2(flags,iid,out);
 if(span_count<32)spans[span_count++]=(span_record){"CreateFactory",begin,tick(),0,hr,0,0};return hr;
}
static HRESULT measured_pipeline(ID3D12Device *device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc,
 REFIID iid,void **out,unsigned line){
 (void)line;
 LONGLONG begin=tick();
 uint64_t key=hash_bytes(UINT64_C(14695981039346656037),desc->VS.pShaderBytecode,desc->VS.BytecodeLength);
 key=hash_bytes(key,desc->PS.pShaderBytecode,desc->PS.BytecodeLength);
 LONGLONG api_begin=tick();HRESULT hr=ID3D12Device_CreateGraphicsPipelineState(device,desc,iid,out);
 LONGLONG end=tick();
 if(span_count<32)spans[span_count++]=(span_record){"PSO",begin,end,end-api_begin,hr,
     desc->VS.BytecodeLength+desc->PS.BytecodeLength,key};
 return hr;
}
#define D3DCompile measured_compile
#define D3D12CreateDevice measured_device
#define CreateDXGIFactory2 measured_factory
#undef ID3D12Device_CreateGraphicsPipelineState
#define ID3D12Device_CreateGraphicsPipelineState(device,desc,iid,out) measured_pipeline(device,desc,iid,out,__LINE__)
#ifndef NEXAUI_D3D12_STARTUP_RENDERER_SOURCE
#define NEXAUI_D3D12_STARTUP_RENDERER_SOURCE "../renderers/d3d12/renderer.c"
#endif
#include NEXAUI_D3D12_STARTUP_RENDERER_SOURCE
#undef D3DCompile

static int render_frame(nx_window *window,cloud_scene *scene,nx_renderer_driver *driver,
 LONGLONG *cpu_ticks,LONGLONG *render_ticks,size_t *command_count){
 LONGLONG before=tick();nx_window_info info;size_t routed;
 if(nx_window_poll(window,0)!=NX_OK || nx_window_get_info(window,&info)!=NX_OK ||
  nx_window_route_events(window,cloud_context(scene),&routed)!=NX_OK ||
  !cloud_set_pixel_scale(scene,info.scale) || !cloud_frame(scene,info.width,info.height,0) ||
  nx_window_publish_semantics(window,cloud_context(scene))!=NX_OK)return 0;
 const nx_command *commands;size_t count;RECT rect;
 if(nx_frame_commands(cloud_context(scene),&commands,&count)!=NX_OK || !GetClientRect(nx_window_native_hwnd(window),&rect))return 0;
 LONGLONG render_begin=tick();nx_render_outcome outcome;
 nx_result result=nx_renderer_run(driver,(nx_render_target){(uint32_t)rect.right,(uint32_t)rect.bottom,info.scale},commands,count,(nx_color){0,0,0,1},&outcome);
 *cpu_ticks=render_begin-before;*render_ticks=tick()-render_begin;*command_count=count;
 return result==NX_OK && outcome==NX_RENDER_READY;
}
static int selected(cloud_scene *scene,uint64_t id){uint32_t index;nx_semantic_view v;
 return nx_semantics_find(cloud_context(scene),id,&index)==NX_OK && nx_semantics_get(cloud_context(scene),index,&v)==NX_OK && (v.state&NX_SEM_SELECTED);}
int main(int argc,char **argv){
 QueryPerformanceFrequency(&probe_frequency);entry_tick.QuadPart=tick();
 if(argc!=2){fputs("usage: nexaui_d3d12_startup_bench FONT.ttf\n",stderr);return 2;}
 nx_window *window=NULL;nx_renderer_driver driver={0};cloud_scene *scene=NULL;
 LONGLONG marks[9]={entry_tick.QuadPart},cpu,render;size_t commands=0;int result=1;
 scene=cloud_create(argv[1]);if(!scene)goto done;marks[1]=tick();
 nx_window_desc desc={"NexaUI R01 startup probe",1280,720,NX_WINDOW_HIDDEN,{0}};
 if(nx_window_create(&desc,&window)!=NX_OK)goto done;
 HWND hwnd=nx_window_native_hwnd(window);
 ShowWindow(hwnd,SW_SHOWNOACTIVATE);SetWindowPos(hwnd,HWND_TOP,40,40,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
 cloud_set_window(scene,window);marks[2]=tick();
 if(nx_d3d12_driver_open(window,&driver)!=NX_OK)goto done;marks[3]=tick();
 if(!render_frame(window,scene,&driver,&cpu,&render,&commands))goto done;marks[4]=tick();
 if(FAILED(DwmFlush()))goto done;marks[5]=tick();
 DWORD cloaked=1;if(!IsWindowVisible(hwnd)||IsIconic(hwnd)||FAILED(DwmGetWindowAttribute(hwnd,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))||cloaked)goto done;
 nx_d3d12_renderer *backend=driver.backend.user;
 if(backend->submitted_frames!=1 || !backend->width || !backend->height)goto done;
 uint32_t index;nx_semantic_view files;
 if(nx_semantics_find(cloud_context(scene),11,&index)!=NX_OK || nx_semantics_get(cloud_context(scene),index,&files)!=NX_OK)goto done;
 nx_window_info info;if(nx_window_get_info(window,&info)!=NX_OK)goto done;
 LPARAM position=MAKELPARAM((int)((files.bounds.x+files.bounds.width/2)*info.scale),(int)((files.bounds.y+files.bounds.height/2)*info.scale));
 if(!PostMessageW(hwnd,WM_MOUSEMOVE,0,position)||!PostMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,position)||!PostMessageW(hwnd,WM_LBUTTONUP,0,position))goto done;
 LONGLONG interaction_cpu,interaction_render;size_t interaction_commands;
 for(unsigned i=0;i<4;++i){if(!render_frame(window,scene,&driver,&interaction_cpu,&interaction_render,&interaction_commands))goto done;if(selected(scene,11))break;}
 if(!selected(scene,11))goto done;marks[6]=tick();
 if(FAILED(DwmFlush()))goto done;marks[7]=tick();
 PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);
 if(!K32GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory)))goto done;
 printf("READY {\"schema\":1,\"warp\":%d,\"frequency\":%lld,\"entry_tick\":%lld,\"ready_tick\":%lld,\"width\":%u,\"height\":%u,\"scale\":%.3f,\"commands\":%zu,\"font_ms\":%.3f,\"window_ms\":%.3f,\"driver_ms\":%.3f,\"first_ui_ms\":%.3f,\"first_render_ms\":%.3f,\"entry_to_first_present_ms\":%.3f,\"entry_to_first_composed_ms\":%.3f,\"entry_to_ready_ms\":%.3f,\"frames_at_ready\":%llu}\n",
 backend->using_warp,probe_frequency.QuadPart,entry_tick.QuadPart,marks[7],backend->width,backend->height,(double)info.scale,commands,
 ms(marks[1]-marks[0]),ms(marks[2]-marks[1]),ms(marks[3]-marks[2]),ms(cpu),ms(render),ms(marks[4]-marks[0]),ms(marks[5]-marks[0]),ms(marks[7]-marks[0]),backend->submitted_frames);
 fflush(stdout);
 double warm[5];for(unsigned i=0;i<5;++i){LONGLONG ui,gpu;size_t n;if(!render_frame(window,scene,&driver,&ui,&gpu,&n))goto done;warm[i]=ms(ui+gpu);}
 puts("FINAL {");printf("\"ready_private_bytes\":%zu,\"ready_peak_working_set_bytes\":%zu,\"warm_ms\":[%.3f,%.3f,%.3f,%.3f,%.3f],\"spans\":[\n",(size_t)memory.PrivateUsage,(size_t)memory.PeakWorkingSetSize,warm[0],warm[1],warm[2],warm[3],warm[4]);
 for(unsigned i=0;i<span_count;++i){span_record s=spans[i];printf("%s{\"kind\":\"%s\",\"begin_tick\":%lld,\"end_tick\":%lld,\"total_ms\":%.3f,\"api_ms\":%.3f,\"hr\":%ld,\"bytecode_bytes\":%zu,\"key\":\"%016llx\"}\n",i?",":"",s.kind,s.begin,s.end,ms(s.end-s.begin),ms(s.api_ticks),(long)s.hr,s.bytes,(unsigned long long)s.key);}
 puts("]}");fflush(stdout);result=0;
done:
 if(result)fprintf(stderr,"R01 probe failed; stage ticks: %lld %lld %lld %lld %lld %lld %lld %lld; last Win32 %lu\n",marks[0],marks[1],marks[2],marks[3],marks[4],marks[5],marks[6],marks[7],(unsigned long)GetLastError());
 if(driver.backend.user && nx_renderer_close(&driver)!=NX_OK)result=1;
 if(scene)cloud_set_window(scene,NULL);cloud_destroy(scene);nx_window_destroy(window);return result;
}
