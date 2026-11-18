#ifndef CONTENT_NATIVE_WORKER_H
#define CONTENT_NATIVE_WORKER_H
#include "surface.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef CONTENT_NATIVE_PNG
#include "png_frame.h"
#include <stdio.h>
#endif
#include "pdf_frame.h"
#include "content_thumbnails.h"
#include "png_gallery.h"
#include "pdf_detail.h"
#include "pdf_export.h"

/* Private deterministic producer. Only copied requests cross this boundary;
   neither the surface nor UI context belongs to this worker. */
typedef struct native_job {
    content_request request;
    uint64_t sequence;
    unsigned kind;
    int held;
    unsigned page; /* Zero uses the existing scenario's page. */
} native_job;
typedef struct native_png_source {
    content_request request;
    uint64_t sequence;
    nx_bitmap_view bitmap;
} native_png_source;
typedef struct native_pdf_index_result {
    uint64_t generation;
    content_pdf_index *index;
} native_pdf_index_result;
typedef struct native_pdf_properties_result {
    uint64_t generation;
    nx_result result;
    pdf_properties_packet packet;
} native_pdf_properties_result;
typedef struct native_export_job {
    uint64_t ticket, generation;
    unsigned page;
    pdf_export_view view;
} native_export_job;
typedef struct native_export_result {
    uint64_t ticket, generation;
    unsigned page;
    nx_result result;
} native_export_result;
typedef struct native_uri_result {
    uint64_t ticket;
    nx_result result;
} native_uri_result;
typedef struct native_worker {
    nx_snapshot_exchange *exchange;
    const char *encoded_dir, *pdf_renderer, *pdf_reader, *pdf_link_reader;
    const char *uri_launcher;
    const char *export_path;
    const png_gallery *gallery;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    int mutex_ready, condition_ready, started, wake[2];
    int stop, pending, working, prepared, allowed;
    native_job job;
    unsigned completed, publications, discarded;
    nx_result result;
    native_png_source png_source;
    int pdf_file;
    uint64_t pdf_generation, indexed_generation;
    native_pdf_index_result pdf_index;
    native_pdf_properties_result pdf_properties;
    content_thumbnail thumbnail, thumbnail_result;
    int thumbnail_working;
    pdf_detail_key detail;
    pdf_detail_result detail_result;
    int detail_working;
    native_export_job pdf_export;
    native_export_result export_result;
    int export_working;
    uint64_t uri_ticket;
    int uri_working;
    char uri[CONTENT_PDF_URI_LIMIT+1u];
    native_uri_result uri_result;
    _Atomic unsigned disposals;
} native_worker;
typedef struct native_worker_state {
    int working, pending, prepared;
    unsigned completed;
    nx_result result;
} native_worker_state;

static void native_dispose(void *user, void *data, size_t bytes) {
    (void)bytes;
    atomic_fetch_add((_Atomic unsigned *)user, 1u);
    free(data);
}
enum { NATIVE_FRAME_READY='W', NATIVE_PROPERTIES_READY='P', NATIVE_EXPORT_READY='E' };
static void native_wake(native_worker *worker, char byte) {
    ssize_t count;
    do { count = write(worker->wake[1], &byte, 1); } while (count < 0 && errno == EINTR);
    if (count != 1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        pthread_mutex_lock(&worker->mutex);
        worker->result = NX_ERROR_PLATFORM;
        pthread_mutex_unlock(&worker->mutex);
    }
}
static void native_damage(native_job job, content_frame_header *header) {
    if (job.kind != 3 || job.request.width < 4u || job.request.height < 4u) return;
    header->damage_base = 1;
    header->damage_x = job.request.width / 2u;
    header->damage_y = job.request.height / 2u;
    header->damage_width = job.request.width / 4u;
    header->damage_height = job.request.height / 4u;
}
static void *native_pixels(native_job job, size_t *bytes) {
    if (!job.request.width || !job.request.height || job.request.width > NX_VIEWPORT_MAX_DIMENSION ||
        job.request.height > NX_VIEWPORT_MAX_DIMENSION) return NULL;
    size_t stride = (size_t)job.request.width * 4u;
    if (stride > (4u * 1024u * 1024u) / job.request.height) return NULL;
    *bytes = sizeof(content_frame_header) + stride * job.request.height;
    uint8_t *data = malloc(*bytes);
    if (!data) return NULL;
    content_frame_header header = {.request = job.request, .sequence = job.sequence,
        .format = job.kind == 2 ? NX_BITMAP_A8 : NX_BITMAP_RGBA8, .stride = (uint32_t)stride};
    native_damage(job, &header);
    memcpy(data, &header, sizeof(header));
    static const uint8_t colors[5][3] = {{211,79,31},{23,83,197},{127,127,127},{71,163,31},{191,63,137}};
    if (job.kind > 4) { free(data); return NULL; }
    for (size_t i = sizeof(header); i < *bytes; i += 4u) {
        unsigned kind = job.kind;
        if (header.damage_base) {
            size_t x = ((i - sizeof(header)) % stride) / 4u;
            size_t y = (i - sizeof(header)) / stride;
            if (x < header.damage_x || x >= header.damage_x + header.damage_width ||
                y < header.damage_y || y >= header.damage_y + header.damage_height)
                kind = 1;
        }
        memcpy(data + i, colors[kind], 3);
        data[i + 3u] = 255;
    }
    return data;
}

