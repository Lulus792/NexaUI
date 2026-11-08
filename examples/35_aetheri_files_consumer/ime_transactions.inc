/* Included in the separately generated view after its original edit helpers.
   Preview uses public buffers; only the real field receives the marked edit. */
static bool native_current(struct ae_files_view *v,const nx_event *begin,bool *current) {
    *current=false;
    uint64_t id=control(v,v->s->dialog?AE_FV_NAME:AE_FV_SEARCH);
    if(!editing(v) || v->native_reject_pending ||
       (v->s->dialog?v->s->name_bad:v->s->query_bad) ||
       begin->ime_target!=id || nx_focus_id(v->ui)!=id || !begin->ime_revision) return true;
    size_t length=0;int32_t a=0,c=0;
    nx_result result=nx_window_get_ime_preedit_for(v->ime_window,id,begin->ime_revision,NULL,0,&length,&a,&c);
    if(result==NX_ERROR_STATE || result==NX_ERROR_UNSUPPORTED) return true;
    if(result!=NX_OK) return false;
    *current=true;return true;
}
static void native_reject(struct ae_files_view *v) {
    v->native_reject_pending=true;v->native_reject_scope=v->base;
}
static bool native_proposal(struct ae_files_view *v,bool *accepted) {
    *accepted=false;
    const nx_event *begin=&v->native_events[0],*deletion=NULL;
    uint32_t flags=begin->ime_before;
    if(!(flags&(NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE)) ||
       (flags&~(NX_IME_TRANSACTION_COMMIT|NX_IME_TRANSACTION_DELETE))) return true;
    char commit[AE_FILES_NAME+1];size_t bytes=0,texts=0;bool ended=false;
    for(size_t i=1;i+1<v->native_count;++i) {
        const nx_event *e=&v->native_events[i];
        if(e->type==NX_EVENT_IME_PREEDIT_CHANGED && !e->ime_target && !e->ime_revision) continue;
        if(e->ime_target!=begin->ime_target || e->ime_revision!=begin->ime_revision) return true;
        if(e->type==NX_EVENT_IME_PREEDIT_CHANGED) continue;
        if(e->type==NX_EVENT_IME_DELETE_SURROUNDING || e->type==NX_EVENT_IME_DELETE_RANGE) {
            if(deletion || texts || !(flags&NX_IME_TRANSACTION_DELETE)) return true;
            deletion=e;continue;
        }
        char encoded[4];size_t n=e->type==NX_EVENT_TEXT?scalar(e->scalar,encoded):0;
        if(!n || !(flags&NX_IME_TRANSACTION_COMMIT) || ended ||
           (e->ime_before&~(NX_IME_COMMIT_START|NX_IME_COMMIT_END)) ||
           (!!(e->ime_before&NX_IME_COMMIT_START)!=!texts) ||
           e->scalar<32 || e->scalar==127 || (e->modifiers&NX_MOD_SUPER) ||
           ((e->modifiers&NX_MOD_CONTROL) && !(e->modifiers&NX_MOD_ALT)) ||
           n>AE_FILES_NAME-bytes) return true;
        memcpy(commit+bytes,encoded,n);bytes+=n;++texts;ended=(e->ime_before&NX_IME_COMMIT_END)!=0;
    }
    if(!!deletion!=!!(flags&NX_IME_TRANSACTION_DELETE) || (texts && !ended)) return true;
    nx_utf8_span source={0};size_t anchor=0,cursor=0;
    bool previous=v->native_preview && v->native_preview_target==begin->ime_target &&
        v->native_preview_revision==begin->ime_revision && v->native_preview_scope==v->base;
    if(previous) {
        if(nx_text_buffer_text(v->native_preview,&source.data,&source.length)!=NX_OK ||
           nx_text_buffer_selection(v->native_preview,&anchor,&cursor)!=NX_OK) return false;
    } else {
        struct edit *edit=&v->edits[v->s->dialog?1:0];
        if(!edit->field || nx_text_field_text(edit->field,&source)!=NX_OK ||
           nx_text_field_selection(edit->field,&anchor,&cursor)!=NX_OK) return false;
    }
    /* Form the complete byte proposal before asking the public buffer for its
       grapheme-aligned caret. A separate delete followed by insert would snap
       the intermediate caret and could insert past a newly joined cluster. */
    size_t start=anchor<cursor?anchor:cursor,finish=anchor>cursor?anchor:cursor;
    if(deletion && deletion->type==NX_EVENT_IME_DELETE_SURROUNDING) {
        if((!deletion->ime_before && !deletion->ime_after) || deletion->ime_before>start ||
           deletion->ime_after>source.length-finish) return true;
        start-=deletion->ime_before;finish+=deletion->ime_after;
    } else if(deletion) {
        if(!deletion->ime_length) return true;
        if(deletion->ime_index<0) {
            uint64_t before=(uint64_t)-(int64_t)deletion->ime_index;
            if(before>cursor) return true;
            start=cursor-(size_t)before;
        } else {
            size_t after=(size_t)deletion->ime_index;
            if(after>source.length-cursor) return true;
            start=cursor+after;
        }
        if(deletion->ime_length>source.length-start) return true;
        finish=start+deletion->ime_length;
    }
    if((start<source.length && ((unsigned char)source.data[start]&0xc0u)==0x80u) ||
       (finish<source.length && ((unsigned char)source.data[finish]&0xc0u)==0x80u)) return true;
    size_t remaining=source.length-(finish-start);
    if(remaining>AE_FILES_NAME || bytes>AE_FILES_NAME-remaining) return true;
    char next[AE_FILES_NAME+1];size_t length=remaining+bytes;
    memcpy(next,source.data,start);memcpy(next+start,commit,bytes);
    memcpy(next+start+bytes,source.data+finish,source.length-finish);next[length]=0;
    nx_utf8_span proposed={next,length};struct field_validation rule={v->s->dialog,false};
    if(field_valid(proposed,&rule)) {
        nx_text_buffer *candidate=NULL;
        if(nx_text_buffer_create(next,length,0,&candidate)!=NX_OK) return false;
        size_t caret=start+bytes;nx_result selected;
        do {selected=nx_text_buffer_set_selection(candidate,caret,caret);
            if(selected==NX_ERROR_ARGUMENT && caret<length) ++caret;else break;
        } while(caret<=length);
        if(selected!=NX_OK) {nx_text_buffer_destroy(candidate);return false;}
        for(size_t i=0;i<v->native_count;++i) if(nx_context_queue_event(v->ui,&v->native_events[i])!=NX_OK) {
            nx_text_buffer_destroy(candidate);return false;
        }
        nx_text_buffer_destroy(v->native_preview);v->native_preview=candidate;
        v->native_preview_target=begin->ime_target;v->native_preview_revision=begin->ime_revision;
        v->native_preview_scope=v->base;*accepted=true;return true;
    }
    return true;
}
static void native_discard(struct ae_files_view *v) {
    v->native_depth=v->native_count=0;v->native_bad=v->native_ignore=false;
}
static bool native_transaction(struct ae_files_view *v,const nx_event *e,bool *handled) {
    *handled=false;
    if(v->native_depth && v->native_scope!=v->base) native_discard(v);
    if(e->type==NX_EVENT_IME_TRANSACTION_BEGIN) {
        *handled=true;
        if(v->native_depth && (e->ime_target!=v->native_begin.ime_target || e->ime_revision!=v->native_begin.ime_revision)) {
            bool current;if(!native_current(v,e,&current)) return false;
            if(current) native_discard(v);
        }
        if(v->native_depth==SIZE_MAX) return false;
        if(!v->native_depth) {
            bool current;if(!native_current(v,e,&current)) return false;
            v->native_begin=*e;v->native_scope=v->base;v->native_ignore=!current;
            v->native_bad=false;v->native_count=1;v->native_events[0]=*e;
        } else v->native_bad=true;
        ++v->native_depth;return true;
    }
    if(e->type==NX_EVENT_IME_TRANSACTION_END) {
        *handled=true;
        if(!v->native_depth) return true;
        if(v->native_depth>1) {--v->native_depth;v->native_bad=true;return true;}
        if(e->ime_target!=v->native_begin.ime_target || e->ime_revision!=v->native_begin.ime_revision ||
           e->ime_before!=v->native_begin.ime_before) {v->native_bad=true;return true;}
        v->native_depth=0;
        if(v->native_ignore || v->native_scope!=v->base) return true;
        bool current;if(!native_current(v,&v->native_begin,&current)) return false;
        if(!current) return true;
        bool accepted=false;
        if(!v->native_bad && v->native_count<sizeof(v->native_events)/sizeof(v->native_events[0])) {
            v->native_events[v->native_count++]=*e;
            if(!native_proposal(v,&accepted)) return false;
        }
        if(!accepted) native_reject(v);
        return true;
    }
    bool payload=e->type==NX_EVENT_TEXT || e->type==NX_EVENT_IME_DELETE_SURROUNDING ||
        e->type==NX_EVENT_IME_DELETE_RANGE || e->type==NX_EVENT_IME_PREEDIT_CHANGED ||
        (e->type==NX_EVENT_TEXT_COMMAND && (e->ime_target || e->ime_revision));
    if(v->native_depth && payload) {
        *handled=true;
        if(v->native_count<sizeof(v->native_events)/sizeof(v->native_events[0]))
            v->native_events[v->native_count++]=*e;
        else v->native_bad=true;
        return true;
    }
    if(v->native_depth) {
        if(!v->native_ignore && v->native_scope==v->base) reject_text(v);
        native_discard(v);
    }
    if((e->ime_target || e->ime_revision) && payload && e->type!=NX_EVENT_IME_PREEDIT_CHANGED) *handled=true;
    return true;
}
