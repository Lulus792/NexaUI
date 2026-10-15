#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#include "renderer.h"
#include "window_internal.h"
#include "draw_plan.h"
#include "bitmap_plan.h"
#include "batch_plan.h"
#include "timing_history.h"
#include "shader_source.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NX_MTL_MAX_AXIS 8192u
#define NX_MTL_WAIT_NS UINT64_C(1000000000)

typedef struct nx_mtl_frame_slot {
    id<MTLBuffer> geometry, bitmap_upload, readback;
    id<MTLTexture> atlas;
    id<MTLCommandBuffer> pending;
    size_t geometry_capacity, bitmap_capacity, read_stride;
    uint32_t atlas_width, atlas_height, read_width, read_height;
    uint64_t submission;
} nx_mtl_frame_slot;

struct nx_metal_renderer {
    nx_allocator allocator;
    nx_window *window;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLRenderPipelineState> pipeline, batch_pipeline;
    CAMetalLayer *layer;
    CALayer *previous_layer;
    BOOL previous_wants_layer;
    id<MTLTexture> canvas, history;
    nx_mtl_frame_slot slots[NX_METAL_FRAME_SLOTS];
    uint32_t frame_limit, pending_frames, peak_pending_frames, next_slot, last_slot;
    uint64_t geometry_allocations;
    nx_mtl_bitmap_plan bitmaps;
    uint32_t last_bitmaps;
    uint64_t atlas_allocations, bitmap_allocations;
    uint32_t width, height;
    uint64_t submitted, completed;
    nx_metal_frame_profile last_frame;
    nx_mtl_timing_history timings;
    uint64_t retirement_wait_ns, retirement_wait_calls, retirement_clock_errors;
    int leased, closing, broken;
#ifdef NEXAUI_METAL_TEST_HOOKS
    unsigned drawable_unavailable, canvas_memory, history_memory, command_memory, pipeline_memory, atlas_memory, upload_memory, geometry_memory, wait_timeouts;
    int per_draw_bindings, disable_batching;
    id<MTLSharedEvent> test_gate;
#endif
};

static void *nx_mtl_allocate(void *user, size_t bytes) { (void)user; return malloc(bytes); }
static void nx_mtl_free(void *user, void *p, size_t bytes) { (void)user; (void)bytes; free(p); }
static int nx_mtl_owner(nx_metal_renderer *r) { return r && pthread_main_np(); }
static int nx_mtl_color(nx_color c) {
    return isfinite(c.r) && isfinite(c.g) && isfinite(c.b) && isfinite(c.a) &&
        c.r >= 0 && c.r <= 1 && c.g >= 0 && c.g <= 1 && c.b >= 0 && c.b <= 1 && c.a >= 0 && c.a <= 1;
}
static double nx_mtl_byte(float x) { return floorf(x * 255.f + .5f) / 255.0; }
static uint64_t nx_mtl_now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
static double nx_mtl_elapsed_ms(uint64_t start, uint64_t end, int *valid) {
    if (!start || !end || end < start) { *valid = 0; return 0; }
    return (double)(end - start) / 1.e6;
}
static nx_result nx_mtl_wait_finished(nx_metal_renderer *r, uint64_t start, nx_result result) {
    uint64_t end = nx_mtl_now();
    ++r->retirement_wait_calls;
    if (!end || end < start) ++r->retirement_clock_errors;
    else {
        uint64_t elapsed = end - start;
        r->retirement_wait_ns = elapsed > UINT64_MAX - r->retirement_wait_ns ? UINT64_MAX : r->retirement_wait_ns + elapsed;
    }
    return result;
}
/* Poll a retained command buffer instead of an unbounded waitUntilCompleted.
   No asynchronous callback retains or dereferences the C renderer. */
static uint32_t nx_mtl_oldest(nx_metal_renderer *r) {
    uint32_t index = NX_METAL_FRAME_SLOTS;
    for (uint32_t i = 0; i < NX_METAL_FRAME_SLOTS; ++i)
        if (r->slots[i].pending && (index == NX_METAL_FRAME_SLOTS || r->slots[i].submission < r->slots[index].submission)) index = i;
    return index;
}
static nx_result nx_mtl_retire_slot(nx_metal_renderer *r, uint32_t index, int keep_readback, uint64_t drain_start) {
    nx_mtl_frame_slot *slot = &r->slots[index];
    if (!slot->pending) return NX_OK;
    if (!drain_start) { ++r->retirement_clock_errors; return NX_ERROR_PLATFORM; }
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->wait_timeouts) { --r->wait_timeouts; return NX_ERROR_LIMIT; }
#endif
    uint64_t start = nx_mtl_now();
    if (!start) { ++r->retirement_clock_errors; return NX_ERROR_PLATFORM; }
    for (;;) {
        MTLCommandBufferStatus status = [slot->pending status];
        if (status == MTLCommandBufferStatusCompleted || status == MTLCommandBufferStatusError) {
            /* Completed means GPU work is terminal. Read optional timing only
               here, before releasing the command buffer; never while pending.
               Missing/invalid pairs remain explicitly unavailable. */
            int failed = status == MTLCommandBufferStatusError;
            double gs = 0, ge = 0, ks = 0, ke = 0;
            if (!failed) {
                gs = slot->pending.GPUStartTime; ge = slot->pending.GPUEndTime;
                ks = slot->pending.kernelStartTime; ke = slot->pending.kernelEndTime;
            }
            (void)nx_mtl_timing_finish(&r->timings,slot->submission,failed,gs,ge,ks,ke);
            [slot->pending release]; slot->pending = nil;
            if (!keep_readback || status == MTLCommandBufferStatusError) { [slot->readback release]; slot->readback = nil; }
            ++r->completed; --r->pending_frames;
            if (status == MTLCommandBufferStatusError) { r->broken = 1; return nx_mtl_wait_finished(r,start,NX_ERROR_STATE); }
            return nx_mtl_wait_finished(r,start,NX_OK);
        }
        uint64_t now = nx_mtl_now();
        if (!now) return nx_mtl_wait_finished(r,start,NX_ERROR_PLATFORM);
        if (now - drain_start >= NX_MTL_WAIT_NS) return nx_mtl_wait_finished(r,start,NX_ERROR_LIMIT);
        struct timespec pause = {0,1000000}; nanosleep(&pause,NULL);
    }
}
/* One shared deadline bounds a complete drain, rather than one second per
   slot. Terminal errors are latched, but do not strand later submissions. */
