#include "pdf_camera.h"

void pdf_camera_fit(pdf_camera *camera) {
    *camera=(pdf_camera){.percent=100,.center_x=.5f,.center_y=.5f};
}

pdf_camera_view pdf_camera_place(const pdf_camera *camera, nx_rect viewport) {
    float zoom=(float)camera->percent/100;
    nx_rect image={viewport.x+viewport.width*(.5f-camera->center_x*zoom),
        viewport.y+viewport.height*(.5f-camera->center_y*zoom),viewport.width*zoom,viewport.height*zoom};
    return (pdf_camera_view){viewport,image};
}

int pdf_camera_visible(pdf_camera_view view, nx_rect normalized, nx_rect *out) {
    nx_rect image=view.image, clip=view.viewport;
    float x=image.x+normalized.x*image.width;
    float y=image.y+normalized.y*image.height;
    float right=x+normalized.width*image.width;
    float bottom=y+normalized.height*image.height;
    if (x<clip.x) x=clip.x;
    if (y<clip.y) y=clip.y;
    if (right>clip.x+clip.width) right=clip.x+clip.width;
    if (bottom>clip.y+clip.height) bottom=clip.y+clip.height;
    if (!(right>x && bottom>y)) return 0;
    *out=(nx_rect){x,y,right-x,bottom-y};
    return 1;
}

static int contains(nx_rect view, float x, float y) {
    return x>=view.x && y>=view.y && x<view.x+view.width && y<view.y+view.height;
}

static float camera_axis(float value, unsigned percent) {
    float half=50.0f/(float)percent;
    if (value<half) return half;
    if (value>1-half) return 1-half;
    return value;
}

static void pan(pdf_camera *camera, nx_rect view, float x, float y) {
    float zoom=(float)camera->percent/100;
    camera->center_x=camera_axis(camera->center_x+x/(view.width*zoom),camera->percent);
    camera->center_y=camera_axis(camera->center_y+y/(view.height*zoom),camera->percent);
}

static void zoom_at(pdf_camera *camera, int change, float x, float y) {
    int percent=(int)camera->percent+change;
    if (percent<100) percent=100;
    if (percent>(int)PDF_ZOOM_MAX) percent=(int)PDF_ZOOM_MAX;
    float anchor_scale=100.0f/(float)camera->percent-100.0f/(float)percent;
    camera->percent=(unsigned)percent;
    camera->center_x=camera_axis(camera->center_x+x*anchor_scale,camera->percent);
    camera->center_y=camera_axis(camera->center_y+y*anchor_scale,camera->percent);
}

nx_result pdf_camera_update(pdf_camera *camera, const nx_context *context, nx_rect view,
    uint64_t viewport_id, unsigned action, int *redraw) {
    if (!(view.width>0 && view.height>0)) { camera->dragging=0;return NX_OK; }
    pdf_camera before=*camera;
    if (action==PDF_ZOOM_IN) zoom_at(camera,50,0,0);
    if (action==PDF_ZOOM_OUT) zoom_at(camera,-50,0,0);
    if (action==PDF_ZOOM_FIT) pdf_camera_fit(camera);
    const nx_event *events;
    size_t count;
    nx_result result=nx_frame_events(context,&events,&count);
    if (result!=NX_OK) return result;
    int focused=nx_focus_id(context)==viewport_id;
    for (size_t i=0;i<count;++i) {
        const nx_event *event=&events[i];
        switch (event->type) {
        case NX_EVENT_POINTER_DOWN:
            if (event->button==NX_BUTTON_MIDDLE && contains(view,event->x,event->y)) {
                camera->dragging=1;
                camera->pointer=(nx_point){event->x,event->y};
            }
            break;
        case NX_EVENT_POINTER_MOVE:
        case NX_EVENT_POINTER_UP:
            if (camera->dragging && (event->type==NX_EVENT_POINTER_MOVE || event->button==NX_BUTTON_MIDDLE)) {
                pan(camera,view,camera->pointer.x-event->x,camera->pointer.y-event->y);
                camera->pointer=(nx_point){event->x,event->y};
                if (event->type==NX_EVENT_POINTER_UP) camera->dragging=0;
            }
            break;
        case NX_EVENT_POINTER_CANCEL:
            camera->dragging=0;
            break;
        case NX_EVENT_FOCUS:
            if (!event->focused) camera->dragging=0;
            break;
        case NX_EVENT_WHEEL:
            if (contains(view,event->x,event->y)) {
                if (event->modifiers==NX_MOD_CONTROL && event->delta_y!=0)
                    zoom_at(camera,event->delta_y>0?50:-50,
                        (event->x-view.x)/view.width-.5f,(event->y-view.y)/view.height-.5f);
                else if (!event->modifiers)
                    pan(camera,view,-event->delta_x*32,-event->delta_y*32);
            }
            break;
        case NX_EVENT_KEY_DOWN:
            if (focused && event->modifiers==NX_MOD_CONTROL && event->key==NX_KEY_0) pdf_camera_fit(camera);
            if (!focused || event->modifiers) break;
            if (event->key==NX_KEY_LEFT) pan(camera,view,-48,0);
            if (event->key==NX_KEY_RIGHT) pan(camera,view,48,0);
            if (event->key==NX_KEY_UP) pan(camera,view,0,-48);
            if (event->key==NX_KEY_DOWN) pan(camera,view,0,48);
            break;
        default:
            break;
        }
    }
    if (camera->percent!=before.percent || camera->center_x!=before.center_x || camera->center_y!=before.center_y)
        *redraw=1;
    return NX_OK;
}