#ifdef CONTENT_NATIVE_PNG
static nx_result native_png(const char *directory, native_job job, void **data, size_t *bytes,
    native_png_source *original) {
    char path[4096];
    int written = snprintf(path, sizeof(path), "%s/%u.png", directory, job.kind);
    if (written < 0 || (size_t)written >= sizeof(path)) return NX_ERROR_LIMIT;
    FILE *file = fopen(path, "rb");
    if (!file) return NX_ERROR_PLATFORM;
    uint8_t *encoded = NULL;
    nx_result result = NX_ERROR_PLATFORM;
    if (!fseek(file, 0, SEEK_END)) {
        long length = ftell(file);
        if (length > 0 && (unsigned long)length > CONTENT_PNG_ENCODED_LIMIT) result = NX_ERROR_LIMIT;
        else if (length > 0 && !fseek(file, 0, SEEK_SET)) {
            encoded = malloc((size_t)length);
            if (!encoded) result = NX_ERROR_MEMORY;
            else if (fread(encoded, 1, (size_t)length, file) == (size_t)length)
                result = content_png_bitmap(encoded,(size_t)length,&original->bitmap);
        }
    }
    free(encoded);
    fclose(file);
    if (result==NX_OK) result=content_png_fit_bitmap(original->bitmap,job.request,job.sequence,data,bytes,NULL);
    if (result == NX_OK) {
        original->request=job.request;
        original->sequence=job.sequence;
        content_frame_header header;
        memcpy(&header, *data, sizeof(header));
        /* The recovery PNG changes only the declared quarter rectangle at
           exact multiples of the fixture size. Other sizes send full frames. */
        if (original->bitmap.width==112 && original->bitmap.height==52 &&
            job.request.width % 112u == 0 && job.request.height % 52u == 0 &&
            job.request.width / 112u == job.request.height / 52u)
            native_damage(job, &header);
        memcpy(*data, &header, sizeof(header));
    }
    return result;
}
#endif
#ifdef CONTENT_NATIVE_PDF
static nx_result native_pdf(native_worker *worker, native_job job, void **data, size_t *bytes) {
    const char *name = job.kind == 0 ? "old.pdf" : job.kind == 2 ? "bad.pdf" : "document.pdf";
    char path[4096];
    int written = snprintf(path, sizeof(path), "%s/%s", worker->encoded_dir, name);
    if (written < 0 || (size_t)written >= sizeof(path)) return NX_ERROR_LIMIT;
    unsigned page = job.page ? job.page : job.kind == 3 || job.kind == 4 ? 2u : 1u;
    if (job.kind == 2)
        return content_pdf_frame_with_text(worker->pdf_renderer,worker->pdf_reader,path,page,
            job.request,job.sequence,data,bytes);
    if (worker->pdf_generation != job.request.generation) {
        int file;
        nx_result result = content_pdf_open(path,&file);
        if (result != NX_OK) return result;
        if (worker->pdf_file >= 0) close(worker->pdf_file);
        worker->pdf_file = file;
        worker->pdf_generation = job.request.generation;
    }
    return content_pdf_render_with_links(worker->pdf_renderer,worker->pdf_reader,worker->pdf_link_reader,
        worker->pdf_file,page,job.request,job.sequence,data,bytes);
}