static nx_result nx_mtl_retire(nx_metal_renderer *r, int keep_readback) {
    uint64_t start = nx_mtl_now(); nx_result result = NX_OK;
    while (r->pending_frames) {
        nx_result retired = nx_mtl_retire_slot(r,nx_mtl_oldest(r),keep_readback,start);
        if (retired != NX_OK && retired != NX_ERROR_STATE) return retired;
        if (retired != NX_OK) result = retired;
    }
    if (!keep_readback) for (uint32_t i = 0; i < NX_METAL_FRAME_SLOTS; ++i) {
        [r->slots[i].readback release]; r->slots[i].readback = nil;
    }
    return result;
}
static nx_result nx_mtl_acquire_slot(nx_metal_renderer *r) {
    if (r->pending_frames == r->frame_limit) {
        nx_result result = nx_mtl_retire_slot(r,nx_mtl_oldest(r),0,nx_mtl_now());
        if (result != NX_OK) return result;
    }
    nx_mtl_frame_slot *slot = &r->slots[r->next_slot];
    if (slot->pending) return NX_ERROR_STATE;
    [slot->readback release]; slot->readback = nil;
    return NX_OK;
}
static void nx_mtl_slots_release(nx_metal_renderer *r) {
#ifdef NEXAUI_METAL_TEST_HOOKS
    [r->test_gate release]; r->test_gate = nil;
#endif
    for (uint32_t i = 0; i < NX_METAL_FRAME_SLOTS; ++i) {
        nx_mtl_frame_slot *slot = &r->slots[i];
        [slot->atlas release]; [slot->bitmap_upload release];
        [slot->geometry release]; [slot->readback release];
        *slot = (nx_mtl_frame_slot){0};
    }
    r->next_slot = r->last_slot = 0;
}
static nx_result nx_mtl_device(nx_metal_renderer *r) {
    r->device = MTLCreateSystemDefaultDevice(); /* Create rule: owned reference. */
    if (!r->device) return NX_ERROR_UNSUPPORTED;
    r->queue = [r->device newCommandQueue];
    if (!r->queue) { [r->device release]; r->device = nil; return NX_ERROR_MEMORY; }
    [r->queue setLabel:@"NexaUI UI queue"];
    return NX_OK;
}
static id<MTLRenderPipelineState> nx_mtl_make_pipeline(nx_metal_renderer *r, id<MTLLibrary> library,
    NSString *vertex_name, NSString *fragment_name, NSError **error) {
    id<MTLFunction> vertex = [library newFunctionWithName:vertex_name];
    id<MTLFunction> fragment = [library newFunctionWithName:fragment_name];
    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    id<MTLRenderPipelineState> pipeline = nil;
    if (desc && vertex && fragment) {
        desc.label = @"NexaUI shape composition";
        desc.vertexFunction = vertex; desc.fragmentFunction = fragment;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.colorAttachments[0].blendingEnabled = NO;
        pipeline = [r->device newRenderPipelineStateWithDescriptor:desc error:error];
    }
    [desc release]; [fragment release]; [vertex release]; return pipeline;
}
static nx_result nx_mtl_pipeline(nx_metal_renderer *r) {
    if (r->batch_pipeline) return NX_OK;
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->pipeline_memory) { r->pipeline_memory = 0; return NX_ERROR_MEMORY; }
#endif
    NSMutableString *source = [NSMutableString string];
    for (size_t i = 0; i < sizeof(nx_mtl_shader_chunks)/sizeof(nx_mtl_shader_chunks[0]); ++i)
        [source appendString:[NSString stringWithUTF8String:nx_mtl_shader_chunks[i]]];
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = NO; /* Preserve split-product arithmetic. */
    options.languageVersion = MTLLanguageVersion2_0;
    NSError *error = nil;
    id<MTLLibrary> library = [r->device newLibraryWithSource:source options:options error:&error];
    [options release];
    if (!library) {
        const char *message = [[error localizedDescription] UTF8String];
        fprintf(stderr,"NexaUI Metal shader compilation: %s\n",message ? message : "no library returned");
        return NX_ERROR_PLATFORM;
    }
    id<MTLRenderPipelineState> batch = nx_mtl_make_pipeline(r,library,@"nx_mtl_batch_vertex",@"nx_mtl_batch_fragment",&error);
    id<MTLRenderPipelineState> legacy = nil;
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (batch) legacy = nx_mtl_make_pipeline(r,library,@"nx_mtl_vertex",@"nx_mtl_fragment",&error);
    if (!legacy) { [batch release]; batch = nil; }
