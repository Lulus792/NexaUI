#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#include "renderer.h"
#include "window_internal.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NX_MTL_MAX_AXIS 8192u
#define NX_MTL_WAIT_NS UINT64_C(1000000000)

struct nx_metal_renderer {
    nx_allocator allocator;
    nx_window *window;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    CAMetalLayer *layer;
    CALayer *previous_layer;
    BOOL previous_wants_layer;
    id<MTLTexture> canvas;
    id<MTLCommandBuffer> pending;
    id<MTLBuffer> readback;
    uint32_t width, height;
    uint64_t submitted, completed;
    int leased, closing, broken;
#ifdef NEXAUI_METAL_TEST_HOOKS
    unsigned drawable_unavailable, canvas_memory, command_memory, wait_timeouts;
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
/* Poll a retained command buffer instead of an unbounded waitUntilCompleted.
   No asynchronous callback retains or dereferences the C renderer. */
static nx_result nx_mtl_retire(nx_metal_renderer *r, int keep_readback) {
    if (!r->pending) {
        if (!keep_readback) { [r->readback release]; r->readback = nil; }
        return NX_OK;
    }
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->wait_timeouts) { --r->wait_timeouts; return NX_ERROR_LIMIT; }
#endif
    uint64_t start = nx_mtl_now();
    if (!start) return NX_ERROR_PLATFORM;
    for (;;) {
        MTLCommandBufferStatus status = [r->pending status];
        if (status == MTLCommandBufferStatusCompleted || status == MTLCommandBufferStatusError) {
            [r->pending release]; r->pending = nil;
            if (!keep_readback || status == MTLCommandBufferStatusError) { [r->readback release]; r->readback = nil; }
            ++r->completed;
            if (status == MTLCommandBufferStatusError) { r->broken = 1; return NX_ERROR_STATE; }
            return NX_OK;
        }
        uint64_t now = nx_mtl_now();
        if (!now) return NX_ERROR_PLATFORM;
        if (now - start >= NX_MTL_WAIT_NS) return NX_ERROR_LIMIT;
        struct timespec pause = {0,1000000}; nanosleep(&pause,NULL);
    }
}
static nx_result nx_mtl_device(nx_metal_renderer *r) {
    r->device = MTLCreateSystemDefaultDevice(); /* Create rule: owned reference. */
    if (!r->device) return NX_ERROR_UNSUPPORTED;
    r->queue = [r->device newCommandQueue];
    if (!r->queue) { [r->device release]; r->device = nil; return NX_ERROR_MEMORY; }
    [r->queue setLabel:@"NexaUI UI queue"];
    return NX_OK;
}
static nx_result nx_mtl_resize(nx_metal_renderer *r, uint32_t width, uint32_t height, double scale) {
    if (!isfinite(scale) || scale <= 0) return NX_ERROR_ARGUMENT;
    if (width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_LIMIT;
    if (!width || !height) width = height = 0;
    nx_result result = nx_mtl_retire(r,0);
    if (result != NX_OK) return result;
    if (width != r->width || height != r->height) {
        id<MTLTexture> texture = nil;
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
        }
        [r->canvas release]; r->canvas = texture; r->width = width; r->height = height;
    }
    NSView *view = nx_window_native_nsview(r->window);
    [r->layer setFrame:[view bounds]]; [r->layer setContentsScale:scale];
    [r->layer setDrawableSize:CGSizeMake(width,height)];
    return NX_OK;
}
static nx_result nx_mtl_submit(nx_metal_renderer *r, nx_color clear,
    id<MTLBuffer> readback, size_t read_stride, int *presented) {
    *presented = 0;
    if (r->closing || r->broken) return NX_ERROR_STATE;
    if (!r->width || !r->height) return NX_OK;
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->command_memory) { r->command_memory = 0; return NX_ERROR_MEMORY; }
#endif
    id<MTLCommandBuffer> commands = [r->queue commandBuffer];
    if (!commands) return NX_ERROR_MEMORY;
    commands.label = @"NexaUI clear/present";
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = r->canvas;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(nx_mtl_byte(clear.r), nx_mtl_byte(clear.g), nx_mtl_byte(clear.b), nx_mtl_byte(clear.a));
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
    if (!encoder) return NX_ERROR_MEMORY;
    [encoder endEncoding];
    id<MTLBlitCommandEncoder> copy = [commands blitCommandEncoder];
    if (!copy) return NX_ERROR_MEMORY;
    if (readback) [copy copyFromTexture:r->canvas sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(r->width,r->height,1) toBuffer:readback destinationOffset:0
        destinationBytesPerRow:read_stride destinationBytesPerImage:read_stride * r->height];
    [copy endEncoding];
#ifdef NEXAUI_METAL_TEST_HOOKS
    if (r->drawable_unavailable) { r->drawable_unavailable = 0; return NX_OK; }