static void native_index_pdf(native_worker *worker, native_job job) {
    uint64_t generation = job.request.generation;
    if (!worker->pdf_reader || job.kind == 2 || generation == worker->indexed_generation) return;
    pthread_mutex_lock(&worker->mutex);
    int skip = worker->stop;
    pthread_mutex_unlock(&worker->mutex);
    if (skip) return;
    /* After frame publication so indexing never delays that first frame.
       Further per-page work uses this same worker and pinned descriptor. */
    content_pdf_index *index = NULL;
    (void)content_pdf_index_read(worker->pdf_reader,worker->pdf_file,&index);
    worker->indexed_generation = generation;
    pthread_mutex_lock(&worker->mutex);
    content_pdf_index *retired = worker->pdf_index.index;
    worker->pdf_index = (native_pdf_index_result){generation,index};
    pthread_mutex_unlock(&worker->mutex);
    content_pdf_index_destroy(retired);
    native_wake(worker,NATIVE_FRAME_READY);
    if (!worker->pdf_link_reader) return;
    pthread_mutex_lock(&worker->mutex);
    skip=worker->stop;
    pthread_mutex_unlock(&worker->mutex);
    if (skip) return;
    native_pdf_properties_result properties={.generation=generation};
    properties.result=content_pdf_read_properties(worker->pdf_link_reader,worker->pdf_file,&properties.packet);
    pthread_mutex_lock(&worker->mutex);
    worker->pdf_properties=properties;
    pthread_mutex_unlock(&worker->mutex);
    native_wake(worker,NATIVE_PROPERTIES_READY);
}
#endif

#ifdef CONTENT_NATIVE_PDF
static void native_render_export(native_worker *worker, native_export_job job) {
    nx_result result=NX_ERROR_STATE;
    if (worker->pdf_generation==job.generation) {
        nx_bitmap_view image={0};
        result=content_pdf_export_render(worker->pdf_renderer,worker->pdf_link_reader!=NULL,
            worker->pdf_file,job.page,job.view,&image);
        if (result==NX_OK) result=content_pdf_export_png(worker->export_path,image);
        free((void *)image.pixels);
    }
    pthread_mutex_lock(&worker->mutex);
    worker->export_result=(native_export_result){job.ticket,job.generation,job.page,result};
    worker->export_working=0;
    pthread_mutex_unlock(&worker->mutex);
    native_wake(worker,NATIVE_EXPORT_READY);
}

static void native_render_detail(native_worker *worker, pdf_detail_key key) {
    pdf_detail_result detail={.key=key,.result=NX_ERROR_STATE};
    if (worker->pdf_generation==key.generation)
        detail.result=content_pdf_render_detail(worker->pdf_renderer,worker->pdf_link_reader!=NULL,
            worker->pdf_file,key.page,key.edge,&detail.bitmap);
    pthread_mutex_lock(&worker->mutex);
    worker->detail_result=detail;
    worker->detail_working=0;
    pthread_mutex_unlock(&worker->mutex);
    native_wake(worker,NATIVE_FRAME_READY);
}

#endif

#ifdef CONTENT_NATIVE_PNG
static void native_render_thumbnail(native_worker *worker, content_thumbnail thumbnail) {
    if (worker->gallery)
        (void)png_gallery_frame(worker->gallery,thumbnail.page,thumbnail.request,1,&thumbnail.frame,&thumbnail.bytes,NULL);
#ifdef CONTENT_NATIVE_PDF
    else
    if (worker->pdf_generation==thumbnail.request.generation)
        (void)content_pdf_render_thumbnail(worker->pdf_renderer,worker->pdf_link_reader!=NULL,
            worker->pdf_file,thumbnail.page,thumbnail.request,&thumbnail.frame,&thumbnail.bytes);
#endif
    pthread_mutex_lock(&worker->mutex);
    worker->thumbnail_result=thumbnail;
    worker->thumbnail_working=0;
    pthread_mutex_unlock(&worker->mutex);
    native_wake(worker,NATIVE_FRAME_READY);
}
#endif