#endif
    [library release];
    if (!batch) {
        const char *message = [[error localizedDescription] UTF8String];
        fprintf(stderr,"NexaUI Metal shape pipeline: %s\n",message ? message : "missing function or pipeline");
        return NX_ERROR_PLATFORM;
    }
    r->pipeline = legacy; r->batch_pipeline = batch;
    return NX_OK;
}
static nx_result nx_mtl_resize(nx_metal_renderer *r, uint32_t width, uint32_t height, double scale) {
    if (!isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    if (width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_LIMIT;
    if (!width || !height) width = height = 0;
    if (width != r->width || height != r->height) {
        nx_result result = nx_mtl_retire(r,0);
        if (result != NX_OK) return result;
        id<MTLTexture> texture = nil, history = nil;
        if (width) {
#ifdef NEXAUI_METAL_TEST_HOOKS
            if (r->canvas_memory) { r->canvas_memory = 0; return NX_ERROR_MEMORY; }
#endif
            MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                width:width height:height mipmapped:NO];
            desc.storageMode = MTLStorageModePrivate;
            desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            texture = [r->device newTextureWithDescriptor:desc];
            if (!texture) return NX_ERROR_MEMORY;
            [texture setLabel:@"NexaUI owned canvas"];
            /* Standalone device textures use tracked hazards with this
               MTLCommandQueue. Copy/read order spans successive encoders. */
            desc.usage = MTLTextureUsageShaderRead;
#ifdef NEXAUI_METAL_TEST_HOOKS
            if (r->history_memory) { r->history_memory = 0; [texture release]; return NX_ERROR_MEMORY; }
#endif
            history = [r->device newTextureWithDescriptor:desc];
            if (!history) { [texture release]; return NX_ERROR_MEMORY; }
            [history setLabel:@"NexaUI prior destination"];
        }
        [r->canvas release]; [r->history release];
        r->canvas = texture; r->history = history; r->width = width; r->height = height;
    }
    NSView *view = nx_window_native_nsview(r->window);
    [r->layer setFrame:[view bounds]]; [r->layer setContentsScale:scale];
    [r->layer setDrawableSize:CGSizeMake(width,height)];
    return NX_OK;
}
static nx_result nx_mtl_bitmaps_prepare(nx_metal_renderer *r, nx_mtl_frame_slot *slot, const nx_command *stream, size_t count) {
    nx_result result = nx_mtl_bitmaps_build(&r->bitmaps,stream,count,slot->atlas_width,slot->atlas_height);
    if (result != NX_OK || !r->bitmaps.count) return result;
    uint32_t width = r->bitmaps.width, height = r->bitmaps.height;
    size_t bytes = (size_t)width*height*4u;
    id<MTLTexture> candidate = nil;
    id<MTLBuffer> upload = nil;
    if (width != slot->atlas_width || height != slot->atlas_height) {
#ifdef NEXAUI_METAL_TEST_HOOKS
        if (r->atlas_memory) { r->atlas_memory = 0; return NX_ERROR_MEMORY; }
#endif
        MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:width height:height mipmapped:NO];
        desc.storageMode = MTLStorageModePrivate; desc.usage = MTLTextureUsageShaderRead;
        candidate = [r->device newTextureWithDescriptor:desc];
        if (!candidate) return NX_ERROR_MEMORY;
        candidate.label = @"NexaUI RGBA/mask atlas";
    }
    if (bytes > slot->bitmap_capacity) {
#ifdef NEXAUI_METAL_TEST_HOOKS
        if (r->upload_memory) { r->upload_memory = 0; [candidate release]; return NX_ERROR_MEMORY; }
#endif
        upload = [r->device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        if (!upload) { [candidate release]; return NX_ERROR_MEMORY; }
        upload.label = @"NexaUI owned bitmap staging";
    }
    if (candidate) {
        [slot->atlas release]; slot->atlas = candidate; slot->atlas_width = width; slot->atlas_height = height; ++r->atlas_allocations;
    }
    if (upload) {
        [slot->bitmap_upload release]; slot->bitmap_upload = upload; slot->bitmap_capacity = bytes; ++r->bitmap_allocations;
    }
    /* This slot was retired before writing its shared buffer.
       Borrowed source pixels are fully copied before successful return. */
    return nx_mtl_bitmaps_copy(&r->bitmaps,[slot->bitmap_upload contents],slot->bitmap_capacity,(size_t)width*4u);
}
static nx_result nx_mtl_geometry_prepare(nx_metal_renderer *r, nx_mtl_frame_slot *slot, const nx_mtl_plan *plan, nx_mtl_batches *out_batches) {
    nx_mtl_record_plan prepared; size_t bytes = 0;
    nx_result result = nx_mtl_records_begin(&prepared,plan,&r->bitmaps);
    if (result == NX_OK) result = nx_mtl_record_bytes(prepared.count,&bytes);
    if (result != NX_OK) return result;
    id<MTLBuffer> candidate = nil;
    size_t capacity = slot->geometry_capacity;
    if (bytes > capacity) {
#ifdef NEXAUI_METAL_TEST_HOOKS
        if (r->geometry_memory) { r->geometry_memory = 0; return NX_ERROR_MEMORY; }
#endif
        capacity = capacity ? capacity : 4096;
        while (capacity < bytes) capacity *= 2;
        candidate = [r->device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
        if (!candidate) return NX_ERROR_MEMORY;
        candidate.label = @"NexaUI owned draw records";
    }
    id<MTLBuffer> buffer = candidate ? candidate : slot->geometry;
    size_t written;
    result = nx_mtl_records_write(&prepared,[buffer contents],capacity / sizeof(nx_mtl_record),&written);
    nx_mtl_batches check;
    if (result == NX_OK) result = nx_mtl_batches_begin(&check,[buffer contents],written,r->width,r->height);
    if (result != NX_OK) { [candidate release]; return result; }
    if (candidate) {
        [slot->geometry release]; slot->geometry = candidate; slot->geometry_capacity = capacity; ++r->geometry_allocations;
    }
    *out_batches = check; return NX_OK;
}
static void nx_mtl_bind_pass(nx_metal_renderer *r, nx_mtl_frame_slot *slot, id<MTLRenderCommandEncoder> encoder, int batched) {
    [encoder setRenderPipelineState:batched ? r->batch_pipeline : r->pipeline];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setViewport:(MTLViewport){0,0,r->width,r->height,0,1}];
    [encoder setFragmentTexture:r->history atIndex:0];
    [encoder setFragmentTexture:(slot->atlas ? slot->atlas : r->history) atIndex:1];
    if (batched) {
        [encoder setVertexBuffer:slot->geometry offset:0 atIndex:0];
        [encoder setFragmentBuffer:slot->geometry offset:0 atIndex:0];
    }
}
static nx_result nx_mtl_submit(nx_metal_renderer *r, const nx_command *stream, size_t count, double scale, nx_color clear,
    id<MTLBuffer> readback, size_t read_stride, int *presented) {
    *presented = 0;
    if (r->closing || r->broken) return NX_ERROR_STATE;
    if (!r->width || !r->height) return NX_OK;
    if (r->submitted == UINT64_MAX) return NX_ERROR_LIMIT;
    nx_metal_frame_profile profile = {0};
    profile.pixel_width = r->width; profile.pixel_height = r->height; profile.draw_scale = scale;
    profile.input_commands = count; profile.cpu_timing_valid = 1;
    profile.batched = 1;
#ifdef NEXAUI_METAL_TEST_HOOKS
    profile.per_draw_bindings = r->per_draw_bindings;
    profile.batched = !r->disable_batching;
#endif
    uint64_t phase_start = nx_mtl_now();
    nx_mtl_plan plan;
    nx_result result = nx_mtl_plan_begin(&plan,stream,count,scale,r->width,r->height);
    if (result != NX_OK) return result;
    if (count && (result = nx_mtl_pipeline(r)) != NX_OK) return result;
    profile.prepare_ms = nx_mtl_elapsed_ms(phase_start,nx_mtl_now(),&profile.cpu_timing_valid);
    result = nx_mtl_acquire_slot(r);
    if (result != NX_OK) return result == NX_ERROR_LIMIT ? NX_OK : result;
    phase_start = nx_mtl_now();
    nx_mtl_frame_slot *slot = &r->slots[r->next_slot];
    result = nx_mtl_bitmaps_prepare(r,slot,stream,count);
    if (result != NX_OK) return result;
    nx_mtl_batches batches = {0};
    if (profile.batched) {
        result = nx_mtl_geometry_prepare(r,slot,&plan,&batches);
        if (result != NX_OK) return result;
        profile.record_upload_bytes = (uint64_t)batches.count * sizeof(nx_mtl_record);
    }
    profile.prepare_ms += nx_mtl_elapsed_ms(phase_start,nx_mtl_now(),&profile.cpu_timing_valid);
    phase_start = nx_mtl_now();
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->command_memory) { r->command_memory = 0; return NX_ERROR_MEMORY; }
#endif
    id<MTLCommandBuffer> commands = [r->queue commandBuffer];
    if (!commands) return NX_ERROR_MEMORY;
    commands.label = @"NexaUI shapes/present";
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->test_gate) [commands encodeWaitForEvent:r->test_gate value:1];
#endif
    if (r->bitmaps.count) {
        profile.bitmap_upload_bytes = (uint64_t)r->bitmaps.width*r->bitmaps.height*4u;
        id<MTLBlitCommandEncoder> upload = [commands blitCommandEncoder];
        if (!upload) return NX_ERROR_MEMORY;
        size_t stride = (size_t)r->bitmaps.width*4u;
        [upload copyFromBuffer:slot->bitmap_upload sourceOffset:0 sourceBytesPerRow:stride
            sourceBytesPerImage:stride*r->bitmaps.height sourceSize:MTLSizeMake(r->bitmaps.width,r->bitmaps.height,1)
            toTexture:slot->atlas destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
        [upload endEncoding];
    }
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = r->canvas;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(nx_mtl_byte(clear.r), nx_mtl_byte(clear.g), nx_mtl_byte(clear.b), nx_mtl_byte(clear.a));
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
    if (!encoder) return NX_ERROR_MEMORY;
    profile.render_passes = 1;
    int bound = 0;
    if (count) {
        nx_mtl_draw draw = {0}; nx_mtl_batch batch = {0};
        while (profile.batched ? nx_mtl_batches_next(&batches,&batch) : nx_mtl_plan_next(&plan,&draw)) {
            if (!profile.batched && draw.shape.kind == 5) {
                uint32_t entry = nx_mtl_bitmap_find(&r->bitmaps,draw.bitmap);
                if (entry == r->bitmaps.count) { [encoder endEncoding]; return NX_ERROR_STATE; }
                draw.shape.c = (nx_point){(float)r->bitmaps.entries[entry].x,(float)r->bitmaps.entries[entry].y};
            }
            nx_mtl_box scissor = profile.batched ? batch.scissor : draw.scissor;
            int history = profile.batched ? batch.needs_history : draw.shape.kind == 5 || draw.shape.color[3] < 255u;
            if (history) {
                nx_mtl_box history_box = profile.batched ? batch.history_box : scissor;
                [encoder endEncoding];
                id<MTLBlitCommandEncoder> history_copy = [commands blitCommandEncoder];
                if (!history_copy) return NX_ERROR_MEMORY;
                MTLOrigin origin = MTLOriginMake(history_box.x,history_box.y,0);
                [history_copy copyFromTexture:r->canvas sourceSlice:0 sourceLevel:0 sourceOrigin:origin
                    sourceSize:MTLSizeMake(history_box.width,history_box.height,1) toTexture:r->history
                    destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
                [history_copy endEncoding];
                pass.colorAttachments[0].loadAction = MTLLoadActionLoad;
                encoder = [commands renderCommandEncoderWithDescriptor:pass];
                if (!encoder) return NX_ERROR_MEMORY;
                ++profile.render_passes; ++profile.history_copies;
                bound = 0;
                profile.history_copy_bytes += (uint64_t)history_box.width*history_box.height*4u;
            }
            if (!bound) ++profile.bound_passes;
            if (!bound || profile.per_draw_bindings) {
                nx_mtl_bind_pass(r,slot,encoder,profile.batched); profile.fixed_state_bindings += 5;
                if (profile.batched) profile.record_buffer_bindings += 2;
            }
            bound = 1;
            [encoder setScissorRect:(MTLScissorRect){scissor.x,scissor.y,scissor.width,scissor.height}];
            /* set*Bytes copies these bounded arguments into Metal storage.
               No pointer into the caller's command stream survives encoding. */
            if (profile.batched) {
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:(NSUInteger)batch.first*6u vertexCount:(NSUInteger)batch.count*6u];
                profile.draw_records += batch.count;
            } else {
                [encoder setVertexBytes:draw.vertices length:sizeof(draw.vertices) atIndex:0];
                [encoder setFragmentBytes:&draw.shape length:sizeof(draw.shape) atIndex:0];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                ++profile.draw_records;
                profile.draw_argument_bytes += sizeof(draw.vertices) + sizeof(draw.shape);
            }
            ++profile.draw_calls;
        }
    }
    [encoder endEncoding];
    id<MTLBlitCommandEncoder> copy = [commands blitCommandEncoder];
    if (!copy) return NX_ERROR_MEMORY;
    if (readback) [copy copyFromTexture:r->canvas sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(r->width,r->height,1) toBuffer:readback destinationOffset:0
        destinationBytesPerRow:read_stride destinationBytesPerImage:read_stride * r->height];
    [copy endEncoding];
    profile.encode_ms = nx_mtl_elapsed_ms(phase_start,nx_mtl_now(),&profile.cpu_timing_valid);
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->drawable_unavailable) { r->drawable_unavailable = 0; return NX_OK; }
#endif
    phase_start = nx_mtl_now();
    [CATransaction flush]; /* Commit layer geometry in manually pumped C apps. */
    id<CAMetalDrawable> drawable = [r->layer nextDrawable];
    profile.acquire_ms = nx_mtl_elapsed_ms(phase_start,nx_mtl_now(),&profile.cpu_timing_valid);
    if (!drawable) return NX_OK; /* Layer acquisition has a one-second timeout. */
    if (drawable.texture.width != r->width || drawable.texture.height != r->height) return NX_OK;
    phase_start = nx_mtl_now();
    copy = [commands blitCommandEncoder];
    if (!copy) return NX_ERROR_MEMORY;
    [copy copyFromTexture:r->canvas sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(r->width,r->height,1) toTexture:drawable.texture destinationSlice:0 destinationLevel:0
        destinationOrigin:MTLOriginMake(0,0,0)];
    [copy endEncoding];
    [commands presentDrawable:drawable];
    slot->pending = [commands retain];
    if (readback) {
        slot->readback = [readback retain]; slot->read_stride = read_stride;
        slot->read_width = r->width; slot->read_height = r->height;
    }
    [commands commit]; ++r->submitted; r->last_bitmaps = r->bitmaps.count; *presented = 1;
    profile.commit_ms = nx_mtl_elapsed_ms(phase_start,nx_mtl_now(),&profile.cpu_timing_valid);
    profile.present_copy_bytes = (uint64_t)r->width*r->height*4u;
    slot->submission = r->submitted;
    ++r->pending_frames;
    if (r->pending_frames > r->peak_pending_frames) r->peak_pending_frames = r->pending_frames;
    profile.frame_slot = r->next_slot; profile.frame_limit = r->frame_limit; profile.pending_frames = r->pending_frames;
    r->last_slot = r->next_slot; r->next_slot = (r->next_slot + 1) % r->frame_limit;
    profile.submission = r->submitted; r->last_frame = profile;
    nx_mtl_timing_begin(&r->timings,&profile);
    /* The autorelease pool promptly releases our drawable reference. Metal /
       Core Animation retain their own presentation resources. */
    return NX_OK;
}

