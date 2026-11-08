/* Ordinary caption/edge gestures for the original Editor's existing geometry.
   The SDK alone owns native press/serial validation. No native handle escapes. */
static bool editor_window_event(struct app *a,const nx_event *e,bool *handled) {
    *handled=false;
    if(e->type==NX_EVENT_POINTER_CANCEL || (e->type==NX_EVENT_FOCUS && !e->focused)) {
        a->chrome_release=false;return true;
    }
    if(e->type==NX_EVENT_POINTER_UP && e->button==1 && a->chrome_release) {
        a->chrome_release=false;*handled=true;return true;
    }
    if(e->type!=NX_EVENT_POINTER_DOWN || e->button!=1 || a->mode!=EDITING || a->review.id) return true;
    nx_window_status status;
    if(nx_window_get_status(a->window,&status)!=NX_OK) return false;
    if(status.state&(NX_WINDOW_STATE_MAXIMIZED|NX_WINDOW_STATE_FULLSCREEN)) return true;
    if(!isfinite(e->x)||!isfinite(e->y)||e->x<0||e->y<0||e->x>=a->width||e->y>=a->height) return true;
    nx_window_resize_edge edge=0;
    bool left=e->x<6,right=e->x>=a->width-6,top=e->y<6,bottom=e->y>=a->height-6;
    if(top) edge=left?NX_WINDOW_RESIZE_TOP_LEFT:right?NX_WINDOW_RESIZE_TOP_RIGHT:NX_WINDOW_RESIZE_TOP;
    else if(bottom) edge=left?NX_WINDOW_RESIZE_BOTTOM_LEFT:right?NX_WINDOW_RESIZE_BOTTOM_RIGHT:NX_WINDOW_RESIZE_BOTTOM;
    else if(left) edge=NX_WINDOW_RESIZE_LEFT;
    else if(right) edge=NX_WINDOW_RESIZE_RIGHT;
    nx_result result;
    if(edge && (status.capabilities&NX_WINDOW_CAP_RESIZE)) result=nx_window_begin_resize(a->window,edge);
    else if(e->x>=112 && e->y<fmaxf(36,13*a->scale*1.4f+12) && (status.capabilities&NX_WINDOW_CAP_MOVE)) result=nx_window_begin_move(a->window);
    else return true;
    *handled=true;a->chrome_release=true;
    if(a->diagnostics) {printf("EDITOR_GESTURE edge=%d result=%d\n",(int)edge,(int)result);fflush(stdout);}
    return result==NX_OK || result==NX_ERROR_STATE || result==NX_ERROR_UNSUPPORTED;
}
