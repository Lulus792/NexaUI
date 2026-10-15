/* Private C17 diagnostic workload; included by the native Metal test build.
   No readback, file I/O or sorting occurs inside a measured frame interval. */
#define CLOUD_PROFILE_SAMPLES 32u
typedef struct cloud_profile_sample {
    nx_metal_frame_profile frame;
    double wall_ms;
    uint64_t retirement_ns;
} cloud_profile_sample;

static int profile_double_compare(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void profile_distribution(const char *name, double *values) {
    qsort(values, CLOUD_PROFILE_SAMPLES, sizeof(*values), profile_double_compare);
    printf(" %s median=%.3f p95=%.3f min=%.3f max=%.3f ms;", name,
        (values[15] + values[16]) * .5, values[30], values[0], values[31]);
}

static int profile_workload(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
                             const char *prefix, const char *workload) {
    for (unsigned i = 0; i < 4; ++i) if (!frame(window, scene, renderer, 0)) return 0;
    nx_metal_renderer_info previous, current;
    if (nx_metal_renderer_get_info(renderer->backend.user, &previous) != NX_OK) return 0;
    uint64_t atlas_allocations = previous.atlas_allocations, uploads = previous.bitmap_upload_allocations;
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
            p.draw_calls > p.draw_records || p.render_passes != p.history_copies + 1 || p.history_copies > p.draw_records ||
            p.draw_argument_bytes != p.draw_records * 100 ||
            p.present_copy_bytes != (uint64_t)p.pixel_width * p.pixel_height * 4u ||
            p.pixel_width != current.pixel_width || p.pixel_height != current.pixel_height ||
            current.retirement_wait_ns < previous.retirement_wait_ns || current.retirement_clock_errors ||
            current.atlas_allocations != atlas_allocations || current.bitmap_upload_allocations != uploads)
            return 0;
        const double phases[] = {p.prepare_ms, p.encode_ms, p.acquire_ms, p.commit_ms};
        double phase_total = 0;
        for (unsigned j = 0; j < 4; ++j) {
            if (!isfinite(phases[j]) || phases[j] < 0) return 0;
            phase_total += phases[j];
        }
        samples[i] = (cloud_profile_sample){p, (end - start) * 1000,
            current.retirement_wait_ns - previous.retirement_wait_ns};
        if (phase_total > samples[i].wall_ms + .01) return 0;
        previous = current;
    }
    char path[1024];
    if (snprintf(path, sizeof(path), "%s-macos-profile-%s.csv", prefix, workload) >= (int)sizeof(path)) return 0;
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    int okay = fprintf(file, "workload,pixel_width,pixel_height,draw_scale,text_scale,submission,input_commands,draw_records,draw_calls,render_passes,history_copies,history_copy_bytes,bitmap_upload_bytes,draw_argument_bytes,present_copy_bytes,prepare_ms,encode_ms,acquire_ms,commit_ms,retirement_ms,wall_ms\n") > 0;
    for (unsigned i = 0; okay && i < CLOUD_PROFILE_SAMPLES; ++i) {
        nx_metal_frame_profile p = samples[i].frame;
        okay = fprintf(file, "%s,%u,%u,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
            workload, p.pixel_width, p.pixel_height, p.draw_scale, cloud_text_scale(scene),
            (unsigned long long)p.submission, (unsigned long long)p.input_commands, (unsigned long long)p.draw_records, (unsigned long long)p.draw_calls,
            (unsigned long long)p.render_passes, (unsigned long long)p.history_copies,
            (unsigned long long)p.history_copy_bytes, (unsigned long long)p.bitmap_upload_bytes,
            (unsigned long long)p.draw_argument_bytes, (unsigned long long)p.present_copy_bytes,
            p.prepare_ms, p.encode_ms, p.acquire_ms, p.commit_ms, (double)samples[i].retirement_ns / 1.e6, samples[i].wall_ms) > 0;
    }
    if (fclose(file)) okay = 0;
    printf("Metal CPU baseline %s: %s, %ux%u, %u samples, 4 warmup, last frame %llu draws / %llu history copies / %llu atlas bytes.\n",
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
        profile_distribution(names[phase], values);
    }
    putchar('\n');
    if (!okay || snprintf(path, sizeof(path), "%s-macos-profile-%s.ppm", prefix, workload) >= (int)sizeof(path)) return 0;
    return capture(window, scene, renderer, path);
}