nx_result nx_metal_renderer_create(nx_window *window, const nx_allocator *allocator, nx_metal_renderer **out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = NULL;
    if (!window || (allocator && (!!allocator->allocate != !!allocator->deallocate))) return NX_ERROR_ARGUMENT;
    if (!pthread_main_np()) return NX_ERROR_STATE;
    nx_allocator a = allocator ? *allocator : (nx_allocator){0};
    if (!a.allocate) a = (nx_allocator){NULL,nx_mtl_allocate,nx_mtl_free};
    nx_metal_renderer *r = a.allocate(a.user,sizeof(*r));
    if (!r) return NX_ERROR_MEMORY;
    *r = (nx_metal_renderer){0}; r->allocator = a; r->window = window; r->frame_limit = NX_METAL_FRAME_SLOTS;
    nx_result result = nx_window_renderer_acquire(window,r);
    if (result == NX_OK) r->leased = 1;
    @autoreleasepool {
        if (result == NX_OK) result = nx_mtl_device(r);
        if (result == NX_OK) {
            NSView *view = nx_window_native_nsview(window);
            r->previous_layer = [[view layer] retain]; r->previous_wants_layer = [view wantsLayer];
            r->layer = [[CAMetalLayer alloc] init];
            if (!r->layer) result = NX_ERROR_MEMORY;
            else {
                r->layer.device = r->device; r->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                r->layer.framebufferOnly = NO; r->layer.opaque = YES;
                r->layer.allowsNextDrawableTimeout = YES;
                CGColorSpaceRef color = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
                if (!color) result = NX_ERROR_MEMORY;
                else {
                    r->layer.colorspace = color; CGColorSpaceRelease(color);
                    /* Layer hosting requires this order; wantsLayer first
                       would ask AppKit to manage a backing layer instead. */
                    [view setLayer:r->layer]; [view setWantsLayer:YES];
                }
            }
        }
    }
    if (result != NX_OK) { (void)nx_metal_renderer_destroy(r); return result; }
    *out = r; return NX_OK;
}
nx_result nx_metal_render(nx_metal_renderer *r, const nx_command *commands, size_t count, double scale, nx_color clear, int *presented) {
    if (!presented) return NX_ERROR_ARGUMENT;
    *presented = 0;
    if (!r || !isfinite(scale) || scale <= 0 || !nx_mtl_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    nx_mtl_plan preflight;
    nx_result result = nx_mtl_plan_begin(&preflight,commands,count,scale,0,0);
    if (result != NX_OK) return result;
    if (r->closing || r->broken) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_window_info info; result = nx_window_get_info(r->window,&info);
        if (result != NX_OK) return result;
        double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
        if (width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_LIMIT;
        result = nx_mtl_resize(r,(uint32_t)width,(uint32_t)height,info.scale);
        if (result == NX_ERROR_LIMIT) return NX_OK;
        return result == NX_OK ? nx_mtl_submit(r,commands,count,scale,clear,nil,0,presented) : result;
    }
}
nx_result nx_metal_renderer_get_info(nx_metal_renderer *r, nx_metal_renderer_info *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_metal_renderer_info){0};
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    @autoreleasepool {
        const char *name = [[r->device name] UTF8String];
        snprintf(out->device_name,sizeof(out->device_name),"%s",name ? name : "");
        out->pixel_width = r->width; out->pixel_height = r->height;
        out->submitted_frames = r->submitted; out->completed_frames = r->completed;
        out->pending = r->pending_frames != 0; out->pending_frames = r->pending_frames;
        out->peak_pending_frames = r->peak_pending_frames; out->frame_limit = r->frame_limit; out->next_frame_slot = r->next_slot;
        nx_mtl_frame_slot *last = &r->slots[r->last_slot];
        out->atlas_width = last->atlas_width; out->atlas_height = last->atlas_height; out->unique_bitmaps = r->last_bitmaps;
        out->atlas_allocations = r->atlas_allocations; out->bitmap_upload_allocations = r->bitmap_allocations;
        out->record_upload_allocations = r->geometry_allocations;
        for (uint32_t i = 0; i < NX_METAL_FRAME_SLOTS; ++i) {
            nx_mtl_frame_slot *slot = &r->slots[i];
            out->bitmap_capacity_bytes += slot->bitmap_capacity; out->record_capacity_bytes += slot->geometry_capacity;
            out->pending_readback |= slot->readback != nil;
            out->slots[i] = (nx_metal_slot_info){slot->submission,slot->geometry_capacity,slot->bitmap_capacity,
                slot->atlas_width,slot->atlas_height,slot->pending != nil,slot->readback != nil};
        }
        out->last_frame = r->last_frame;
        out->last_completed_timing = r->timings.last_completed;
        out->retirement_wait_ns = r->retirement_wait_ns; out->retirement_wait_calls = r->retirement_wait_calls;
        out->retirement_clock_errors = r->retirement_clock_errors;
        out->closing = r->closing; out->broken = r->broken; return NX_OK;
    }
}
nx_result nx_metal_renderer_get_timing(nx_metal_renderer *r, uint64_t submission, nx_metal_gpu_timing *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_metal_gpu_timing){0};
    if (!r || !submission) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    return nx_mtl_timing_find(&r->timings,submission,out);
}
nx_result nx_metal_renderer_wait_idle(nx_metal_renderer *r) {
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    @autoreleasepool { return nx_mtl_retire(r,0); }
}
nx_result nx_metal_renderer_recover(nx_metal_renderer *r) {
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_result result = nx_mtl_retire(r,0);
        if (r->pending_frames) return result;
        r->timings = (nx_mtl_timing_history){0};
        [r->canvas release]; r->canvas = nil; r->width = r->height = 0;
        [r->history release]; r->history = nil;
        nx_mtl_slots_release(r); r->last_bitmaps = 0; r->bitmaps.count = 0;
        [r->pipeline release]; r->pipeline = nil; [r->batch_pipeline release]; r->batch_pipeline = nil;
        r->layer.device = nil; [r->queue release]; r->queue = nil; [r->device release]; r->device = nil;
        result = nx_mtl_device(r);
        if (result == NX_OK) r->layer.device = r->device;
        r->broken = result != NX_OK; return result;
    }
}
nx_result nx_metal_renderer_destroy(nx_metal_renderer *r) {
    if (!r) return NX_OK;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    r->closing = 1;
    @autoreleasepool {
        nx_result result = nx_mtl_retire(r,0);
        if (r->pending_frames) return result;
        NSView *view = nx_window_native_nsview(r->window);
        if (r->layer && [view layer] == r->layer) {
            /* Disabling first avoids AppKit manufacturing a replacement
               backing layer when restoring an originally nil layer. */
            [view setWantsLayer:NO]; [view setLayer:r->previous_layer];
            if (r->previous_wants_layer) [view setWantsLayer:YES];
        }
        [r->previous_layer release]; [r->layer release]; [r->canvas release]; [r->history release];
        nx_mtl_slots_release(r); [r->pipeline release]; [r->batch_pipeline release];
        [r->queue release]; [r->device release];
        if (r->leased) (void)nx_window_renderer_release(r->window,r);
        nx_allocator a = r->allocator; a.deallocate(a.user,r,sizeof(*r)); return NX_OK;
    }
}
static nx_result nx_mtl_driver_resize(void *user, nx_render_target target, nx_render_outcome *outcome) {
    nx_metal_renderer *r = user; *outcome = NX_RENDER_SUSPENDED;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    if (target.pixel_width > NX_MTL_MAX_AXIS || target.pixel_height > NX_MTL_MAX_AXIS) return NX_ERROR_LIMIT;
    if (r->broken) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
    @autoreleasepool {
        nx_result result = nx_mtl_resize(r,target.pixel_width,target.pixel_height,target.scale);
        if (result == NX_ERROR_LIMIT) return NX_OK;
        if (result == NX_ERROR_STATE) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
        if (result == NX_OK && r->width) *outcome = NX_RENDER_READY;
        return result;
    }
}
static nx_result nx_mtl_driver_submit(void *user, nx_render_target target, const nx_command *commands, size_t count,
    nx_color clear, nx_render_outcome *outcome) {
    nx_metal_renderer *r = user; *outcome = NX_RENDER_SUSPENDED;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    nx_mtl_plan preflight;
    nx_result result = nx_mtl_plan_begin(&preflight,commands,count,target.scale,target.pixel_width,target.pixel_height);
    if (result != NX_OK) return result;
    if (r->broken) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
    @autoreleasepool {
        int presented = 0;
        result = nx_mtl_resize(r,target.pixel_width,target.pixel_height,target.scale);
        if (result == NX_ERROR_LIMIT) return NX_OK;
        if (result == NX_OK) result = nx_mtl_submit(r,commands,count,target.scale,clear,nil,0,&presented);
        *outcome = r->broken ? NX_RENDER_DEVICE_LOST : presented ? NX_RENDER_READY : NX_RENDER_SUSPENDED;
        return r->broken ? NX_OK : result;
    }
}
static nx_result nx_mtl_driver_recover(void *user) { return nx_metal_renderer_recover(user); }
static nx_result nx_mtl_driver_destroy(void *user) { return nx_metal_renderer_destroy(user); }
nx_result nx_metal_driver_open(nx_window *window, nx_renderer_driver *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    if (out->open) return NX_ERROR_STATE;
    nx_metal_renderer *r = NULL; nx_result result = nx_metal_renderer_create(window,NULL,&r);
    if (result != NX_OK) return result;
    nx_render_backend backend = {r,nx_mtl_driver_resize,nx_mtl_driver_submit,nx_mtl_driver_recover,nx_mtl_driver_destroy};
    result = nx_renderer_open(out,&backend);
    if (result != NX_OK) (void)nx_metal_renderer_destroy(r);
    return result;
}

