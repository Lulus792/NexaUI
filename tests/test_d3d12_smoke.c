#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "renderer.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

static int pixels_match(const uint8_t *gpu, const uint8_t *reference,
                        uint32_t width, uint32_t height) {
    size_t stride = (size_t)width * 4u;
    for (size_t i = 0; i < stride * height; ++i) {
        if (gpu[i] != reference[i]) {
            fprintf(stderr, "GPU/headless mismatch at (%zu,%zu) channel %zu: %u != %u\n",
                (i / 4u) % width, i / stride,
                i % 4u, gpu[i], reference[i]);
            return 0;
        }
    }
    return 1;
}

int main(void) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI D3D12 smoke";
    desc.width = 320;
    desc.height = 200;
    desc.flags = NX_WINDOW_HIDDEN;
    nx_window *window = NULL;
    CHECK(nx_window_create(&desc, &window) == NX_OK);
    nx_d3d12_renderer *renderer = NULL;
    nx_result result = nx_d3d12_renderer_create(window, &renderer);
    if (result != NX_OK) {
        fprintf(stderr, "D3D12 renderer create: %d\n", result);
        nx_window_destroy(window);
        return 1;
    }
    int presented = 0;
    nx_color blue = {0.05f, 0.22f, 0.55f, 1};
    CHECK(nx_d3d12_clear_present(renderer,
        (nx_color){-1, 0, 0, 1}, &presented) == NX_ERROR_ARGUMENT);
    nx_d3d12_renderer_info before, after;
    CHECK(nx_d3d12_renderer_get_info(renderer, &before) == NX_OK);
    CHECK(before.pixel_width > 0 && before.pixel_height > 0 &&
          before.submitted_frames == 0);
    for (unsigned frame = 0; frame < 5; ++frame) {
        CHECK(nx_d3d12_clear_present(renderer, blue, &presented) == NX_OK);
        CHECK(nx_window_poll(window, 0) == NX_OK);
    }
    HWND hwnd = nx_window_native_hwnd(window);
    CHECK(hwnd != NULL);
    CHECK(SetWindowPos(hwnd, NULL, 0, 0, 440, 290,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(nx_window_poll(window, 0) == NX_OK);
    CHECK(nx_d3d12_clear_present(renderer,
        (nx_color){0.52f, 0.12f, 0.08f, 1}, &presented) == NX_OK);
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(after.pixel_width > before.pixel_width &&
          after.pixel_height > before.pixel_height &&
          after.submitted_frames == 6);
    nx_result removed = nx_d3d12_test_remove_device(renderer);
    if (removed == NX_OK) {
        CHECK(nx_d3d12_clear_present(renderer, blue, &presented) ==
              NX_ERROR_STATE);
        CHECK(nx_d3d12_renderer_recover(renderer) == NX_OK);
        CHECK(nx_d3d12_clear_present(renderer, blue, &presented) == NX_OK);
        CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
              after.submitted_frames == 7);
        puts("direct D3D12 removal and recovery verified");
    } else {
        CHECK(removed == NX_ERROR_UNSUPPORTED);
        puts("ID3D12Device5 removal unavailable; direct recovery skipped");
    }
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
    size_t stride = (size_t)after.pixel_width * 4u;
    size_t bytes = stride * after.pixel_height;
    uint8_t *gpu = calloc(1, bytes);
    uint8_t *reference = calloc(1, bytes);
    CHECK(gpu != NULL && reference != NULL);
    nx_command scene[3] = {0};
    scene[0].type = NX_COMMAND_FILL_RECT;
    scene[0].rect = (nx_rect){12, 14, 83, 61};
    scene[0].color = (nx_color){1, 0, 0, 1};
    scene[1].type = NX_COMMAND_FILL_TRIANGLE;
    scene[1].points[0] = (nx_point){140, 24};
    scene[1].points[1] = (nx_point){215, 37};
    scene[1].points[2] = (nx_point){172, 115};
    scene[1].color = (nx_color){0, 1, 0, 1};
    scene[2].type = NX_COMMAND_FILL_RECT;
    scene[2].rect = (nx_rect){34, 39, 28, 26};
    scene[2].color = (nx_color){0, 0, 1, 1};
    nx_color black = {0, 0, 0, 1};
    CHECK(nx_headless_render(scene, 3, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, scene, 3, 1.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(after.gpu_timestamp_frequency > 0 && after.last_timed_frame > 0);
    CHECK(isfinite(after.last_gpu_frame_ms) &&
          after.last_gpu_frame_ms > 0 && after.last_gpu_frame_ms < 10000);
    CHECK(isfinite(after.last_gpu_upload_ms) &&
          isfinite(after.last_gpu_render_ms) &&
          after.last_gpu_upload_ms >= 0 &&
          after.last_gpu_render_ms >= 0 &&
          fabs(after.last_gpu_frame_ms - after.last_gpu_upload_ms -
               after.last_gpu_render_ms) < 0.001);
    CHECK(gpu[(20u * stride) + 20u * 4u] == 255u);
    CHECK(gpu[(45u * stride) + 40u * 4u + 2u] == 255u);
    CHECK(gpu[(60u * stride) + 170u * 4u + 1u] == 255u);
    nx_command scaled[3];
    memcpy(scaled, scene, sizeof(scaled));
    for (unsigned i = 0; i < 3; ++i) {
        if (scaled[i].type == NX_COMMAND_FILL_RECT) {
            scaled[i].rect.x *= 2;
            scaled[i].rect.y *= 2;
            scaled[i].rect.width *= 2;
            scaled[i].rect.height *= 2;
        } else {
            for (unsigned p = 0; p < 3; ++p) {
                scaled[i].points[p].x *= 2;
                scaled[i].points[p].y *= 2;
            }
        }
    }
    CHECK(nx_headless_render(scaled, 3, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, scene, 3, 2.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command translucent[2] = {0};
    translucent[0].type = NX_COMMAND_FILL_RECT;
    translucent[0].rect = (nx_rect){20, 20, 50, 40};
    translucent[0].color = (nx_color){1, 0, 0, 0.5f};
    translucent[1].type = NX_COMMAND_FILL_TRIANGLE;
    translucent[1].points[0] = (nx_point){30, 25};
    translucent[1].points[1] = (nx_point){75, 25};
    translucent[1].points[2] = (nx_point){30, 65};
    translucent[1].color = (nx_color){0, 0, 1, 0.5f};
    CHECK(nx_headless_render(translucent, 2, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, translucent, 2, 1.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
    CHECK(after.last_composite_copy_texels > 0 &&
          after.last_composite_copy_texels <
              (uint64_t)after.pixel_width * after.pixel_height * 2u &&
          after.last_draw_calls == 2);
    nx_color transparent = {0, 0, 0, 0};
    CHECK(nx_headless_render(translucent, 2, reference, after.pixel_width,
        after.pixel_height, stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, translucent, 2, 1.0,
        transparent, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_color translucent_clear = {0.3f, 0.4f, 0.5f, 0.25f};
    CHECK(nx_headless_render(translucent, 2, reference, after.pixel_width,
        after.pixel_height, stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, translucent, 2, 1.0,
        translucent_clear, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command fractional[2] = {0};
    fractional[0].type = NX_COMMAND_FILL_RECT;
    fractional[0].rect = (nx_rect){-5.25f, 2.25f, 17.5f, 19.5f};
    fractional[0].color = (nx_color){0.25f, 0.5f, 0.75f, 1};
    fractional[1].type = NX_COMMAND_FILL_TRIANGLE;
    fractional[1].points[0] = (nx_point){95.25f, 60.25f};
    fractional[1].points[1] = (nx_point){45.5f, 110.25f};
    fractional[1].points[2] = (nx_point){40.5f, 35.75f};
    fractional[1].color = (nx_color){0.75f, 0.25f, 0.5f, 1};
    CHECK(nx_headless_render(fractional, 2, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, fractional, 2, 1.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command clipped[8] = {0};
    clipped[0].type = NX_COMMAND_PUSH_CLIP;
    clipped[0].rect = (nx_rect){18.25f, 16.25f, 90.5f, 70.5f};
    clipped[1].type = NX_COMMAND_FILL_RECT;
    clipped[1].rect = (nx_rect){0, 0, 130, 110};
    clipped[1].color = (nx_color){1, 0, 0, 1};
    clipped[2].type = NX_COMMAND_PUSH_CLIP;
    clipped[2].rect = (nx_rect){40.5f, 30.5f, 60.25f, 42.25f};
    clipped[3].type = NX_COMMAND_FILL_TRIANGLE;
    clipped[3].points[0] = (nx_point){25, 20};
    clipped[3].points[1] = (nx_point){125, 35};
    clipped[3].points[2] = (nx_point){45, 95};
    clipped[3].color = (nx_color){0, 0, 1, 0.5f};
    clipped[4].type = NX_COMMAND_POP_CLIP;
    clipped[5].type = NX_COMMAND_FILL_RECT;
    clipped[5].rect = (nx_rect){72, 62, 55, 35};
    clipped[5].color = (nx_color){0, 1, 0, 0.5f};
    clipped[6].type = NX_COMMAND_POP_CLIP;
    clipped[7].type = NX_COMMAND_FILL_RECT;
    clipped[7].rect = (nx_rect){132, 18, 18, 26};
    clipped[7].color = (nx_color){1, 1, 0, 1};
    CHECK(nx_headless_render(clipped, 8, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clipped, 8, 1.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command clipped_scaled[8];
    memcpy(clipped_scaled, clipped, sizeof(clipped_scaled));
    for (unsigned i = 0; i < 8; ++i) {
        nx_command *cmd = &clipped_scaled[i];
        if (cmd->type == NX_COMMAND_PUSH_CLIP ||
            cmd->type == NX_COMMAND_FILL_RECT) {
            cmd->rect.x *= 2;
            cmd->rect.y *= 2;
            cmd->rect.width *= 2;
            cmd->rect.height *= 2;
        } else if (cmd->type == NX_COMMAND_FILL_TRIANGLE) {
            for (unsigned p = 0; p < 3; ++p) {
                cmd->points[p].x *= 2;
                cmd->points[p].y *= 2;
            }
        }
    }
    CHECK(nx_headless_render(clipped_scaled, 8, reference, after.pixel_width,
        after.pixel_height, stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clipped, 8, 2.0,
        black, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    CHECK(nx_headless_render(clipped_scaled, 8, reference, after.pixel_width,
        after.pixel_height, stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clipped, 8, 2.0,
        transparent, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command clip_only[2] = {0};
    clip_only[0].type = NX_COMMAND_PUSH_CLIP;
    clip_only[0].rect = (nx_rect){10, 10, 20, 20};
    clip_only[1].type = NX_COMMAND_POP_CLIP;
    CHECK(nx_headless_render(clip_only, 2, reference, after.pixel_width,
        after.pixel_height, stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clip_only, 2, 1.0,
        translucent_clear, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    uint8_t rgba_image[32] = {
        255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 0, 17, 18, 19, 20,
        255, 255, 0, 64, 0, 255, 255, 255, 255, 255, 255, 0, 21, 22, 23, 24
    };
    uint8_t mask_image[10] = {0, 64, 255, 19, 20, 128, 255, 32, 21, 22};
    nx_glyph_point glyph_points[8] = {
        {0, 0, 1}, {4, 0, 1}, {4, 4, 1}, {0, 4, 1},
        {1, 1, 1}, {1, 3, 1}, {3, 3, 1}, {3, 1, 1}
    };
    uint16_t contour_ends[2] = {3, 7};
    nx_glyph_outline_buffer glyph_outline = {0};
    glyph_outline.points = glyph_points;
    glyph_outline.point_count = glyph_outline.point_capacity = 8;
    glyph_outline.contour_ends = contour_ends;
    glyph_outline.contour_count = glyph_outline.contour_capacity = 2;
    uint8_t rasterized_glyph[64] = {0};
    CHECK(nx_rasterize_glyph_a8(&glyph_outline, 2.0, 0.0, 8.0,
        rasterized_glyph, 8, 8, 8, sizeof(rasterized_glyph), NULL) == NX_OK);
    CHECK(rasterized_glyph[0] == 255 && rasterized_glyph[3u * 8u + 3u] == 0);
    nx_command textured[8] = {0};
    textured[0].type = NX_COMMAND_PUSH_CLIP;
    textured[0].rect = (nx_rect){12.25f, 10.5f, 110, 80};
    textured[1].type = NX_COMMAND_BITMAP;
    textured[1].rect = (nx_rect){15.25f, 14.75f, 80.5f, 49.5f};
    textured[1].color = (nx_color){1, 0.5f, 0.75f, 0.8f};
    textured[1].bitmap = (nx_bitmap_view){rgba_image, 3, 2, 16,
                                           NX_BITMAP_RGBA8};
    textured[2].type = NX_COMMAND_BITMAP;
    textured[2].rect = (nx_rect){35.5f, 22.25f, 58.25f, 48.5f};
    textured[2].color = (nx_color){0.8f, 0.2f, 0.1f, 0.7f};
    textured[2].bitmap = (nx_bitmap_view){mask_image, 3, 2, 5,
                                           NX_BITMAP_A8};
    textured[3].type = NX_COMMAND_FILL_RECT;
    textured[3].rect = (nx_rect){70, 45, 36, 30};
    textured[3].color = (nx_color){0.1f, 0.5f, 0.9f, 0.5f};
    textured[4].type = NX_COMMAND_POP_CLIP;
    textured[5].type = NX_COMMAND_BITMAP;
    textured[5].rect = (nx_rect){-8.25f, 95.5f, 17.5f, 20.25f};
    textured[5].color = (nx_color){1, 1, 1, 1};
    textured[5].bitmap = textured[1].bitmap;
    textured[6].type = NX_COMMAND_BITMAP;
    textured[6].rect = (nx_rect){140, 20, 12, 12};
    textured[6].color = (nx_color){1, 1, 1, 0};
    textured[6].bitmap = textured[2].bitmap;
    textured[7].type = NX_COMMAND_BITMAP;
    textured[7].rect = (nx_rect){160, 50, 64, 64};
    textured[7].color = (nx_color){0.9f, 0.8f, 1, 0.8f};
    textured[7].bitmap = (nx_bitmap_view){rasterized_glyph, 8, 8, 8,
                                           NX_BITMAP_A8};
    CHECK(nx_headless_render(textured, 8, reference, after.pixel_width,
        after.pixel_height, stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
        transparent, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    CHECK(nx_headless_render(textured, 8, reference, after.pixel_width,
        after.pixel_height, stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
        translucent_clear, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    nx_command textured_scaled[8];
    memcpy(textured_scaled, textured, sizeof(textured_scaled));
    for (unsigned i = 0; i < 8; ++i) {
        if (textured_scaled[i].type == NX_COMMAND_PUSH_CLIP ||
            textured_scaled[i].type == NX_COMMAND_FILL_RECT ||
            textured_scaled[i].type == NX_COMMAND_BITMAP) {
            textured_scaled[i].rect.x *= 2;
            textured_scaled[i].rect.y *= 2;
            textured_scaled[i].rect.width *= 2;
            textured_scaled[i].rect.height *= 2;
        }
    }
    CHECK(nx_headless_render(textured_scaled, 8, reference,
        after.pixel_width, after.pixel_height, stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 2.0,
        transparent, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    rgba_image[0] = 33;
    CHECK(nx_headless_render(textured, 8, reference, after.pixel_width,
        after.pixel_height, stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
        transparent, gpu, stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, after.pixel_width,
                       after.pixel_height));
    CHECK(SetWindowPos(hwnd, NULL, 0, 0, 400, 260,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(nx_window_poll(window, 0) == NX_OK);
    RECT resized_client;
    CHECK(GetClientRect(hwnd, &resized_client));
    uint32_t resized_width = (uint32_t)resized_client.right;
    uint32_t resized_height = (uint32_t)resized_client.bottom;
    CHECK(resized_width > 0 && resized_height > 0 &&
          resized_width <= after.pixel_width &&
          resized_height <= after.pixel_height);
    size_t resized_stride = (size_t)resized_width * 4u;
    CHECK(nx_headless_render(textured, 8, reference, resized_width,
        resized_height, resized_stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
        transparent, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.pixel_width == resized_width &&
          after.pixel_height == resized_height);
    removed = nx_d3d12_test_remove_device(renderer);
    if (removed == NX_OK) {
        CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
            transparent, gpu, resized_stride) == NX_ERROR_STATE);
        CHECK(nx_d3d12_renderer_recover(renderer) == NX_OK);
        CHECK(nx_d3d12_test_render_readback(renderer, textured, 8, 1.0,
            transparent, gpu, resized_stride) == NX_OK);
        CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    } else {
        CHECK(removed == NX_ERROR_UNSUPPORTED);
    }
    nx_command opaque_stress[1000] = {0};
    for (unsigned i = 0; i < 1000; ++i) {
        opaque_stress[i].type = NX_COMMAND_FILL_RECT;
        opaque_stress[i].rect = (nx_rect){2.0f + 9.0f * (i % 40u),
                                          2.0f + 8.0f * (i / 40u), 4, 4};
        opaque_stress[i].color = (nx_color){(float)(i % 5u) / 4.0f,
                                            0.4f, 0.8f, 1};
    }
    CHECK(nx_headless_render(opaque_stress, 1000, reference, resized_width,
        resized_height, resized_stride, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, opaque_stress, 1000, 1.0,
        black, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_draw_calls == 16 && after.last_bitmap_uploads == 0);
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        CHECK(nx_d3d12_test_render_readback(renderer, opaque_stress, 1000, 1.0,
            black, gpu, resized_stride) == NX_OK);
        CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    }
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
    uint64_t ring_allocations = after.vertex_ring_allocations;
    uint64_t ring_wraps = after.vertex_ring_wraps;
    for (unsigned repeat = 0; repeat < 30; ++repeat) {
        CHECK(nx_d3d12_test_render_readback(renderer, opaque_stress, 1000, 1.0,
            black, gpu, resized_stride) == NX_OK);
        CHECK(pixels_match(gpu, reference, resized_width, resized_height));
        CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK);
        CHECK(after.vertex_ring_allocations == ring_allocations);
    }
    CHECK(after.vertex_ring_wraps > ring_wraps);
    nx_command alpha_stress[100] = {0};
    for (unsigned i = 0; i < 100; ++i) {
        alpha_stress[i].type = NX_COMMAND_FILL_RECT;
        alpha_stress[i].rect = (nx_rect){5.0f + 20.0f * (i % 10u),
                                         5.0f + 18.0f * (i / 10u), 8, 8};
        alpha_stress[i].color = (nx_color){0.2f, 0.7f, 0.9f, 0.5f};
    }
    CHECK(nx_headless_render(alpha_stress, 100, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, alpha_stress, 100, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_draw_calls == 2);
    nx_command alpha_mixed[6] = {0};
    alpha_mixed[0].type = NX_COMMAND_FILL_RECT;
    alpha_mixed[0].rect = (nx_rect){8, 8, 20, 20};
    alpha_mixed[0].color = (nx_color){1, 0, 0, 0.5f};
    alpha_mixed[1].type = NX_COMMAND_FILL_TRIANGLE;
    alpha_mixed[1].points[0] = (nx_point){50, 8};
    alpha_mixed[1].points[1] = (nx_point){70, 8};
    alpha_mixed[1].points[2] = (nx_point){50, 28};
    alpha_mixed[1].color = (nx_color){0, 1, 0, 0.5f};
    alpha_mixed[2].type = NX_COMMAND_PUSH_CLIP;
    alpha_mixed[2].rect = (nx_rect){80, 8, 15, 15};
    alpha_mixed[3].type = NX_COMMAND_FILL_RECT;
    alpha_mixed[3].rect = (nx_rect){78, 6, 20, 20};
    alpha_mixed[3].color = (nx_color){0, 0, 1, 0.5f};
    alpha_mixed[4].type = NX_COMMAND_POP_CLIP;
    alpha_mixed[5].type = NX_COMMAND_FILL_RECT;
    alpha_mixed[5].rect = (nx_rect){110, 8, 20, 20};
    alpha_mixed[5].color = (nx_color){1, 1, 0, 0.5f};
    CHECK(nx_headless_render(alpha_mixed, 6, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, alpha_mixed, 6, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_draw_calls == 3);
    nx_command strokes[3] = {0};
    strokes[0].type = NX_COMMAND_STROKE_RECT;
    strokes[0].rect = (nx_rect){10.25f, 10.25f, 55.5f, 35.5f};
    strokes[0].thickness = 3.5f;
    strokes[0].color = (nx_color){1, 0.2f, 0.1f, 0.5f};
    strokes[1].type = NX_COMMAND_STROKE_RECT;
    strokes[1].rect = (nx_rect){90, 10, 20, 18};
    strokes[1].thickness = 12;
    strokes[1].color = (nx_color){0.1f, 0.8f, 0.3f, 0.8f};
    strokes[2].type = NX_COMMAND_FILL_RECT;
    strokes[2].rect = (nx_rect){12, 12, 12, 12};
    strokes[2].color = (nx_color){0.2f, 0.3f, 1, 0.5f};
    CHECK(nx_headless_render(strokes, 3, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, strokes, 3, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_draw_calls == 2);
    nx_command strokes_scaled[3];
    memcpy(strokes_scaled, strokes, sizeof(strokes_scaled));
    for (unsigned i = 0; i < 3; ++i) {
        strokes_scaled[i].rect.x *= 2;
        strokes_scaled[i].rect.y *= 2;
        strokes_scaled[i].rect.width *= 2;
        strokes_scaled[i].rect.height *= 2;
        strokes_scaled[i].thickness *= 2;
    }
    CHECK(nx_headless_render(strokes_scaled, 3, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, strokes, 3, 2.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command lines[3] = {0};
    lines[0].type = NX_COMMAND_LINE;
    lines[0].points[0] = (nx_point){10, 80};
    lines[0].points[1] = (nx_point){90, 80};
    lines[0].thickness = 5;
    lines[0].color = (nx_color){1, 0.2f, 0.1f, 0.5f};
    lines[1].type = NX_COMMAND_LINE;
    lines[1].points[0] = (nx_point){100, 60};
    lines[1].points[1] = (nx_point){160, 120};
    lines[1].thickness = 9;
    lines[1].color = (nx_color){0.1f, 0.9f, 0.2f, 0.7f};
    lines[2].type = NX_COMMAND_LINE;
    lines[2].points[0] = (nx_point){180, 80};
    lines[2].points[1] = lines[2].points[0];
    lines[2].thickness = 11;
    lines[2].color = (nx_color){0.2f, 0.4f, 1, 1};
    CHECK(nx_headless_render(lines, 3, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, lines, 3, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command lines_scaled[3];
    memcpy(lines_scaled, lines, sizeof(lines_scaled));
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned p = 0; p < 2; ++p) {
            lines_scaled[i].points[p].x *= 2;
            lines_scaled[i].points[p].y *= 2;
        }
        lines_scaled[i].thickness *= 2;
    }
    CHECK(nx_headless_render(lines_scaled, 3, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, lines, 3, 2.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command line_stress[48] = {0};
    for (unsigned i = 0; i < 48; ++i) {
        line_stress[i].type = NX_COMMAND_LINE;
        line_stress[i].points[0] = (nx_point){
            (float)((i * 37u) % 300u) - 20.25f,
            (float)((i * 29u) % 160u) - 10.75f};
        line_stress[i].points[1] = (nx_point){
            (float)((i * 53u) % 320u) - 20.5f,
            (float)((i * 47u) % 180u) - 10.25f};
        line_stress[i].thickness = 1.5f + (float)(i % 9u);
        line_stress[i].color = (nx_color){
            (float)(i % 5u) / 4.0f, 0.4f, 0.8f, 0.5f};
    }
    CHECK(nx_headless_render(line_stress, 48, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, line_stress, 48, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command clipped_line[3] = {0};
    clipped_line[0].type = NX_COMMAND_PUSH_CLIP;
    clipped_line[0].rect = (nx_rect){20.25f, 20.75f, 90.5f, 55.5f};
    clipped_line[1] = lines[1];
    clipped_line[1].points[0] = (nx_point){-5.25f, 10.5f};
    clipped_line[1].points[1] = (nx_point){125.75f, 100.25f};
    clipped_line[2].type = NX_COMMAND_POP_CLIP;
    nx_command clipped_line_scaled[3];
    memcpy(clipped_line_scaled, clipped_line,
           sizeof(clipped_line_scaled));
    clipped_line_scaled[0].rect.x *= 2;
    clipped_line_scaled[0].rect.y *= 2;
    clipped_line_scaled[0].rect.width *= 2;
    clipped_line_scaled[0].rect.height *= 2;
    for (unsigned p = 0; p < 2; ++p) {
        clipped_line_scaled[1].points[p].x *= 2;
        clipped_line_scaled[1].points[p].y *= 2;
    }
    clipped_line_scaled[1].thickness *= 2;
    CHECK(nx_headless_render(clipped_line_scaled, 3, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clipped_line, 3, 2.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command rounded[3] = {0};
    rounded[0].type = NX_COMMAND_FILL_ROUND_RECT;
    rounded[0].rect = (nx_rect){10.25f, 10.75f, 56.5f, 38.5f};
    rounded[0].radius = 12.5f;
    rounded[0].color = (nx_color){0.9f, 0.1f, 0.3f, 0.5f};
    rounded[1].type = NX_COMMAND_FILL_ROUND_RECT;
    rounded[1].rect = (nx_rect){85, 12, 20, 18};
    rounded[1].radius = 50;
    rounded[1].color = (nx_color){0.2f, 0.8f, 0.4f, 0.8f};
    rounded[2].type = NX_COMMAND_FILL_ROUND_RECT;
    rounded[2].rect = (nx_rect){120, 10, 30, 25};
    rounded[2].radius = 0;
    rounded[2].color = (nx_color){0.2f, 0.3f, 1, 1};
    CHECK(nx_headless_render(rounded, 3, reference, resized_width,
        resized_height, resized_stride, translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, rounded, 3, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command rounded_scaled[3];
    memcpy(rounded_scaled, rounded, sizeof(rounded_scaled));
    for (unsigned i = 0; i < 3; ++i) {
        rounded_scaled[i].rect.x *= 2;
        rounded_scaled[i].rect.y *= 2;
        rounded_scaled[i].rect.width *= 2;
        rounded_scaled[i].rect.height *= 2;
        rounded_scaled[i].radius *= 2;
    }
    CHECK(nx_headless_render(rounded_scaled, 3, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, rounded, 3, 2.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command rounded_stress[32] = {0};
    for (unsigned i = 0; i < 32; ++i) {
        rounded_stress[i].type = NX_COMMAND_FILL_ROUND_RECT;
        rounded_stress[i].rect = (nx_rect){
            (float)((i * 31u) % 220u) - 15.25f,
            (float)((i * 37u) % 150u) - 10.75f,
            12.5f + (float)((i * 7u) % 45u),
            10.25f + (float)((i * 11u) % 38u)};
        rounded_stress[i].radius = 0.5f + (float)((i * 5u) % 30u);
        rounded_stress[i].color = (nx_color){0.7f, 0.3f,
                                             (float)(i % 5u) / 4.0f, 0.5f};
    }
    CHECK(nx_headless_render(rounded_stress, 32, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, rounded_stress, 32, 1.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command clipped_round[3] = {0};
    clipped_round[0].type = NX_COMMAND_PUSH_CLIP;
    clipped_round[0].rect = (nx_rect){35.25f, 22.75f, 54.5f, 47.5f};
    clipped_round[1] = rounded[0];
    clipped_round[1].rect = (nx_rect){18.5f, 12.25f, 90.5f, 70.5f};
    clipped_round[1].radius = 23.5f;
    clipped_round[2].type = NX_COMMAND_POP_CLIP;
    nx_command clipped_round_scaled[3];
    memcpy(clipped_round_scaled, clipped_round,
           sizeof(clipped_round_scaled));
    for (unsigned i = 0; i < 2; ++i) {
        clipped_round_scaled[i].rect.x *= 2;
        clipped_round_scaled[i].rect.y *= 2;
        clipped_round_scaled[i].rect.width *= 2;
        clipped_round_scaled[i].rect.height *= 2;
        clipped_round_scaled[i].radius *= 2;
    }
    CHECK(nx_headless_render(clipped_round_scaled, 3, reference,
        resized_width, resized_height, resized_stride,
        translucent_clear) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, clipped_round, 3, 2.0,
        translucent_clear, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    uint8_t stress_images[260u * 4u * 4u * 4u];
    nx_command bitmap_stress[260] = {0};
    for (unsigned i = 0; i < 260; ++i) {
        uint8_t *source = stress_images + (size_t)i * 64u;
        for (unsigned p = 0; p < 16; ++p) {
            source[p * 4u] = (uint8_t)(i * 7u + p);
            source[p * 4u + 1u] = (uint8_t)(p * 13u);
            source[p * 4u + 2u] = (uint8_t)(255u - i);
            source[p * 4u + 3u] = (uint8_t)(32u + p * 12u);
        }
        bitmap_stress[i].type = NX_COMMAND_BITMAP;
        bitmap_stress[i].rect = (nx_rect){(float)((i * 37u) % 350u),
                                          (float)((i * 23u) % 190u), 8, 8};
        bitmap_stress[i].color = (nx_color){0.8f, 0.4f, 0.9f, 0.8f};
        bitmap_stress[i].bitmap = (nx_bitmap_view){source, 4, 4, 16,
                                                    NX_BITMAP_RGBA8};
    }
    CHECK(nx_headless_render(bitmap_stress, 128, reference, resized_width,
        resized_height, resized_stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, bitmap_stress, 128, 1.0,
        transparent, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_bitmap_uploads == 128 &&
          after.last_draw_calls == 128);
    uint64_t texture_creations = after.texture_creations;
    CHECK(nx_d3d12_test_render_readback(renderer, bitmap_stress, 128, 1.0,
        transparent, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.texture_creations == texture_creations);
    CHECK(nx_headless_render(bitmap_stress, 260, reference, resized_width,
        resized_height, resized_stride, transparent) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(renderer, bitmap_stress, 260, 1.0,
        transparent, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    CHECK(nx_d3d12_renderer_get_info(renderer, &after) == NX_OK &&
          after.last_bitmap_uploads == 260 &&
          after.pooled_textures <= 256);
    puts("D3D12 complete core command stream matches headless pixels");
    nx_d3d12_renderer_destroy(renderer);
    nx_renderer_driver driver = {0};
    CHECK(nx_d3d12_driver_open(window, &driver) == NX_OK);
    RECT client;
    CHECK(GetClientRect(hwnd, &client));
    nx_render_target target = {(uint32_t)(client.right - client.left),
                               (uint32_t)(client.bottom - client.top), 1};
    nx_render_outcome outcome = NX_RENDER_SUSPENDED;
    CHECK(nx_renderer_run(&driver, target, NULL, 0, blue, &outcome) == NX_OK &&
          outcome == NX_RENDER_READY);
    removed = nx_d3d12_test_remove_device(driver.backend.user);
    if (removed == NX_OK) {
        CHECK(nx_renderer_run(&driver, target, NULL, 0, blue,
                              &outcome) == NX_OK &&
              outcome == NX_RENDER_DEVICE_LOST);
        CHECK(nx_d3d12_test_fail_next_rebuild(driver.backend.user) == NX_OK);
        CHECK(nx_renderer_recover(&driver) == NX_ERROR_PLATFORM);
        CHECK(nx_renderer_run(&driver, target, NULL, 0, blue,
                              &outcome) == NX_OK &&
              outcome == NX_RENDER_DEVICE_LOST);
        CHECK(nx_renderer_recover(&driver) == NX_OK);
        CHECK(nx_renderer_run(&driver, target, NULL, 0, blue,
                              &outcome) == NX_OK &&
              outcome == NX_RENDER_READY);
        puts("F11 D3D12 failed rebuild, retry and recovery verified");
    } else {
        CHECK(removed == NX_ERROR_UNSUPPORTED);
        puts("ID3D12Device5 removal unavailable; F11 recovery skipped");
    }
    CHECK(nx_renderer_run(&driver, target, scene, 3, black,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_renderer_run(&driver, target, strokes, 3,
                          translucent_clear, &outcome) == NX_OK &&
          outcome == NX_RENDER_READY);
    CHECK(nx_renderer_run(&driver, target, lines, 3,
                          translucent_clear, &outcome) == NX_OK &&
          outcome == NX_RENDER_READY);
    CHECK(nx_renderer_run(&driver, target, rounded, 3,
                          translucent_clear, &outcome) == NX_OK &&
          outcome == NX_RENDER_READY);
    CHECK(nx_headless_render(opaque_stress, 1000, reference, resized_width,
        resized_height, resized_stride, black) == NX_OK);
    for (unsigned repeat = 0; repeat < 30; ++repeat)
        CHECK(nx_renderer_run(&driver, target, opaque_stress, 1000, black,
                              &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_d3d12_test_render_readback(driver.backend.user,
        opaque_stress, 1000, 1.0, black, gpu, resized_stride) == NX_OK);
    CHECK(pixels_match(gpu, reference, resized_width, resized_height));
    nx_command unsupported = {0};
    unsupported.type = (nx_command_type)999;
    unsupported.color = blue;
    CHECK(nx_renderer_run(&driver, target, &unsupported, 1, blue,
                           &outcome) == NX_ERROR_COMMAND);
    nx_window_info window_info = {0};
    CHECK(nx_window_get_info(window, &window_info) == NX_OK);
    RECT outer;
    CHECK(GetWindowRect(hwnd, &outer));
    UINT simulated_dpi = 144u;
    SendMessageW(hwnd, WM_DPICHANGED,
        MAKEWPARAM(simulated_dpi, simulated_dpi), (LPARAM)&outer);
    CHECK(nx_window_get_info(window, &window_info) == NX_OK &&
          fabsf(window_info.scale - 1.5f) < 0.001f);
    int saw_dpi_resize = 0;
    nx_event event;
    int available = 0;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
        if (available && event.type == NX_EVENT_RESIZE &&
            fabsf(event.scale - 1.5f) < 0.001f)
            saw_dpi_resize = 1;
    } while (available);
    CHECK(saw_dpi_resize);
    RECT dpi_client;
    CHECK(GetClientRect(hwnd, &dpi_client));
    target = (nx_render_target){(uint32_t)dpi_client.right,
                                (uint32_t)dpi_client.bottom,
                                window_info.scale};
    CHECK(fabsf(window_info.width * window_info.scale -
                (float)target.pixel_width) < 0.01f &&
          fabsf(window_info.height * window_info.scale -
                (float)target.pixel_height) < 0.01f);
    nx_command dpi_scene[3];
    memcpy(dpi_scene, scene, sizeof(dpi_scene));
    for (unsigned i = 0; i < 3; ++i) {
        if (dpi_scene[i].type == NX_COMMAND_FILL_RECT) {
            dpi_scene[i].rect.x *= 1.5f;
            dpi_scene[i].rect.y *= 1.5f;
            dpi_scene[i].rect.width *= 1.5f;
            dpi_scene[i].rect.height *= 1.5f;
        } else if (dpi_scene[i].type == NX_COMMAND_FILL_TRIANGLE) {
            for (unsigned p = 0; p < 3; ++p) {
                dpi_scene[i].points[p].x *= 1.5f;
                dpi_scene[i].points[p].y *= 1.5f;
            }
        }
    }
    CHECK(nx_d3d12_renderer_get_info(driver.backend.user, &after) == NX_OK);
    uint64_t before_suspend_frames = after.submitted_frames;
    CHECK(nx_renderer_run(&driver, (nx_render_target){0, 0, 1.5},
        scene, 3, black, &outcome) == NX_OK &&
        outcome == NX_RENDER_SUSPENDED);
    CHECK(nx_d3d12_renderer_get_info(driver.backend.user, &after) == NX_OK &&
          after.submitted_frames == before_suspend_frames);
    CHECK(nx_renderer_run(&driver, target, scene, 3, black,
        &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_headless_render(dpi_scene, 3, reference,
        target.pixel_width, target.pixel_height,
        (size_t)target.pixel_width * 4u, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(driver.backend.user,
        scene, 3, 1.5, black, gpu,
        (size_t)target.pixel_width * 4u) == NX_OK);
    CHECK(pixels_match(gpu, reference, target.pixel_width,
                       target.pixel_height));
    CHECK(SetWindowPos(hwnd, NULL, 0, 0, 300, 210,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(nx_window_poll(window, 0) == NX_OK);
    CHECK(GetClientRect(hwnd, &dpi_client));
    target.pixel_width = (uint32_t)dpi_client.right;
    target.pixel_height = (uint32_t)dpi_client.bottom;
    CHECK(target.pixel_width > 0 && target.pixel_height > 0 &&
          target.pixel_width <= resized_width &&
          target.pixel_height <= resized_height);
    CHECK(nx_renderer_run(&driver, target, scene, 3, black,
        &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(nx_headless_render(dpi_scene, 3, reference,
        target.pixel_width, target.pixel_height,
        (size_t)target.pixel_width * 4u, black) == NX_OK);
    CHECK(nx_d3d12_test_render_readback(driver.backend.user,
        scene, 3, 1.5, black, gpu,
        (size_t)target.pixel_width * 4u) == NX_OK);
    CHECK(pixels_match(gpu, reference, target.pixel_width,
                       target.pixel_height));
    removed = nx_d3d12_test_remove_device(driver.backend.user);
    if (removed == NX_OK) {
        CHECK(nx_renderer_run(&driver, target, scene, 3, black,
            &outcome) == NX_OK && outcome == NX_RENDER_DEVICE_LOST);
        CHECK(nx_renderer_recover(&driver) == NX_OK);
        CHECK(nx_renderer_run(&driver, target, scene, 3, black,
            &outcome) == NX_OK && outcome == NX_RENDER_READY);
        CHECK(nx_d3d12_test_render_readback(driver.backend.user,
            scene, 3, 1.5, black, gpu,
            (size_t)target.pixel_width * 4u) == NX_OK);
        CHECK(pixels_match(gpu, reference, target.pixel_width,
                           target.pixel_height));
        puts("F11 DPI, resize, suspension and recovery integration verified");
    } else {
        CHECK(removed == NX_ERROR_UNSUPPORTED);
        puts("F11 DPI, resize and suspension verified; device removal unavailable");
    }
    nx_renderer_close(&driver);
    free(reference);
    free(gpu);
    nx_window_destroy(window);
    return 0;
}
