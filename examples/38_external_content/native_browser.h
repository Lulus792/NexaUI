#ifndef CONTENT_NATIVE_BROWSER_H
#define CONTENT_NATIVE_BROWSER_H

#define BROWSER_EDITOR_ID UINT64_C(100)
#define BROWSER_EDIT_QUEUE 8u
enum browser_input_phase { BROWSER_HOST, BROWSER_ENTERING, BROWSER_EDITING, BROWSER_LEAVING };
typedef struct browser_editor {
    enum browser_input_phase phase;
    content_request request;
    uint64_t revision, sequence;
    unsigned count, awaiting, operation;
    int enabled, receiving;
    content_browser_edit pending[BROWSER_EDIT_QUEUE], commit;
} browser_editor;

static int same_browser_surface(content_request a,content_request b) {
    return a.generation==b.generation && a.resize==b.resize && a.width==b.width && a.height==b.height;
}

static int browser_edit_queue(browser_editor *editor,content_browser_edit edit) {
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
    editor->count=0;
    editor->receiving=0;
    editor->commit=(content_browser_edit){0};
    editor->phase=BROWSER_LEAVING;
    return browser_ime_disable(editor,window) &&
           browser_edit_queue(editor,(content_browser_edit){.kind=CONTENT_BROWSER_BLUR});
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
        case NX_EVENT_FOCUS:
            return event->focused || browser_edit_leave(editor,window);
        case NX_EVENT_RESIZE:
            return browser_edit_leave(editor,window);
        default:
            return 1;
    }
}

static int browser_edit_enter(browser_editor *editor,nx_context *context,content_request request) {
    editor->request=request;
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
    return browser_ime_disable(editor,window);
}

static int browser_edit_update(browser_editor *editor,native_worker *worker,native_worker_state state,
    content_surface_status surface,nx_window *window,nx_context *context,nx_window_info info,int *redraw) {
    if (editor->phase==BROWSER_HOST) return 1;
    if (!same_browser_surface(editor->request,surface.request)) {
        *redraw=1;
        return browser_edit_stop(editor,window);
    }
    if (editor->phase!=BROWSER_LEAVING && nx_focus_id(context)!=BROWSER_EDITOR_ID &&
        !browser_edit_leave(editor,window)) return 0;
    if (editor->awaiting && state.completed==editor->awaiting) {
        editor->awaiting=0;
        if (state.result!=NX_OK || surface.sequence!=editor->sequence) return 0;
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
            if (editor->operation!=CONTENT_BROWSER_PREEDIT) {
                if (editor->revision==UINT64_MAX ||
                    nx_window_set_ime_surrounding_text(window,BROWSER_EDITOR_ID,++editor->revision,"",0,0,0)!=NX_OK)
                    return 0;
            }
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
        if (surface.sequence==UINT64_MAX || state.completed==UINT_MAX) return 0;
        native_job job={.request=surface.request,.sequence=surface.sequence+1,.edit=editor->pending[0]};
        if (!native_schedule(worker,job)) return 0;
        memmove(editor->pending,editor->pending+1,(--editor->count)*sizeof(editor->pending[0]));
        editor->awaiting=state.completed+1;
        editor->operation=job.edit.kind;
        editor->sequence=job.sequence;
    }
    return 1;
}

#endif