#ifdef NEXAUI_METAL_TEST_HOOKS
nx_result nx_metal_test_queue_capture(nx_metal_renderer *r, const nx_command *stream, size_t count,
    double scale, nx_color clear, uint64_t *ticket) {
    if (!ticket) return NX_ERROR_ARGUMENT;
    *ticket = 0;
    if (!r || !nx_mtl_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    nx_mtl_plan preflight;
    nx_result validation = nx_mtl_plan_begin(&preflight,stream,count,scale,0,0);
    if (validation != NX_OK) return validation;
    @autoreleasepool {
        nx_window_info info; nx_result result = nx_window_get_info(r->window,&info);
        if (result != NX_OK) return result;
        double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
        if (!width || !height || width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_ARGUMENT;
        result = nx_mtl_resize(r,(uint32_t)width,(uint32_t)height,info.scale);
        if (result != NX_OK) return result;
        size_t gpu_stride = ((size_t)width * 4u + 255u) & ~(size_t)255u;
        id<MTLBuffer> readback = [r->device newBufferWithLength:gpu_stride * r->height options:MTLResourceStorageModeShared];
        if (!readback) return NX_ERROR_MEMORY;
        int presented; result = nx_mtl_submit(r,stream,count,scale,clear,readback,gpu_stride,&presented);
        [readback release];
        if (result != NX_OK || !presented) return result != NX_OK ? result : NX_ERROR_STATE;
        *ticket = r->submitted; return NX_OK;
    }
}
nx_result nx_metal_test_read_capture(nx_metal_renderer *r, uint64_t ticket, uint8_t *pixels, size_t stride) {
    if (!r || !ticket || !pixels) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_mtl_frame_slot *slot = NULL;
        for (uint32_t i = 0; i < NX_METAL_FRAME_SLOTS; ++i)
            if (r->slots[i].submission == ticket && r->slots[i].readback) slot = &r->slots[i];
        if (!slot) return NX_ERROR_STATE;
        size_t row = (size_t)slot->read_width * 4u;
        if (stride < row || (size_t)(slot->read_height - 1) > (SIZE_MAX - row) / stride) return NX_ERROR_ARGUMENT;
        uint64_t start = nx_mtl_now();
        while (slot->pending) {
            nx_result result = nx_mtl_retire_slot(r,nx_mtl_oldest(r),1,start);
            if (result != NX_OK) return result;
        }
        const uint8_t *source = [slot->readback contents];
        for (uint32_t y = 0; y < slot->read_height; ++y) for (uint32_t x = 0; x < slot->read_width; ++x) {
            const uint8_t *s = source + y * slot->read_stride + x * 4u; uint8_t *d = pixels + y * stride + x * 4u;
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
        }
        [slot->readback release]; slot->readback = nil; return NX_OK;
    }
}
nx_result nx_metal_test_render_readback(nx_metal_renderer *r, const nx_command *stream, size_t count,
    double scale, nx_color clear, uint8_t *pixels, size_t stride) {
    if (!r || !pixels || !nx_mtl_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    nx_mtl_plan plan;
    nx_result result = nx_mtl_plan_begin(&plan,stream,count,scale,0,0);
    if (result != NX_OK) return result;
    nx_window_info info; result = nx_window_get_info(r->window,&info);
    if (result != NX_OK) return result;
    double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
    if (!width || !height || width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_ARGUMENT;
    size_t row = (size_t)width * 4u;
    if (stride < row || (size_t)(height - 1) > (SIZE_MAX - row) / stride) return NX_ERROR_ARGUMENT;
    @autoreleasepool {
        result = nx_mtl_retire(r,0);
        if (result != NX_OK) return result;
        uint64_t ticket;
        result = nx_metal_test_queue_capture(r,stream,count,scale,clear,&ticket);
        return result == NX_OK ? nx_metal_test_read_capture(r,ticket,pixels,stride) : result;
    }
}
nx_result nx_metal_test_readback(nx_metal_renderer *r, nx_color clear, uint8_t *pixels, size_t stride) {
    return nx_metal_test_render_readback(r,NULL,0,1,clear,pixels,stride);
}
nx_result nx_metal_test_fail_next(nx_metal_renderer *r, nx_metal_test_fault fault) {
    if (!r || fault < NX_MTL_TEST_DRAWABLE_UNAVAILABLE || fault > NX_MTL_TEST_WAIT_TIMEOUT) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    switch (fault) {
    case NX_MTL_TEST_DRAWABLE_UNAVAILABLE: r->drawable_unavailable = 1; break;
    case NX_MTL_TEST_CANVAS_MEMORY: r->canvas_memory = 1; break;
    case NX_MTL_TEST_HISTORY_MEMORY: r->history_memory = 1; break;
    case NX_MTL_TEST_COMMAND_MEMORY: r->command_memory = 1; break;
    case NX_MTL_TEST_PIPELINE_MEMORY: r->pipeline_memory = 1; break;
    case NX_MTL_TEST_ATLAS_MEMORY: r->atlas_memory = 1; break;
    case NX_MTL_TEST_UPLOAD_MEMORY: r->upload_memory = 1; break;
    case NX_MTL_TEST_GEOMETRY_MEMORY: r->geometry_memory = 1; break;
    case NX_MTL_TEST_WAIT_TIMEOUT: r->wait_timeouts = 2; break;
    }
    return NX_OK;
}
nx_result nx_metal_test_invalidate(nx_metal_renderer *r) {
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    r->broken = 1; return NX_OK;
}
nx_result nx_metal_test_set_per_draw_bindings(nx_metal_renderer *r, int enabled) {
    if (!r || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    r->per_draw_bindings = enabled; return NX_OK;
}
nx_result nx_metal_test_set_batching(nx_metal_renderer *r, int enabled) {
    if (!r || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    r->disable_batching = !enabled; return NX_OK;
}
nx_result nx_metal_test_set_frame_limit(nx_metal_renderer *r, uint32_t limit) {
    if (!r || !limit || limit > NX_METAL_FRAME_SLOTS) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_result result = nx_mtl_retire(r,0);
        if (result != NX_OK) return result;
        r->frame_limit = limit; r->next_slot = 0; return NX_OK;
    }
}
nx_result nx_metal_test_hold_gpu(nx_metal_renderer *r, int enabled) {
    if (!r || (enabled != 0 && enabled != 1)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    if (enabled) {
        if (r->closing || r->broken || r->pending_frames || r->test_gate) return NX_ERROR_STATE;
        r->test_gate = [r->device newSharedEvent];
        return r->test_gate ? NX_OK : NX_ERROR_UNSUPPORTED;
    }
    r->test_gate.signaledValue = 1;
    [r->test_gate release]; r->test_gate = nil; return NX_OK;
}

#endif
