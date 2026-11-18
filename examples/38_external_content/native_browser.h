#ifndef CONTENT_NATIVE_BROWSER_H
#define CONTENT_NATIVE_BROWSER_H

#include <math.h>

#define BROWSER_EDITOR_ID UINT64_C(100)
#define BROWSER_EDIT_QUEUE 8u
enum browser_input_phase { BROWSER_HOST, BROWSER_ENTERING, BROWSER_EDITING, BROWSER_LEAVING };
typedef struct browser_editor {
    enum browser_input_phase phase;
    content_request request;
    nx_rect bounds;
    uint64_t revision, sequence;
    unsigned count, awaiting, operation;
    int enabled, receiving;
    int pointer_enabled, pointer_inside;
    uint32_t pressed, draining, host_buttons;
    content_browser_edit pending[BROWSER_EDIT_QUEUE], commit;
} browser_editor;

static int same_browser_surface(content_request a,content_request b) {
    return a.generation==b.generation && a.resize==b.resize && a.width==b.width && a.height==b.height;
}

static int browser_edit_queue(browser_editor *editor,content_browser_edit edit) {
    if (edit.kind==CONTENT_BROWSER_MOUSE_MOVE && editor->count &&
        editor->pending[editor->count-1u].kind==CONTENT_BROWSER_MOUSE_MOVE) {
        editor->pending[editor->count-1u]=edit;
        return 1;
    }
    if (editor->count==BROWSER_EDIT_QUEUE) return 0;
    editor->pending[editor->count++]=edit;
    return 1;
}

static int browser_ime_disable(browser_editor *editor,nx_window *window) {
    if (!editor->enabled) return 1;
    editor->enabled=0;
    nx_result withdrawn=nx_window_set_ime_surrounding_text(window,0,0,NULL,0,0,0);
    nx_result disabled=nx_window_set_ime_enabled(window,0);
    return withdrawn==NX_OK && disabled==NX_OK;
}

static int browser_edit_leave(browser_editor *editor,nx_window *window) {
    if (editor->phase==BROWSER_HOST || editor->phase==BROWSER_LEAVING) return 1;
    int remote_focus=editor->phase!=BROWSER_ENTERING || editor->awaiting;
    editor->count=0;
    editor->receiving=0;
    editor->commit=(content_browser_edit){0};
    editor->draining|=editor->pressed;
    editor->pressed=0;
    editor->pointer_inside=0;
    editor->phase=remote_focus?BROWSER_LEAVING:BROWSER_HOST;
    return browser_ime_disable(editor,window) &&
           (!remote_focus || browser_edit_queue(editor,(content_browser_edit){.kind=CONTENT_BROWSER_BLUR}));
}

static int browser_pointer_event(browser_editor *editor,nx_window *window,const nx_event *event,int *route) {
    uint32_t button=event->button;
    int edge=event->type==NX_EVENT_POINTER_DOWN || event->type==NX_EVENT_POINTER_UP;
    if (edge && (editor->draining&button)) {
        if (event->type==NX_EVENT_POINTER_UP) editor->draining&=~button;
        *route=0;
        return 1;
    }
    if (editor->phase!=BROWSER_EDITING || editor->host_buttons) {
        if (event->type==NX_EVENT_POINTER_DOWN) editor->host_buttons|=button;
        if (event->type==NX_EVENT_POINTER_UP) editor->host_buttons&=~button;
        return 1;
    }
    nx_point local;
    if (!content_input_pointer_local(editor->bounds,event->x,event->y,&local)) {
        if (event->type==NX_EVENT_POINTER_DOWN) editor->host_buttons|=button;
        if (!editor->pointer_inside) return 1;
        editor->pointer_inside=0;
        editor->draining|=editor->pressed;
        if (event->type==NX_EVENT_POINTER_UP && (editor->pressed&button)) {
            editor->draining&=~button;
            *route=0;
        }
        editor->pressed=0;
        return browser_edit_queue(editor,(content_browser_edit){.kind=CONTENT_BROWSER_MOUSE_CANCEL}) ||
            browser_edit_leave(editor,window);
    }
    *route=0;
    editor->pointer_inside=1;
    unsigned kind=CONTENT_BROWSER_MOUSE_MOVE;
    if (edge) {
        if (!button || button>NX_BUTTON_X2 || (button&(button-1u))) return browser_edit_leave(editor,window);
        int down=event->type==NX_EVENT_POINTER_DOWN;
        if (!!(editor->pressed&button)==down) return 1;
        kind=down?CONTENT_BROWSER_MOUSE_DOWN:CONTENT_BROWSER_MOUSE_UP;
    } else if (event->type==NX_EVENT_WHEEL) {
        if (!isfinite(event->delta_x) || !isfinite(event->delta_y)) return browser_edit_leave(editor,window);
        kind=CONTENT_BROWSER_MOUSE_WHEEL;
    }
    content_browser_edit edit={.kind=kind,.pointer={
        .x=(int32_t)(local.x*256.0f),.y=(int32_t)(local.y*256.0f),
        .button=edge?button:0,.modifiers=event->modifiers&(NX_MOD_SHIFT|NX_MOD_CONTROL|NX_MOD_ALT|NX_MOD_SUPER)}};
    if (kind==CONTENT_BROWSER_MOUSE_WHEEL) {
        /* NexaUI's abstract units increase toward the start of a scroll area.
           The fixture chooses 40 CSS px per unit, positive toward the end. */
        edit.pointer.delta_x=(int32_t)(-10240.0f*fmaxf(-100.0f,fminf(100.0f,event->delta_x)));
        edit.pointer.delta_y=(int32_t)(-10240.0f*fmaxf(-100.0f,fminf(100.0f,event->delta_y)));
    }
    if (!browser_edit_queue(editor,edit)) return browser_edit_leave(editor,window);
    if (kind==CONTENT_BROWSER_MOUSE_DOWN) editor->pressed|=button;
    if (kind==CONTENT_BROWSER_MOUSE_UP) editor->pressed&=~button;
    return 1;
}

