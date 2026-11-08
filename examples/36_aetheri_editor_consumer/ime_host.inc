/* Included after the original layout/message/clipboard helpers. All document,
   save and renderer ownership stays in that original host. */
static bool editor_ime_same(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static bool editor_ime_initialize(struct app *a) {
    nx_result result=nx_window_set_ime_transaction_events(a->window,1);
    if(result!=NX_OK && result!=NX_ERROR_UNSUPPORTED)return false;
    a->ime.available=result==NX_OK;a->ime.semantic=identity(a);
    if(a->diagnostics) {printf("EDITOR_IME available=%u\n",a->ime.available?1u:0u);fflush(stdout);}
    return a->ime.semantic!=0;
}
static void editor_ime_cancel(struct app *a) {
    if(a->ime.owns && a->window) {
        nx_result withdrawn=nx_window_set_ime_surrounding_text(a->window,0,0,NULL,0,0,0);
        nx_result disabled=nx_window_set_ime_enabled(a->window,0);
        if(withdrawn!=NX_OK || disabled!=NX_OK)a->ime.failed=true;
    }
    a->ime.owns=false;a->ime.length=0;a->ime.rect_sent=false;
    editor_ime_batch_invalidate(&a->ime.batch);
}
static bool editor_ime_active(struct app *a,struct ae_direct **input,uint64_t *target) {
    *input=NULL;*target=0;
    if(!a->ime.available || !a->focused || a->mode!=EDITING || a->review.id || !a->document ||
       a->document!=ae_editor_workspace_active(a->workspace))return false;
    struct ids *ids=tab_ids(a,a->document);if(!ids || nx_focus_id(a->context)!=ids->canvas)return false;
    *input=ae_editor_workspace_input(a->workspace);*target=ids->canvas;return *input!=NULL;
}
static bool editor_ime_frame(struct app *a) {
    a->ime.caret_valid=false;
    struct ae_direct *input;uint64_t target;struct ae_direct_token current;
    if(a->ime.owns && (!editor_ime_active(a,&input,&target) || target!=a->ime.batch.target ||
       !ae_direct_token(input,&current) || !editor_ime_same(current,a->ime.batch.expected)))editor_ime_cancel(a);
    return !a->ime.failed;
}
static void editor_ime_notice(struct app *a,bool memory,bool composition) {
    a->save_notice=false;
    const char *text=memory?tr(a,"Not enough memory for text input. Your document is unchanged.","Nicht genug Arbeitsspeicher für die Texteingabe. Dein Dokument bleibt unverändert."):
        composition?tr(a,"This composition cannot be shown here. Your document is unchanged.","Diese unbestätigte Eingabe kann hier nicht angezeigt werden. Dein Dokument bleibt unverändert."):
        tr(a,"The text input could not be applied. Your document is unchanged.","Die Texteingabe konnte nicht übernommen werden. Dein Dokument bleibt unverändert.");
    a->ime.notice=text;snprintf(a->message,sizeof(a->message),"%s",text);a->dirty=true;
}
static void editor_ime_clear_notice(struct app *a) {
    if(a->ime.notice && !strcmp(a->message,a->ime.notice))a->message[0]=0;
    a->ime.notice=NULL;
}
static bool editor_ime_read(struct app *a) {
    size_t length=0;int32_t anchor=0,cursor=0;
    nx_result result=nx_window_get_ime_preedit_for(a->window,a->ime.batch.target,a->ime.batch.revision,NULL,0,&length,&anchor,&cursor);
    if(result==NX_ERROR_STATE) {editor_ime_cancel(a);return !a->ime.failed;}
    if(result!=NX_OK)return false;
    if(length)cancel_paste(a);
    if(length>=sizeof(a->ime.preedit)) {
        a->ime.length=0;editor_ime_notice(a,false,true);return true;
    }
    if(nx_window_get_ime_preedit_for(a->window,a->ime.batch.target,a->ime.batch.revision,
       a->ime.preedit,sizeof(a->ime.preedit),&length,&anchor,&cursor)!=NX_OK)return false;
    size_t at=0;
    while(at<length) {
        uint32_t scalar=0;
        if(nx_utf8_decode(a->ime.preedit,length,&at,&scalar)!=NX_OK)return false;
        if(scalar<32 || (scalar>=127 && scalar<=159) || scalar==0x2028 || scalar==0x2029) {
            a->ime.length=0;editor_ime_notice(a,false,true);return true;
        }
    }
    bool hidden=anchor==-1 && cursor==-1;
    if(!hidden && (anchor<0 || cursor<0 || (size_t)anchor>length || (size_t)cursor>length ||
       ((size_t)anchor<length && ((unsigned char)a->ime.preedit[anchor]&0xc0u)==0x80u) ||
       ((size_t)cursor<length && ((unsigned char)a->ime.preedit[cursor]&0xc0u)==0x80u)))return false;
    a->ime.length=length;a->ime.anchor=anchor;a->ime.cursor=cursor;
    if(a->diagnostics) {printf("EDITOR_PREEDIT bytes=%zu anchor=%d cursor=%d epoch=%llu\n",length,anchor,cursor,(unsigned long long)a->ime.batch.revision);fflush(stdout);}
    return true;
}
static bool editor_ime_event(struct app *a,const nx_event *event,bool *handled) {
    *handled=false;
    bool native=event->type==NX_EVENT_IME_TRANSACTION_BEGIN || event->type==NX_EVENT_IME_TRANSACTION_END ||
        event->type==NX_EVENT_IME_PREEDIT_CHANGED || event->type==NX_EVENT_IME_DELETE_SURROUNDING || event->type==NX_EVENT_IME_DELETE_RANGE ||
        ((event->ime_target || event->ime_revision) && (event->type==NX_EVENT_TEXT || event->type==NX_EVENT_TEXT_COMMAND));
    if(!native) {
        if(a->ime.batch.collecting)editor_ime_cancel(a);
        if(event->type==NX_EVENT_KEY_DOWN || event->type==NX_EVENT_TEXT || event->type==NX_EVENT_POINTER_DOWN)editor_ime_clear_notice(a);
        if(event->type==NX_EVENT_KEY_DOWN || event->type==NX_EVENT_TEXT || event->type==NX_EVENT_TEXT_COMMAND ||
           event->type==NX_EVENT_FOCUS || event->type==NX_EVENT_POINTER_CANCEL || event->type==NX_EVENT_POINTER_DOWN ||
           event->type==NX_EVENT_WHEEL || (a->dragging && (event->type==NX_EVENT_POINTER_MOVE || event->type==NX_EVENT_POINTER_UP)))editor_ime_cancel(a);
        return !a->ime.failed;
    }
    /* The original filename field owns its own native path. Never withdraw or
       consume its target when the document adapter does not own the window. */
    if(!a->ime.owns)return true;
    *handled=true;struct ae_direct *input;uint64_t target;
    bool admitted=editor_ime_active(a,&input,&target) && target==a->ime.batch.target;
    if(admitted) {
        size_t length=0;int32_t anchor=0,cursor=0;
        nx_result result=nx_window_get_ime_preedit_for(a->window,target,a->ime.batch.revision,NULL,0,&length,&anchor,&cursor);
        if(result!=NX_OK && result!=NX_ERROR_STATE)return false;
        admitted=result==NX_OK;
    }
    enum editor_ime_batch_outcome outcome=editor_ime_batch_event(&a->ime.batch,input,event,admitted);
    if(a->diagnostics && outcome!=EDITOR_IME_BUFFERED) {
        printf("EDITOR_IME_BATCH outcome=%d result=%d\n",(int)outcome,(int)a->ime.batch.result);fflush(stdout);
    }
    if(outcome==EDITOR_IME_REJECTED) {
        enum ae_document_result result=a->ime.batch.result;editor_ime_cancel(a);
        if(result==AE_DOCUMENT_NOMEM || admitted)editor_ime_notice(a,result==AE_DOCUMENT_NOMEM,false);
    } else if(outcome==EDITOR_IME_APPLIED) {
        cancel_paste(a);editor_ime_clear_notice(a);a->ensure_caret=true;a->dirty=true;
        if(!editor_ime_read(a))return false;
    } else if(event->type==NX_EVENT_IME_PREEDIT_CHANGED && !editor_ime_read(a))return false;
    return !a->ime.failed;
}
static bool editor_ime_idle(struct app *a) {
    if(a->ime.failed)return false;
    if(a->ime.batch.collecting)editor_ime_cancel(a);
    struct ae_direct *input;uint64_t target;struct ae_direct_token current;
    if(!editor_ime_active(a,&input,&target) || !a->ime.caret_valid) {editor_ime_cancel(a);return !a->ime.failed;}
    if(!ae_direct_token(input,&current) || a->info.revision!=current.revision || a->info.cursor!=current.cursor)return false;
    if(!a->ime.owns || !editor_ime_same(current,a->ime.published)) {
        if(a->ime.serial==UINT64_MAX)return false;
        /* Keep a continuing native composition through our own acknowledged
           edit. External changes were already cancelled by the frame gate. */
        if(!a->ime.owns && (nx_window_set_ime_enabled(a->window,0)!=NX_OK ||
           nx_window_set_ime_purpose(a->window,NX_IME_PURPOSE_NORMAL)!=NX_OK))return false;
        size_t cursor=current.cursor,anchor=current.selecting?current.anchor:cursor;
        size_t begin=cursor<anchor?cursor:anchor,end=cursor>anchor?cursor:anchor;
        const char *bytes=a->bytes?a->bytes:"";size_t size=0,relative_cursor=0,relative_anchor=0;
        if(end-begin<=4000) {
            size_t available=4000-(end-begin),left=begin<available/2?begin:available/2;
            size_t right=a->byte_size-end<available-left?a->byte_size-end:available-left;
            begin-=left;end+=right;available-=left+right;
            begin-=begin<available?begin:available;
            while(begin<cursor && begin<anchor && ((unsigned char)bytes[begin]&0xc0u)==0x80u)++begin;
            while(end<a->byte_size && end>cursor && end>anchor && ((unsigned char)bytes[end]&0xc0u)==0x80u)--end;
            if(begin && bytes[begin]=='\n' && bytes[begin-1]=='\r')++begin;
            if(end && end<a->byte_size && bytes[end-1]=='\r' && bytes[end]=='\n')--end;
            bytes+=begin;size=end-begin;relative_cursor=cursor-begin;relative_anchor=anchor-begin;
        }
        uint64_t serial=++a->ime.serial;
        if(!editor_ime_batch_bind(&a->ime.batch,input,target,serial) ||
           nx_window_set_ime_surrounding_text(a->window,target,serial,bytes,size,relative_cursor,relative_anchor)!=NX_OK ||
           nx_window_set_ime_rect(a->window,a->ime.caret)!=NX_OK || nx_window_set_ime_enabled(a->window,1)!=NX_OK)return false;
        a->ime.owns=true;a->ime.published=current;a->ime.sent=a->ime.caret;a->ime.rect_sent=true;
        if(a->diagnostics) {printf("EDITOR_IME_CONTEXT target=%llu epoch=%llu bytes=%zu cursor=%zu anchor=%zu revision=%llu document_cursor=%zu\n",(unsigned long long)target,(unsigned long long)serial,size,relative_cursor,relative_anchor,(unsigned long long)current.revision,current.cursor);fflush(stdout);}
    } else if(!a->ime.rect_sent || a->ime.sent.x!=a->ime.caret.x || a->ime.sent.y!=a->ime.caret.y ||
              a->ime.sent.width!=a->ime.caret.width || a->ime.sent.height!=a->ime.caret.height) {
        if(nx_window_set_ime_rect(a->window,a->ime.caret)!=NX_OK)return false;
        a->ime.sent=a->ime.caret;a->ime.rect_sent=true;
    }
    return true;
}
static bool editor_ime_layout_caret(nx_text_layout *layout,size_t offset,bool ceil,nx_text_caret *out) {
    bool found=false;size_t nearest=ceil?SIZE_MAX:0;
    for(size_t i=0;i<nx_text_layout_caret_count(layout);++i) {
        nx_text_caret caret;if(nx_text_layout_caret_at(layout,i,&caret)!=NX_OK)return false;
        if((ceil && caret.byte_offset>=offset && (!found || caret.byte_offset<nearest)) ||
           (!ceil && caret.byte_offset<=offset && (!found || caret.byte_offset>nearest))) {*out=caret;nearest=caret.byte_offset;found=true;}
    }
    return found;
}
static bool editor_ime_paint(struct app *a,const struct painted_row *row,nx_point origin,const nx_text_caret *document_caret) {
    nx_rect normal={origin.x+(float)document_caret->x,origin.y+(float)document_caret->top,1.5f,(float)document_caret->height};
    a->ime.caret=normal;a->ime.caret_valid=true;
    if(!a->ime.owns || !a->ime.length)return nx_fill_rect(a->context,normal,foreground(a))==NX_OK;
    nx_text_layout *value=layout_fonts(a,a->mono_fonts,a->ime.preedit,a->ime.length,a->em,10000000);if(!value)return false;
    nx_text_caret caret={0},first={0},last={0};bool hidden=a->ime.cursor<0;
    size_t anchor=hidden?0:(size_t)a->ime.anchor,cursor=hidden?0:(size_t)a->ime.cursor;
    size_t lo=anchor<cursor?anchor:cursor,hi=anchor>cursor?anchor:cursor;
    if(!editor_ime_layout_caret(value,cursor,true,&caret) || !editor_ime_layout_caret(value,lo,false,&first) ||
       !editor_ime_layout_caret(value,hi,true,&last))return false;
    float x=fmaxf(row->bounds.x,fminf(normal.x,row->bounds.x+row->bounds.width-fminf(160*a->scale,row->bounds.width)));
    float available=row->bounds.x+row->bounds.width-x;
    float scroll=fmaxf(0,(float)caret.x-available+12),width=fminf(available,(float)nx_text_layout_width(value)+12);
    nx_point place={x-scroll,row->bounds.y};
    if(nx_push_clip(a->context,(nx_rect){x,row->bounds.y,available,row->bounds.height})!=NX_OK)return false;
    if(nx_fill_rect(a->context,(nx_rect){x,row->bounds.y,width,row->bounds.height},background(a))!=NX_OK)return false;
    size_t count=0;nx_text_selection_rect *rectangles=NULL;
    nx_result selection_result=nx_text_layout_selection(value,first.byte_offset,last.byte_offset,NULL,0,&count);
    if((selection_result!=NX_OK && selection_result!=NX_ERROR_LIMIT) || count>4001)return false;
    if(count) {rectangles=malloc(count*sizeof(*rectangles));if(!rectangles || nx_text_layout_selection(value,first.byte_offset,last.byte_offset,rectangles,count,&count)!=NX_OK) {free(rectangles);return false;}}
    bool okay=true;
    for(size_t i=0;i<count && okay;++i)okay=nx_fill_rect(a->context,(nx_rect){place.x+(float)rectangles[i].x,place.y+(float)rectangles[i].y,(float)rectangles[i].width,(float)rectangles[i].height},selected(a))==NX_OK;
    if(okay)okay=nx_draw_text_layout(a->context,value,place,foreground(a),CACHE)==NX_OK;
    if(a->contrast)for(size_t i=0;i<count && okay;++i) {
        nx_rect rect={place.x+(float)rectangles[i].x,place.y+(float)rectangles[i].y,(float)rectangles[i].width,(float)rectangles[i].height};
        okay=nx_push_clip(a->context,rect)==NX_OK && nx_draw_text_layout(a->context,value,place,rgb(0),CACHE)==NX_OK && nx_pop_clip(a->context)==NX_OK;
    }
    if(okay)okay=nx_fill_rect(a->context,(nx_rect){x,row->bounds.y+row->bounds.height-2,width,1},focus_color(a))==NX_OK;
    free(rectangles);if(!okay)return false;
    a->ime.caret=(nx_rect){place.x+(float)caret.x,place.y+(float)caret.top,1.5f,(float)caret.height};
    if(!hidden && nx_fill_rect(a->context,a->ime.caret,foreground(a))!=NX_OK)return false;
    struct ids *ids=tab_ids(a,a->document);if(!ids)return false;
    nx_semantic_desc semantic={.id=a->ime.semantic,.parent_id=ids->canvas,.role=NX_ROLE_TEXT,.bounds={x,row->bounds.y,width,row->bounds.height},
        .name=span(tr(a,"Uncommitted composition","Unbestätigte Eingabe")),.value={a->ime.preedit,a->ime.length},.state=NX_SEM_READONLY};
    bool semantic_ok=(hidden?nx_semantics_add_readonly_text_snapshot_without_caret(a->context,&semantic,a->ime.serial,NULL):
        nx_semantics_add_readonly_text_snapshot(a->context,&semantic,a->ime.serial,anchor,cursor,NULL))==NX_OK;
    return nx_pop_clip(a->context)==NX_OK && semantic_ok;
}