static void *native_worker_entry(void *user) {
    native_worker *worker = user;
    for (;;) {
        pthread_mutex_lock(&worker->mutex);
        while (!worker->stop && !worker->pending && !worker->uri_ticket && !worker->thumbnail.page &&
            !worker->detail.edge && !worker->pdf_export.ticket)
            pthread_cond_wait(&worker->condition, &worker->mutex);
        if (worker->stop) { pthread_mutex_unlock(&worker->mutex); break; }
        if (!worker->pending && worker->uri_ticket) {
            char address[CONTENT_PDF_URI_LIMIT+1u];
            memcpy(address,worker->uri,sizeof(address));
            uint64_t ticket=worker->uri_ticket;
            worker->uri_ticket=0;
            worker->uri_working=1;
            pthread_mutex_unlock(&worker->mutex);
            nx_result opened=NX_ERROR_UNSUPPORTED;
#ifdef CONTENT_NATIVE_PDF
            opened=content_pdf_open_uri(worker->uri_launcher,address);
#endif
            pthread_mutex_lock(&worker->mutex);
            worker->uri_result=(native_uri_result){ticket,opened};
            worker->uri_working=0;
            pthread_mutex_unlock(&worker->mutex);
            native_wake(worker,NATIVE_FRAME_READY);
            continue;
        }
#ifdef CONTENT_NATIVE_PDF
        if (!worker->pending && worker->pdf_export.ticket) {
            native_export_job job=worker->pdf_export;
            worker->pdf_export=(native_export_job){0};
            worker->export_working=1;
            pthread_mutex_unlock(&worker->mutex);
            native_render_export(worker,job);
            continue;
        }
        if (!worker->pending && worker->detail.edge) {
            pdf_detail_key key=worker->detail;
            worker->detail=(pdf_detail_key){0};
            worker->detail_working=1;
            pthread_mutex_unlock(&worker->mutex);
            native_render_detail(worker,key);
            continue;
        }
#endif
#ifdef CONTENT_NATIVE_PNG
        if (!worker->pending && worker->thumbnail.page) {
            content_thumbnail thumbnail=worker->thumbnail;
            worker->thumbnail=(content_thumbnail){0};
            worker->thumbnail_working=1;
            pthread_mutex_unlock(&worker->mutex);
            native_render_thumbnail(worker,thumbnail);
            continue;
        }
#endif
        native_job job = worker->job;
        worker->pending = 0; worker->working = 1;
        pthread_mutex_unlock(&worker->mutex);
        size_t bytes = 0;
        void *data = NULL;
        native_png_source original={0};
        nx_result result;
#ifdef CONTENT_NATIVE_PDF
        if (worker->pdf_renderer) result = native_pdf(worker, job, &data, &bytes);
        else
#endif
#ifdef CONTENT_NATIVE_PNG
        if (worker->gallery) {
            result=png_gallery_frame(worker->gallery,job.page,job.request,job.sequence,&data,&bytes,&original.bitmap);
            if (result==NX_OK) { original.request=job.request;original.sequence=job.sequence; }
        } else if (worker->encoded_dir) result = native_png(worker->encoded_dir, job, &data, &bytes,&original);
        else
#endif
        {
            data = native_pixels(job, &bytes);
            result = data ? NX_OK : NX_ERROR_MEMORY;
        }
        pthread_mutex_lock(&worker->mutex);
        worker->prepared = 1;
        pthread_mutex_unlock(&worker->mutex);
        native_wake(worker,NATIVE_FRAME_READY);
        pthread_mutex_lock(&worker->mutex);
        while (!worker->stop && job.held && !worker->allowed)
            pthread_cond_wait(&worker->condition, &worker->mutex);
        int stop = worker->stop;
        if (stop && data) ++worker->discarded;
        pthread_mutex_unlock(&worker->mutex);
        if (stop) { free(data);free((void *)original.bitmap.pixels);break; }
        if (result == NX_OK) result = nx_snapshot_exchange_publish(worker->exchange, data, bytes,
            native_dispose, &worker->disposals);
        if (result != NX_OK) free(data);
        pthread_mutex_lock(&worker->mutex);
        worker->prepared = 0; worker->working = 0; ++worker->completed; worker->result = result;
        if (result == NX_OK) ++worker->publications;
        if (result==NX_OK && original.bitmap.pixels) {
            free((void *)worker->png_source.bitmap.pixels);
            worker->png_source=original;
            original=(native_png_source){0};
        }
        pthread_mutex_unlock(&worker->mutex);
        free((void *)original.bitmap.pixels);
        native_wake(worker,NATIVE_FRAME_READY);
#ifdef CONTENT_NATIVE_PDF
        if (result == NX_OK) native_index_pdf(worker,job);
#endif
    }
    return NULL;
}
static int native_worker_close(native_worker *worker) {
    int result = 0;
    if (worker->started) {
        pthread_mutex_lock(&worker->mutex);
        worker->stop = 1;
        pthread_cond_broadcast(&worker->condition);
        pthread_mutex_unlock(&worker->mutex);
        /* No UI/engine call remains in this worker; stop releases its gate. */
        if (pthread_join(worker->thread, NULL)) _Exit(1);
        worker->started = 0;
    }
    free((void *)worker->png_source.bitmap.pixels);
    free(worker->thumbnail_result.frame);
#ifdef CONTENT_NATIVE_PDF
    content_pdf_index_destroy(worker->pdf_index.index);
    free((void *)worker->detail_result.bitmap.pixels);
    if (worker->pdf_renderer && worker->pdf_file >= 0 && close(worker->pdf_file)) result = -1;
#endif
    for (unsigned i = 0; i < 2; ++i) if (worker->wake[i] >= 0) {
        if (close(worker->wake[i])) result = -1;
        worker->wake[i] = -1;
    }
    if (worker->condition_ready) { if (pthread_cond_destroy(&worker->condition)) result = -1; worker->condition_ready = 0; }
    if (worker->mutex_ready) { if (pthread_mutex_destroy(&worker->mutex)) result = -1; worker->mutex_ready = 0; }
    return result;
}
static int native_worker_open(native_worker *worker, nx_snapshot_exchange *exchange,
    const char *encoded_dir, const char *pdf_renderer, const char *pdf_reader, const char *pdf_link_reader,
    const char *uri_launcher, const char *export_path,const png_gallery *gallery) {
    memset(worker, 0, sizeof(*worker)); worker->wake[0] = worker->wake[1] = -1;
    worker->pdf_file = -1;
    atomic_init(&worker->disposals, 0); worker->exchange = exchange;
    worker->encoded_dir = encoded_dir; worker->pdf_renderer = pdf_renderer; worker->pdf_reader = pdf_reader;
    worker->pdf_link_reader = pdf_link_reader;
    worker->uri_launcher = uri_launcher;
    worker->export_path = export_path;
    worker->gallery = gallery;
    if (pthread_mutex_init(&worker->mutex, NULL)) return -1;
    worker->mutex_ready = 1;
    if (pthread_cond_init(&worker->condition, NULL)) return -1;
    worker->condition_ready = 1;
    if (pipe2(worker->wake, O_CLOEXEC | O_NONBLOCK) ||
        pthread_create(&worker->thread, NULL, native_worker_entry, worker)) return -1;
    worker->started = 1;
    return 0;
}
static native_worker_state native_state(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_worker_state state = {worker->working, worker->pending, worker->prepared, worker->completed, worker->result};
    pthread_mutex_unlock(&worker->mutex);
    return state;
}

