/* Included after the original sync_document and clipboard helpers. No native
   snapshot retains this binding or the original controller. */
struct editor_document_binding {
    struct app *app;
    struct ae_direct_token token;
    uint64_t document,serial;
};
static bool editor_document_same(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static bool editor_document_blocked(struct app *a) {
    struct ae_editor_close_token review;
    return a->mode!=EDITING || ae_editor_workspace_review(a->workspace,&review);
}
static bool editor_document_state(struct app *a,nx_text_document_state *out) {
    struct ae_direct_token token={0};struct ae_direct *input=ae_editor_workspace_input(a->workspace);
    bool blocked=editor_document_blocked(a);
    if(!a->document || a->document!=ae_editor_workspace_active(a->workspace) ||
       a->info.size!=a->byte_size)return false;
    if(input) {
        if(!ae_direct_token(input,&token) || token.identity!=a->document || token.revision!=a->revision)return false;
    } else if(!blocked)return false;
    /* The original workspace deliberately withholds its input controller while
       a close review is open. Preserve its read-only view through existing
       info/selection getters; zero token is never treated as an input grant. */
    size_t anchor=a->selection.active?(a->info.cursor==a->selection.begin?a->selection.end:a->selection.begin):a->info.cursor;
    uint32_t operations=blocked?0u:NX_TEXT_DOCUMENT_REPLACE|NX_TEXT_DOCUMENT_SELECT;
#ifdef EDITOR_DOCUMENT_WITH_CLIPBOARD
    /* Only a real window supplies the native clipboard. The headless original
       model owner remains a valid replace/select-only consumer. */
    if(!blocked && a->window)operations|=NX_TEXT_DOCUMENT_COPY|NX_TEXT_DOCUMENT_CUT|NX_TEXT_DOCUMENT_PASTE;
#endif
    struct editor_native_document *p=&a->native_document;
    if(!editor_document_same(p->token,token) || p->operations!=operations ||
       p->document!=a->document || p->content_revision!=a->revision || p->anchor!=anchor || p->cursor!=a->info.cursor) {
        if(p->serial==UINT64_MAX)return false;
        ++p->serial;p->token=token;p->operations=operations;p->document=a->document;
        p->content_revision=a->revision;p->anchor=anchor;p->cursor=a->info.cursor;
    }
    if(!p->serial)return false;
    *out=(nx_text_document_state){{a->bytes?a->bytes:"",a->byte_size},a->document,p->serial,
        anchor,a->info.cursor,operations,1};
    return true;
}
static bool editor_document_current(struct editor_document_binding *b,
    const nx_text_document_request *request,struct ae_direct **out) {
    struct app *a=b->app;struct ae_direct_token now;
    *out=ae_editor_workspace_input(a->workspace);
    return !editor_document_blocked(a) && b->document==a->document &&
        b->document==ae_editor_workspace_active(a->workspace) &&
        request->identity==b->token.identity && request->revision==b->serial &&
        a->native_document.serial==b->serial && ae_direct_token(*out,&now) &&
        editor_document_same(now,b->token);
}
static nx_result editor_document_reserve(struct app *a,const nx_text_document_request *r) {
    if(r->start_byte>r->end_byte || r->end_byte>a->byte_size || (!r->value.data && r->value.length))return NX_ERROR_ARGUMENT;
    size_t length=a->byte_size-(r->end_byte-r->start_byte);
    if(r->value.length>NX_TEXT_VALUE_MAX_BYTES || r->value.length>AE_DOCUMENT_BYTES-length)return NX_ERROR_LIMIT;
    length+=r->value.length;size_t lines=a->line_count;
    for(size_t i=r->start_byte;i<r->end_byte;++i)if(a->bytes[i]=='\n')--lines;
    for(size_t i=0;i<r->value.length;++i)if(r->value.data[i]=='\n')++lines;
    if(!lines || lines>AE_DOCUMENT_LINES || lines>SIZE_MAX/sizeof(*a->lines))return NX_ERROR_LIMIT;
    /* Reserve all UI copies before the original atomic operation. Reallocation
       may replace the input span: the SDK has already copied that snapshot. */
    if(length>a->byte_capacity) {
        char *bytes=realloc(a->bytes,length);if(!bytes)return NX_ERROR_MEMORY;
        a->bytes=bytes;a->byte_capacity=length;
    }
    if(lines>a->line_capacity) {
        struct line *index=realloc(a->lines,lines*sizeof(*index));if(!index)return NX_ERROR_MEMORY;
        a->lines=index;a->line_capacity=lines;
    }
    return NX_OK;
}
static nx_result editor_document_apply(void *user,const nx_text_document_request *request,
    nx_text_document_state *out) {
    struct editor_document_binding *b=user;struct app *a=b->app;struct ae_direct *input;
    if(!editor_document_current(b,request,&input))return NX_ERROR_STATE;
    if(a->native_document.serial==UINT64_MAX)return NX_ERROR_LIMIT;
    if(request->operation==NX_TEXT_DOCUMENT_REPLACE_RANGE) {
        nx_result reserved=editor_document_reserve(a,request);if(reserved!=NX_OK)return reserved;
        if(!editor_document_current(b,request,&input))return NX_ERROR_STATE;
        enum ae_document_result result=editor_direct_ime_replace(input,&b->token,
            request->start_byte,request->end_byte,request->value.data,request->value.length);
        if(result!=AE_DOCUMENT_OK)return result==AE_DOCUMENT_NOMEM?NX_ERROR_MEMORY:
            result==AE_DOCUMENT_LIMIT || result==AE_DOCUMENT_EXHAUSTED?NX_ERROR_LIMIT:NX_ERROR_ARGUMENT;
    } else if(request->operation==NX_TEXT_DOCUMENT_SET_SELECTION) {
        if(!editor_direct_accessible_select_bytes(input,&b->token,request->start_byte,request->end_byte))return NX_ERROR_ARGUMENT;
    } else return NX_ERROR_UNSUPPORTED;
    /* Valid original-model postconditions make this allocation-free: text and
       line capacities were reserved above; selection changes retain both. An
       unexpected broken model invariant is fatal, never a claimed rollback. */
    if(!sync_document(a) || !editor_document_state(a,out))return NX_ERROR_STATE;
    return NX_OK;
}
static bool editor_native_document_register(struct app *a,struct ids *ids) {
    nx_text_document_state state;if(!editor_document_state(a,&state))return false;
    bool blocked=!state.operations;
    nx_widget_desc widget={.semantic={.id=ids->canvas,.parent_id=1,.role=NX_ROLE_TEXT_FIELD,.bounds=a->canvas,
        .name=span(tr(a,"Edit document","Dokument bearbeiten")),
        .state=blocked?NX_SEM_DISABLED|NX_SEM_READONLY:0,
        .actions=NX_ACTION_FOCUS|(blocked?0u:NX_ACTION_SET_VALUE)},
        .flags=NX_WIDGET_INTERACTIVE|NX_WIDGET_FOCUSABLE|NX_WIDGET_SCROLLABLE};
    struct editor_document_binding binding={a,a->native_document.token,a->document,state.revision};
    nx_text_document_result result;
    nx_result registered;
#ifdef EDITOR_DOCUMENT_WITH_CLIPBOARD
    if(a->window) {
        nx_text_document_clipboard clipboard={0};
        if(nx_text_clipboard_for_window(a->window,&clipboard.clipboard)!=NX_OK)return false;
        registered=nx_text_document_register_with_clipboard(a->context,&widget,&state,
            editor_document_apply,&binding,&clipboard,&result);
    } else
#endif
        registered=nx_text_document_register(a->context,&widget,&state,editor_document_apply,&binding,&result);
    if(registered!=NX_OK)return false;
    if(result.applied) {
        if(a->native_document.notice && !strcmp(a->message,a->native_document.notice))a->message[0]=0;
        a->native_document.notice=NULL;
        cancel_paste(a);
#ifdef EDITOR_DOCUMENT_WITH_IME
        editor_ime_cancel(a);if(a->ime.failed)return false;
#endif
        a->ensure_caret=true;a->native_document.changed=true;
        if(a->diagnostics) {printf("EDITOR_ACCESSIBLE applied=1 bytes=%zu serial=%llu\n",a->byte_size,(unsigned long long)a->native_document.serial);fflush(stdout);}
    } else if(result.edit_result==NX_ERROR_MEMORY || result.edit_result==NX_ERROR_LIMIT) {
        a->save_notice=false;
        a->native_document.notice=tr(a,"The assistive edit could not be applied. Your document is unchanged.",
            "Die assistive Änderung konnte nicht übernommen werden. Dein Dokument bleibt unverändert.");
        snprintf(a->message,sizeof(a->message),"%s",a->native_document.notice);
    }
    if(a->focus_document && !blocked) {if(nx_focus_set(a->context,ids->canvas)!=NX_OK)return false;a->focus_document=false;}
    return true;
}