#endif
    [CATransaction flush]; /* Commit layer geometry in manually pumped C apps. */
    id<CAMetalDrawable> drawable = [r->layer nextDrawable];
    if (!drawable) return NX_OK; /* Layer acquisition has a one-second timeout. */
    if (drawable.texture.width != r->width || drawable.texture.height != r->height) return NX_OK;
    copy = [commands blitCommandEncoder];
    if (!copy) return NX_ERROR_MEMORY;
    [copy copyFromTexture:r->canvas sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
        sourceSize:MTLSizeMake(r->width,r->height,1) toTexture:drawable.texture destinationSlice:0 destinationLevel:0
        destinationOrigin:MTLOriginMake(0,0,0)];
    [copy endEncoding];
    [commands presentDrawable:drawable];
    r->pending = [commands retain];
    if (readback) r->readback = [readback retain];
    [commands commit]; ++r->submitted; *presented = 1;
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
    *r = (nx_metal_renderer){0}; r->allocator = a; r->window = window;
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
    if (count > UINT32_MAX / 6u) return NX_ERROR_LIMIT;
    nx_result result = nx_render_validate_commands(commands,count);
    if (result != NX_OK) return result;
    if (count) return NX_ERROR_UNSUPPORTED;
    if (r->closing || r->broken) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_window_info info; result = nx_window_get_info(r->window,&info);
        if (result != NX_OK) return result;
        double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
        if (width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_LIMIT;
        result = nx_mtl_resize(r,(uint32_t)width,(uint32_t)height,info.scale);
        if (result == NX_ERROR_LIMIT) return NX_OK;
        return result == NX_OK ? nx_mtl_submit(r,clear,nil,0,presented) : result;
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
        out->pending = r->pending != nil; out->pending_readback = r->readback != nil;
        out->closing = r->closing; out->broken = r->broken; return NX_OK;
    }
}
nx_result nx_metal_renderer_recover(nx_metal_renderer *r) {
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_result result = nx_mtl_retire(r,0);
        if (r->pending) return result;
        [r->canvas release]; r->canvas = nil; r->width = r->height = 0;
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
        if (r->pending) return result;
        NSView *view = nx_window_native_nsview(r->window);
        if (r->layer && [view layer] == r->layer) {
            [view setLayer:r->previous_layer]; [view setWantsLayer:r->previous_wants_layer];
        }
        [r->previous_layer release]; [r->layer release]; [r->canvas release];
        [r->readback release]; [r->queue release]; [r->device release];
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
    nx_result result = nx_render_validate_commands(commands,count);
    if (result != NX_OK) return result;
    if (count) return NX_ERROR_UNSUPPORTED;
    if (r->broken) { *outcome = NX_RENDER_DEVICE_LOST; return NX_OK; }
    @autoreleasepool {
        int presented = 0;
        result = nx_mtl_resize(r,target.pixel_width,target.pixel_height,target.scale);
        if (result == NX_ERROR_LIMIT) return NX_OK;
        if (result == NX_OK) result = nx_mtl_submit(r,clear,nil,0,&presented);
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
nx_result nx_metal_test_readback(nx_metal_renderer *r, nx_color clear, uint8_t *pixels, size_t stride) {
    if (!r || !pixels || !nx_mtl_color(clear)) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing || r->broken) return NX_ERROR_STATE;
    @autoreleasepool {
        nx_window_info info; nx_result result = nx_window_get_info(r->window,&info);
        if (result != NX_OK) return result;
        double width = ceil((double)info.width * info.scale), height = ceil((double)info.height * info.scale);
        if (!width || !height || width > NX_MTL_MAX_AXIS || height > NX_MTL_MAX_AXIS) return NX_ERROR_ARGUMENT;
        size_t row = (size_t)width * 4u;
        if (stride < row || (size_t)(height - 1) > (SIZE_MAX - row) / stride) return NX_ERROR_ARGUMENT;
        result = nx_mtl_resize(r,(uint32_t)width,(uint32_t)height,info.scale);
        if (result != NX_OK) return result;
        size_t gpu_stride = (row + 255u) & ~(size_t)255u;
        id<MTLBuffer> readback = [r->device newBufferWithLength:gpu_stride * r->height options:MTLResourceStorageModeShared];
        if (!readback) return NX_ERROR_MEMORY;
        int presented; result = nx_mtl_submit(r,clear,readback,gpu_stride,&presented);
        [readback release];
        if (result != NX_OK || !presented) return result != NX_OK ? result : NX_ERROR_STATE;
        result = nx_mtl_retire(r,1);
        if (result != NX_OK) return result;
        const uint8_t *source = [r->readback contents];
        for (uint32_t y = 0; y < r->height; ++y) for (uint32_t x = 0; x < r->width; ++x) {
            const uint8_t *s = source + y * gpu_stride + x * 4u; uint8_t *d = pixels + y * stride + x * 4u;
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
        }
        [r->readback release]; r->readback = nil; return NX_OK;
    }
}
nx_result nx_metal_test_fail_next(nx_metal_renderer *r, nx_metal_test_fault fault) {
    if (!r || fault < NX_MTL_TEST_DRAWABLE_UNAVAILABLE || fault > NX_MTL_TEST_WAIT_TIMEOUT) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r)) return NX_ERROR_STATE;
    switch (fault) {
    case NX_MTL_TEST_DRAWABLE_UNAVAILABLE: r->drawable_unavailable = 1; break;
    case NX_MTL_TEST_CANVAS_MEMORY: r->canvas_memory = 1; break;
    case NX_MTL_TEST_COMMAND_MEMORY: r->command_memory = 1; break;
    case NX_MTL_TEST_WAIT_TIMEOUT: r->wait_timeouts = 2; break;
    }
    return NX_OK;
}
nx_result nx_metal_test_invalidate(nx_metal_renderer *r) {
    if (!r) return NX_ERROR_ARGUMENT;
    if (!nx_mtl_owner(r) || r->closing) return NX_ERROR_STATE;
    r->broken = 1; return NX_OK;
}
#endif
