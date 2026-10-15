/* Native Vulkan CPU workload. No readback, sorting or file I/O occurs inside
   the measured frame interval. Included only by the diagnostic test build. */
#define CLOUD_VK_PROFILE_SAMPLES 32u
typedef struct cloud_vk_profile_sample {
    nx_vulkan_frame_profile frame;
    nx_vulkan_renderer_info info;
    double wall_ms;
} cloud_vk_profile_sample;
static int vk_profile_double_compare(const void *a, const void *b) {
    double x=*(const double *)a, y=*(const double *)b; return (x>y)-(x<y);
}
static void vk_profile_distribution(const char *name, double *values) {
    qsort(values,CLOUD_VK_PROFILE_SAMPLES,sizeof(*values),vk_profile_double_compare);
    printf(" %s median=%.3f p95=%.3f min=%.3f max=%.3f ms;",name,
        (values[15]+values[16])*.5,values[30],values[0],values[31]);
}
static int vk_profile_measure(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
    const char *prefix, const char *workload) {
    for (unsigned i=0;i<4;++i) if (!frame(window,scene,renderer,0)) return 0;
    nx_vulkan_renderer_info previous,current;
    if (nx_vulkan_renderer_get_info(renderer->backend.user,&previous) != NX_OK) return 0;
    nx_vulkan_frame_profile geometry=previous.last_frame;
    uint64_t vertices=previous.vertex_buffer_allocations, atlases=previous.atlas_allocations, uploads=previous.bitmap_upload_allocations;
    cloud_vk_profile_sample samples[CLOUD_VK_PROFILE_SAMPLES];
    for (unsigned i=0;i<CLOUD_VK_PROFILE_SAMPLES;++i) {
        double start=now();
        if (start<0 || !frame(window,scene,renderer,0)) return 0;
        double end=now();
        if (end<start || nx_vulkan_renderer_get_info(renderer->backend.user,&current) != NX_OK) return 0;
        nx_vulkan_frame_profile p=current.last_frame;
        const nx_command *commands; size_t count;
        if (nx_frame_commands(cloud_context(scene),&commands,&count) != NX_OK) return 0;
        (void)commands;
        if (current.submitted_frames != previous.submitted_frames+1 || p.submission != current.submitted_frames ||
            !p.cpu_timing_valid || p.input_commands != count || !p.draw_records || !p.draw_calls || p.draw_calls>p.draw_records ||
            p.frames_in_flight != current.frame_slots || p.frame_slot >= current.frame_slots ||
            p.frame_slot != (previous.last_frame.frame_slot+1u)%current.frame_slots ||
            current.pending_submissions>current.frame_slots || current.pending_captures ||
            p.frames_in_flight != geometry.frames_in_flight ||
            p.vertex_upload_bytes != p.draw_records*84u ||
            (p.batched ? p.draw_calls != p.render_passes : p.draw_calls != p.draw_records) ||
            p.render_passes<p.history_copies || p.render_passes>p.history_copies+1 ||
            p.pixel_width != current.pixel_width || p.pixel_height != current.pixel_height ||
            p.present_copy_bytes != (uint64_t)p.pixel_width*p.pixel_height*4u ||
            current.vertex_buffer_allocations != vertices || current.atlas_allocations != atlases || current.bitmap_upload_allocations != uploads) return 0;
        if (p.pixel_width != geometry.pixel_width || p.pixel_height != geometry.pixel_height || p.draw_scale != geometry.draw_scale ||
            p.input_commands != geometry.input_commands || p.draw_records != geometry.draw_records || p.draw_calls != geometry.draw_calls ||
            p.render_passes != geometry.render_passes || p.history_copies != geometry.history_copies ||
            p.history_copy_bytes != geometry.history_copy_bytes || p.bitmap_upload_bytes != geometry.bitmap_upload_bytes ||
            p.vertex_upload_bytes != geometry.vertex_upload_bytes || p.batched != geometry.batched) return 0;
        const double phases[]={p.retirement_ms,p.prepare_ms,p.acquire_ms,p.encode_ms,p.submit_ms,p.present_ms};
        double total=0;
        for (unsigned j=0;j<6;++j) { if (!isfinite(phases[j]) || phases[j]<0) return 0; total+=phases[j]; }
        samples[i]=(cloud_vk_profile_sample){p,current,(end-start)*1000.};
        if (total>samples[i].wall_ms+.01) return 0;
        previous=current;
    }
    const char *backend=getenv("NEXAUI_PLATFORM"); if (!backend) backend="auto";
    char path[1024];
    if (snprintf(path,sizeof(path),"%s-linux-profile-%s.csv",prefix,workload)>=(int)sizeof(path)) return 0;
    FILE *file=fopen(path,"wb"); if (!file) return 0;
    int okay=fprintf(file,"workload,backend,pixel_width,pixel_height,draw_scale,text_scale,submission,input_commands,draw_records,draw_calls,render_passes,history_copies,history_copy_bytes,vertex_upload_bytes,bitmap_upload_bytes,present_copy_bytes,batched,retirement_ms,prepare_ms,acquire_ms,encode_ms,submit_ms,present_ms,wall_ms,frame_slot,frame_slots,retired_frames,pending_submissions,vertex_capacity_bytes,bitmap_capacity_bytes\n")>0;
    for (unsigned i=0;okay && i<CLOUD_VK_PROFILE_SAMPLES;++i) {
        nx_vulkan_frame_profile p=samples[i].frame;
        nx_vulkan_renderer_info info=samples[i].info;
        okay=fprintf(file,"%s,%s,%u,%u,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%u,%u,%llu,%u,%llu,%llu\n",
            workload,backend,p.pixel_width,p.pixel_height,p.draw_scale,cloud_text_scale(scene),
            (unsigned long long)p.submission,(unsigned long long)p.input_commands,(unsigned long long)p.draw_records,
            (unsigned long long)p.draw_calls,(unsigned long long)p.render_passes,(unsigned long long)p.history_copies,
            (unsigned long long)p.history_copy_bytes,(unsigned long long)p.vertex_upload_bytes,
            (unsigned long long)p.bitmap_upload_bytes,(unsigned long long)p.present_copy_bytes,p.batched,
            p.retirement_ms,p.prepare_ms,p.acquire_ms,p.encode_ms,p.submit_ms,p.present_ms,samples[i].wall_ms,
            p.frame_slot,p.frames_in_flight,(unsigned long long)info.retired_frames,info.pending_submissions,
            (unsigned long long)info.vertex_capacity_bytes,(unsigned long long)info.bitmap_upload_capacity_bytes)>0;
    }
    if (fclose(file)) okay=0;
    printf("Vulkan CPU baseline %s (%s): %s, %ux%u, 32 samples, 4 warmup, %llu records / %llu draws / %llu history copies / %llu vertex bytes.\n",
        workload,backend,previous.device.name,previous.pixel_width,previous.pixel_height,
        (unsigned long long)previous.last_frame.draw_records,(unsigned long long)previous.last_frame.draw_calls,
        (unsigned long long)previous.last_frame.history_copies,(unsigned long long)previous.last_frame.vertex_upload_bytes);
    double values[CLOUD_VK_PROFILE_SAMPLES];
    for (unsigned phase=0;phase<7;++phase) {
        for (unsigned i=0;i<CLOUD_VK_PROFILE_SAMPLES;++i) {
            nx_vulkan_frame_profile p=samples[i].frame;
            const double times[]={samples[i].wall_ms,p.retirement_ms,p.prepare_ms,p.acquire_ms,p.encode_ms,p.submit_ms,p.present_ms};
            values[i]=times[phase];
        }
        static const char *names[]={"wall","retirement","prepare","acquire","encode","submit","present"};
        vk_profile_distribution(names[phase],values);
    }
    putchar('\n');
    if (!okay || snprintf(path,sizeof(path),"%s-linux-profile-%s.ppm",prefix,workload)>=(int)sizeof(path)) return 0;
    return capture(window,scene,renderer,path);
}
static int vk_profile_workload(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
    const char *prefix, const char *workload) {
    if (!cloud_vk_profile_enabled) return 1;
    cloud_vk_profile_active=1;
    int result=vk_profile_measure(window,scene,renderer,prefix,workload);
    cloud_vk_profile_active=0;
    return result;
}
static int vk_profile_images_equal(const char *left_path, const char *right_path) {
    FILE *left=fopen(left_path,"rb"), *right=fopen(right_path,"rb");
    int okay=left && right;
    if (okay) {
        unsigned char a[4096],b[4096];
        for (;;) {
            size_t na=fread(a,1,sizeof(a),left), nb=fread(b,1,sizeof(b),right);
            if (na!=nb || memcmp(a,b,na)) { okay=0; break; }
            if (!na) { okay=!ferror(left) && !ferror(right); break; }
        }
    }
    if (left && fclose(left)) okay=0;
    if (right && fclose(right)) okay=0;
    return okay;
}
static int vk_profile_compare(nx_window *window, cloud_scene *scene, nx_renderer_driver *renderer,
    const char *prefix, const char *workload) {
    if (!cloud_vk_profile_enabled) return 1;
    static const int batched[]={0,1,1,0};
    char baseline[1200]={0};
    for (unsigned block=0;block<4;++block) {
        char name[80];
        const char *mode=cloud_vk_profile_ring ? (batched[block] ? "ring" : "serial") : (batched[block] ? "batched" : "unbatched");
        if (snprintf(name,sizeof(name),"%s-%s-%u",workload,mode,block)>=(int)sizeof(name) ||
            nx_vulkan_test_set_frame_slots(renderer->backend.user,cloud_vk_profile_ring && batched[block] ? 3u : 1u) != NX_OK ||
            nx_vulkan_test_set_batching(renderer->backend.user,cloud_vk_profile_ring ? 1 : batched[block]) != NX_OK ||
            !vk_profile_workload(window,scene,renderer,prefix,name)) return 0;
        char capture_path[1200];
        if (snprintf(capture_path,sizeof(capture_path),"%s-linux-profile-%s.ppm",prefix,name)>=(int)sizeof(capture_path)) return 0;
        if (!block) memcpy(baseline,capture_path,strlen(capture_path)+1);
        else if (!vk_profile_images_equal(baseline,capture_path)) {
            fprintf(stderr,"Vulkan benchmark scene drift: %s differs from its first block\n",name);
            return 0;
        }
    }
    return nx_vulkan_test_set_batching(renderer->backend.user,1) == NX_OK &&
        nx_vulkan_test_set_frame_slots(renderer->backend.user,3) == NX_OK;
}