#ifdef CONTENT_NATIVE_PNG
static void native_png_take(native_worker *worker,native_png_source *cache,content_surface_status base) {
    pthread_mutex_lock(&worker->mutex);
    native_png_source source=worker->png_source;
    worker->png_source=(native_png_source){0};
    pthread_mutex_unlock(&worker->mutex);
    if (source.bitmap.pixels) {
        free((void *)cache->bitmap.pixels);
        *cache=source;
    }
    if (cache->request.generation!=base.request.generation || cache->request.resize!=base.request.resize ||
        cache->sequence<base.sequence) {
        free((void *)cache->bitmap.pixels);
        *cache=(native_png_source){0};
    }
}
#endif

#ifdef CONTENT_NATIVE_PDF
static native_pdf_index_result native_take_pdf_index(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_pdf_index_result result = worker->pdf_index;
    worker->pdf_index = (native_pdf_index_result){0};
    pthread_mutex_unlock(&worker->mutex);
    return result;
}

static native_pdf_properties_result native_take_pdf_properties(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_pdf_properties_result result=worker->pdf_properties;
    worker->pdf_properties.generation=0;
    pthread_mutex_unlock(&worker->mutex);
    return result;
}

#endif
#ifdef CONTENT_NATIVE_PNG
static int native_schedule_thumbnail(native_worker *worker, content_request request, unsigned page) {
    pthread_mutex_lock(&worker->mutex);
    int accepted=!worker->stop && !worker->pending && !worker->working && !worker->uri_ticket &&
        !worker->thumbnail.page && !worker->thumbnail_result.page && !worker->thumbnail_working;
    if (accepted) {
        worker->thumbnail=(content_thumbnail){.request=request,.page=page};
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
    return accepted;
}

static content_thumbnail native_take_thumbnail(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    content_thumbnail result=worker->thumbnail_result;
    worker->thumbnail_result=(content_thumbnail){0};
    pthread_mutex_unlock(&worker->mutex);
    return result;
}
#endif

#ifdef CONTENT_NATIVE_PDF
static void native_schedule_detail(native_worker *worker, pdf_detail_key key) {
    pthread_mutex_lock(&worker->mutex);
    if (!worker->stop && !worker->pending && !worker->working && !worker->uri_ticket &&
        !worker->detail.edge && !worker->detail_result.key.edge && !worker->detail_working) {
        worker->detail=key;
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
}

static pdf_detail_result native_take_detail(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    pdf_detail_result result=worker->detail_result;
    worker->detail_result=(pdf_detail_result){0};
    pthread_mutex_unlock(&worker->mutex);
    return result;
}

static int native_schedule_export(native_worker *worker, native_export_job job) {
    if (!worker->export_path || !job.ticket || !job.generation || !job.page) return 0;
    pthread_mutex_lock(&worker->mutex);
    int accepted=!worker->stop && !worker->pdf_export.ticket && !worker->export_result.ticket && !worker->export_working;
    if (accepted) {
        worker->pdf_export=job;
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
    return accepted;
}

static native_export_result native_take_export(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_export_result result=worker->export_result;
    worker->export_result=(native_export_result){0};
    pthread_mutex_unlock(&worker->mutex);
    return result;
}
#endif
static int native_schedule(native_worker *worker, native_job job) {
    pthread_mutex_lock(&worker->mutex);
    int accepted = !worker->stop && !worker->pending && !worker->working;
    if (accepted) {
        worker->job = job; worker->pending = 1; worker->prepared = 0; worker->allowed = !job.held;
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
    return accepted;
}
static int native_schedule_uri(native_worker *worker, uint64_t ticket,
    const char uri[CONTENT_PDF_URI_LIMIT+1u]) {
    if (!worker->uri_launcher || !ticket || !content_pdf_web_uri(uri)) return 0;
    pthread_mutex_lock(&worker->mutex);
    int accepted=!worker->stop && !worker->uri_ticket && !worker->uri_working;
    if (accepted) {
        memcpy(worker->uri,uri,sizeof(worker->uri));
        worker->uri_ticket=ticket;
        pthread_cond_signal(&worker->condition);
    }
    pthread_mutex_unlock(&worker->mutex);
    return accepted;
}

static native_uri_result native_take_uri_result(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex);
    native_uri_result result=worker->uri_result;
    worker->uri_result=(native_uri_result){0};
    pthread_mutex_unlock(&worker->mutex);
    return result;
}
static void native_release(native_worker *worker) {
    pthread_mutex_lock(&worker->mutex); worker->allowed = 1;
    pthread_cond_signal(&worker->condition); pthread_mutex_unlock(&worker->mutex);
}
#endif
