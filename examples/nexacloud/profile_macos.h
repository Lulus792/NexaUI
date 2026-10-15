/* Private C17 diagnostic workload; included by the native Metal test build.
   No readback, file I/O or sorting occurs inside a measured frame interval. */
#define CLOUD_PROFILE_SAMPLES 32u
typedef struct cloud_profile_sample {
    nx_metal_frame_profile frame;
    double wall_ms;
    uint64_t retirement_ns;
    nx_metal_gpu_timing gpu;
} cloud_profile_sample;

static int profile_double_compare(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void profile_distribution(const char *name, double *values, unsigned count) {
    if (!count) { printf(" %s unavailable;",name); return; }
    qsort(values, count, sizeof(*values), profile_double_compare);
    printf(" %s median=%.3f p95=%.3f min=%.3f max=%.3f ms;", name,
        (values[(count-1)/2] + values[count/2]) * .5, values[(95u*count+99u)/100u-1u], values[0], values[count-1]);
}

static int profile_workload(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
                             const char *prefix, const char *workload) {
    for (unsigned i = 0; i < 4; ++i) if (!frame(window, scene, renderer, 0)) return 0;
    nx_metal_renderer_info previous, current;
    if (nx_metal_renderer_get_info(renderer->backend.user, &previous) != NX_OK) return 0;
    uint64_t atlas_allocations = previous.atlas_allocations, uploads = previous.bitmap_upload_allocations;
    uint64_t record_allocations = previous.record_upload_allocations;
    cloud_profile_sample samples[CLOUD_PROFILE_SAMPLES];
    for (unsigned i = 0; i < CLOUD_PROFILE_SAMPLES; ++i) {
        double start = now();
        if (start < 0 || !frame(window, scene, renderer, 0)) return 0;
        double end = now();
        if (end < start || nx_metal_renderer_get_info(renderer->backend.user, &current) != NX_OK) return 0;
        nx_metal_frame_profile p = current.last_frame;
        const nx_command *commands; size_t count;
        if (nx_frame_commands(cloud_context(scene), &commands, &count) != NX_OK) return 0;
        (void)commands;
        if (current.submitted_frames != previous.submitted_frames + 1 || p.submission != current.submitted_frames ||
            !p.cpu_timing_valid || p.input_commands != count || !p.draw_calls ||
            !p.frame_limit || p.frame_limit > NX_METAL_FRAME_SLOTS || p.frame_slot >= p.frame_limit ||
            p.pending_frames != current.pending_frames || p.pending_frames > p.frame_limit ||
            p.draw_calls > p.draw_records || p.render_passes != p.history_copies + 1 || p.history_copies > p.draw_records ||
            p.bound_passes > p.render_passes || p.fixed_state_bindings != 5 * (p.per_draw_bindings ? p.draw_calls : p.bound_passes) ||
            (p.batched ? p.record_upload_bytes != p.draw_records * 116 || p.draw_argument_bytes ||
                p.record_buffer_bindings != p.fixed_state_bindings / 5 * 2 :
                p.draw_argument_bytes != p.draw_records * 100 || p.record_upload_bytes || p.record_buffer_bindings) ||
            p.present_copy_bytes != (uint64_t)p.pixel_width * p.pixel_height * 4u ||
            p.pixel_width != current.pixel_width || p.pixel_height != current.pixel_height ||
            current.retirement_wait_ns < previous.retirement_wait_ns || current.retirement_clock_errors ||
            current.atlas_allocations != atlas_allocations || current.bitmap_upload_allocations != uploads ||
            current.record_upload_allocations != record_allocations)
            return 0;
        const double phases[] = {p.prepare_ms, p.encode_ms, p.acquire_ms, p.commit_ms};
        double phase_total = 0;
        for (unsigned j = 0; j < 4; ++j) {
            if (!isfinite(phases[j]) || phases[j] < 0) return 0;
            phase_total += phases[j];
        }
        samples[i] = (cloud_profile_sample){p, (end - start) * 1000,
            current.retirement_wait_ns - previous.retirement_wait_ns,{0}};
        if (phase_total > samples[i].wall_ms + .01) return 0;
        previous = current;
    }
    /* No extra synchronization inside the measured frame interval. Drain the
       final tail afterward, then join GPU results by immutable submission ID.
       The fixed history retains all 32 samples, including the three-slot tail. */
    double drain_start = now();
    if (drain_start < 0 || nx_metal_renderer_wait_idle(renderer->backend.user) != NX_OK) return 0;
    double drain_end = now();
    if (drain_end < drain_start) return 0;
    const char *require = getenv("NEXAUI_REQUIRE_METAL_TIMING");
    unsigned valid_gpu = 0, valid_kernel = 0;
    for (unsigned i = 0; i < CLOUD_PROFILE_SAMPLES; ++i) {
        nx_metal_gpu_timing *gpu = &samples[i].gpu;
        nx_metal_frame_profile p = samples[i].frame;
        if (nx_metal_renderer_get_timing(renderer->backend.user,p.submission,gpu) != NX_OK ||
            gpu->submission != p.submission || gpu->frame_slot != p.frame_slot ||
            gpu->pixel_width != p.pixel_width || gpu->pixel_height != p.pixel_height ||
            (gpu->state != NX_MTL_TIMING_READY && gpu->state != NX_MTL_TIMING_UNAVAILABLE)) return 0;
        if (gpu->state == NX_MTL_TIMING_READY) ++valid_gpu;
        if (gpu->kernel_timing_valid) ++valid_kernel;
    }
    if (require && !strcmp(require,"1") && valid_gpu != CLOUD_PROFILE_SAMPLES) {
        fprintf(stderr,"Metal GPU timestamps required: only %u/%u valid in %s\n",valid_gpu,CLOUD_PROFILE_SAMPLES,workload);
        return 0;
    }
    char path[1024];
    if (snprintf(path, sizeof(path), "%s-macos-profile-%s.csv", prefix, workload) >= (int)sizeof(path)) return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int okay = fprintf(file, "workload,pixel_width,pixel_height,draw_scale,text_scale,submission,input_commands,draw_records,draw_calls,render_passes,bound_passes,fixed_state_bindings,per_draw_bindings,batched,history_copies,history_copy_bytes,bitmap_upload_bytes,draw_argument_bytes,record_upload_bytes,record_buffer_bindings,present_copy_bytes,frame_slot,frame_limit,pending_frames,prepare_ms,encode_ms,acquire_ms,commit_ms,retirement_ms,wall_ms,gpu_submission,gpu_frame_slot,gpu_state,gpu_start_seconds,gpu_end_seconds,gpu_ms,kernel_timing_valid,kernel_start_seconds,kernel_end_seconds,kernel_ms\n") > 0;
    for (unsigned i = 0; okay && i < CLOUD_PROFILE_SAMPLES; ++i) {
        nx_metal_frame_profile p = samples[i].frame;
        nx_metal_gpu_timing gpu = samples[i].gpu;
        okay = fprintf(file, "%s,%u,%u,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%llu,%u,%d,%.9f,%.9f,%.6f,%d,%.9f,%.9f,%.6f\n",
            workload, p.pixel_width, p.pixel_height, p.draw_scale, cloud_text_scale(scene),
            (unsigned long long)p.submission, (unsigned long long)p.input_commands, (unsigned long long)p.draw_records, (unsigned long long)p.draw_calls,
            (unsigned long long)p.render_passes, (unsigned long long)p.bound_passes,
            (unsigned long long)p.fixed_state_bindings, p.per_draw_bindings, p.batched, (unsigned long long)p.history_copies,
            (unsigned long long)p.history_copy_bytes, (unsigned long long)p.bitmap_upload_bytes,
            (unsigned long long)p.draw_argument_bytes, (unsigned long long)p.record_upload_bytes,
            (unsigned long long)p.record_buffer_bindings, (unsigned long long)p.present_copy_bytes,
            p.frame_slot, p.frame_limit, p.pending_frames,
            p.prepare_ms, p.encode_ms, p.acquire_ms, p.commit_ms, (double)samples[i].retirement_ns / 1.e6, samples[i].wall_ms,
            (unsigned long long)gpu.submission, gpu.frame_slot, (int)gpu.state,
            gpu.gpu_start_seconds,gpu.gpu_end_seconds,gpu.gpu_ms,gpu.kernel_timing_valid,
            gpu.kernel_start_seconds,gpu.kernel_end_seconds,gpu.kernel_ms) > 0;
    }
    if (fclose(file)) okay = 0;
    printf("Metal CPU/GPU baseline %s: %s, %ux%u, %u samples, 4 warmup, last frame %llu draws / %llu history copies / %llu atlas bytes.\n",
        workload, previous.device_name, previous.pixel_width, previous.pixel_height, CLOUD_PROFILE_SAMPLES,
        (unsigned long long)previous.last_frame.draw_calls, (unsigned long long)previous.last_frame.history_copies,
        (unsigned long long)previous.last_frame.bitmap_upload_bytes);
    double values[CLOUD_PROFILE_SAMPLES];
    for (unsigned phase = 0; phase < 6; ++phase) {
        for (unsigned i = 0; i < CLOUD_PROFILE_SAMPLES; ++i) {
            nx_metal_frame_profile p = samples[i].frame;
            values[i] = phase == 0 ? samples[i].wall_ms : phase == 1 ? p.prepare_ms : phase == 2 ? p.encode_ms :
                phase == 3 ? p.acquire_ms : phase == 4 ? p.commit_ms : (double)samples[i].retirement_ns / 1.e6;
        }
        static const char *names[] = {"wall", "prepare", "encode", "acquire", "commit", "retirement"};
        profile_distribution(names[phase], values,CLOUD_PROFILE_SAMPLES);
    }
    unsigned count = 0;
    for (unsigned i = 0; i < CLOUD_PROFILE_SAMPLES; ++i)
        if (samples[i].gpu.state == NX_MTL_TIMING_READY) values[count++] = samples[i].gpu.gpu_ms;
    profile_distribution("GPU command buffer",values,count);
    count = 0;
    for (unsigned i = 0; i < CLOUD_PROFILE_SAMPLES; ++i)
        if (samples[i].gpu.kernel_timing_valid) values[count++] = samples[i].gpu.kernel_ms;
    profile_distribution("CPU driver scheduling",values,count);
    printf(" GPU valid=%u/%u, kernel valid=%u/%u; tail drain=%.3f ms (outside measurement);",
        valid_gpu,CLOUD_PROFILE_SAMPLES,valid_kernel,CLOUD_PROFILE_SAMPLES,(drain_end-drain_start)*1000.);
    putchar('\n');
    if (!okay || snprintf(path, sizeof(path), "%s-macos-profile-%s.ppm", prefix, workload) >= (int)sizeof(path)) return 0;
    return capture(window, scene, renderer, path);
}

static int profile_compare(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
                            const char *prefix, const char *workload) {
    /* Serial/ring/ring/serial keeps both paths on the same device,
       scene and build, with allocation stability checked after each warmup.
       Each block warms independently so retirement samples belong to its mode. */
    static const uint32_t limits[] = {1,3,3,1};
    for (unsigned block = 0; block < 4; ++block) {
        char name[80];
        if (snprintf(name, sizeof(name), "%s-%s-%u", workload, limits[block] == 3 ? "ring" : "serial", block) >= (int)sizeof(name) ||
            nx_metal_test_set_per_draw_bindings(renderer->backend.user,0) != NX_OK ||
            nx_metal_test_set_batching(renderer->backend.user,1) != NX_OK ||
            nx_metal_test_set_frame_limit(renderer->backend.user,limits[block]) != NX_OK ||
            !profile_workload(window,scene,renderer,prefix,name)) return 0;
    }
    return nx_metal_test_set_frame_limit(renderer->backend.user,3) == NX_OK;
}