static int browser_append(content_browser_edit *text,uint32_t scalar) {
    if (scalar<32 || scalar==127 || scalar>0x10ffff || (scalar>=0xd800 && scalar<=0xdfff)) return 0;
    unsigned count=scalar<0x80?1u:scalar<0x800?2u:scalar<0x10000?3u:4u;
    if (count>CONTENT_BROWSER_TEXT_LIMIT-text->length) return 0;
    unsigned char *bytes=(unsigned char *)text->text+text->length;
    for (unsigned i=count-1;i>0;--i) {
        bytes[i]=(unsigned char)(0x80u|(scalar&63u));
        scalar>>=6;
    }
    static const unsigned char lead[]={0,0,0xc0u,0xe0u,0xf0u};
    bytes[0]=(unsigned char)(lead[count]|scalar);
    text->length+=count;
    return 1;
}

/* Native transactions carry the editor identity and revision used to
   advertise context; physical keyboard text has neither. */
static int browser_edit_event(browser_editor *editor,nx_window *window,const nx_event *event) {
    if (editor->phase!=BROWSER_EDITING) return 1;
    int current=event->ime_target==BROWSER_EDITOR_ID && event->ime_revision==editor->revision;
    if (event->type==NX_EVENT_IME_TRANSACTION_BEGIN) {
        editor->commit=(content_browser_edit){.kind=CONTENT_BROWSER_COMMIT};
        editor->receiving=current && event->ime_before==NX_IME_TRANSACTION_COMMIT;
    } else if (event->type==NX_EVENT_IME_TRANSACTION_END) {
        int ready=editor->receiving && current && event->ime_before==NX_IME_TRANSACTION_COMMIT && editor->commit.length;
        editor->receiving=0;
        if (ready) return browser_edit_queue(editor,editor->commit);
    } else if (event->type==NX_EVENT_TEXT) {
        if (current) {
            if (editor->receiving && ((event->ime_before&NX_IME_COMMIT_FILTERED) ||
                !browser_append(&editor->commit,event->scalar))) editor->receiving=0;
        } else if (!event->ime_target && !event->ime_before) {
            content_browser_edit text={.kind=CONTENT_BROWSER_COMMIT};
            return browser_append(&text,event->scalar) && browser_edit_queue(editor,text);
        }
    } else if (event->type==NX_EVENT_IME_PREEDIT_CHANGED && current) {
        content_browser_edit draft={.kind=CONTENT_BROWSER_PREEDIT};
        nx_result result=nx_window_get_ime_preedit_for(window,BROWSER_EDITOR_ID,editor->revision,
            draft.text,sizeof(draft.text),&draft.length,&draft.begin,&draft.end);
        if (result==NX_ERROR_STATE || (!draft.length && editor->receiving)) return 1;
        return result==NX_OK && browser_edit_queue(editor,draft);
    }
    return 1;
}

static int browser_edit_filter(browser_editor *editor,nx_window *window,const nx_event *event,int *route) {
    *route=1;
    if (editor->pointer_enabled && (event->type==NX_EVENT_POINTER_MOVE || event->type==NX_EVENT_POINTER_DOWN ||
        event->type==NX_EVENT_POINTER_UP || event->type==NX_EVENT_WHEEL))
        return browser_pointer_event(editor,window,event,route);
    if ((editor->pointer_enabled && event->type==NX_EVENT_POINTER_CANCEL) ||
        (event->type==NX_EVENT_FOCUS&&!event->focused)) {
        int leaving=browser_edit_leave(editor,window);
        editor->draining=editor->host_buttons=0;
        return leaving;
    }
    if (editor->phase==BROWSER_HOST) return 1;
    switch (event->type) {
        case NX_EVENT_KEY_DOWN:
        case NX_EVENT_KEY_DOWN_CONSUMED:
            if (event->key==NX_KEY_F12 && (event->modifiers&(NX_MOD_CONTROL|NX_MOD_SHIFT))==
                (NX_MOD_CONTROL|NX_MOD_SHIFT)) {
                *route=0;
                puts("BROWSER_IME host escape");
                fflush(stdout);
                return browser_edit_leave(editor,window);
            }
            /* fall through */
        case NX_EVENT_KEY_UP:
        case NX_EVENT_TEXT:
        case NX_EVENT_TEXT_COMMAND:
        case NX_EVENT_IME_TRANSACTION_BEGIN:
        case NX_EVENT_IME_TRANSACTION_END:
        case NX_EVENT_IME_PREEDIT_CHANGED:
        case NX_EVENT_IME_DELETE_SURROUNDING:
        case NX_EVENT_IME_DELETE_RANGE:
            *route=0;
            return browser_edit_event(editor,window,event) || browser_edit_leave(editor,window);
        case NX_EVENT_RESIZE:
            return browser_edit_leave(editor,window);
        default:
            return 1;
    }
}

static int browser_edit_enter(browser_editor *editor,nx_context *context,content_surface_status surface,nx_rect bounds) {
    editor->request=surface.request;
    editor->sequence=surface.sequence;
    editor->bounds=bounds;
    editor->phase=BROWSER_ENTERING;
    return nx_focus_set(context,BROWSER_EDITOR_ID)==NX_OK &&
           browser_edit_queue(editor,(content_browser_edit){.kind=CONTENT_BROWSER_FOCUS});
}

static int browser_edit_stop(browser_editor *editor,nx_window *window) {
    editor->count=0;
    editor->awaiting=0;
    editor->receiving=0;
    editor->commit=(content_browser_edit){0};
    editor->phase=BROWSER_HOST;
    editor->draining|=editor->pressed;
    editor->pressed=0;
    editor->pointer_inside=0;
    return browser_ime_disable(editor,window);
}

static int browser_edit_update(browser_editor *editor,native_worker *worker,native_worker_state state,
    content_surface_status surface,nx_window *window,nx_context *context,nx_window_info info,int *redraw) {
    if (editor->phase==BROWSER_HOST) return 1;
    int same=same_browser_surface(editor->request,surface.request);
    if (editor->phase!=BROWSER_LEAVING && (!same || nx_focus_id(context)!=BROWSER_EDITOR_ID) &&
        !browser_edit_leave(editor,window)) return 0;
    if (editor->awaiting && state.completed==editor->awaiting) {
        editor->awaiting=0;
        if (state.result!=NX_OK || (same && surface.sequence!=editor->sequence)) return 0;
        if (editor->operation==CONTENT_BROWSER_BLUR) {
            editor->phase=BROWSER_HOST;
            *redraw=1;
            if (nx_focus_set(context,15)!=NX_OK) return 0;
            puts("BROWSER_IME released");
        } else if (editor->phase!=BROWSER_LEAVING) {
            nx_rect caret=state.browser_caret;
            if (caret.x+caret.width>info.width-32 || caret.y+caret.height>info.height-152) return 0;
            caret.x+=16;caret.y+=136;
            if (nx_window_set_ime_rect(window,caret)!=NX_OK) return 0;
            if (editor->operation==CONTENT_BROWSER_FOCUS || editor->operation==CONTENT_BROWSER_COMMIT) {
                if (editor->revision==UINT64_MAX ||
                    nx_window_set_ime_surrounding_text(window,BROWSER_EDITOR_ID,++editor->revision,"",0,0,0)!=NX_OK)
                    return 0;
            }
            if (editor->operation>=CONTENT_BROWSER_MOUSE_MOVE)
                printf("BROWSER_POINTER kind=%u adopted\n",editor->operation);
            else
                printf("BROWSER_IME kind=%u revision=%llu caret=%.0f,%.0f,%.0f,%.0f\n",editor->operation,
                    (unsigned long long)editor->revision,
                    (double)caret.x,(double)caret.y,(double)caret.width,(double)caret.height);
            if (editor->phase==BROWSER_ENTERING) {
                if (nx_window_set_ime_enabled(window,1)!=NX_OK) return 0;
                editor->enabled=1;
                editor->phase=BROWSER_EDITING;
                *redraw=1;
            }
        }
        fflush(stdout);
    }
    if (!editor->awaiting && editor->count && !state.working && !state.pending) {
        if (editor->sequence==UINT64_MAX || state.completed==UINT_MAX) return 0;
        /* Complete a release on the old engine geometry even when the view
           resized. The surface rejects those old pixels independently. */
        native_job job={.request=editor->request,.sequence=editor->sequence+1,.edit=editor->pending[0]};
        if (!native_schedule(worker,job)) return 0;
        memmove(editor->pending,editor->pending+1,(--editor->count)*sizeof(editor->pending[0]));
        editor->awaiting=state.completed+1;
        editor->operation=job.edit.kind;
        editor->sequence=job.sequence;
    }
    return 1;
}

#endif
