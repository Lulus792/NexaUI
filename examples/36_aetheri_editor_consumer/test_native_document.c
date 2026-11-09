/* Compile the actual generated owner and its real document adapter; the renamed
   native main stays available to the linker but no window is created here. */
#define main original_editor_native_main
#include EDITOR_NATIVE_DOCUMENT_HOST
#undef main

static size_t test_realloc_calls,test_realloc_fail_at;
static bool test_malloc_fail;
void *__real_malloc(size_t);
void *__real_realloc(void *,size_t);
void *__wrap_malloc(size_t size) {return test_malloc_fail?NULL:__real_malloc(size);}
void *__wrap_realloc(void *pointer,size_t size) {
    ++test_realloc_calls;
    return test_realloc_fail_at==test_realloc_calls?NULL:__real_realloc(pointer,size);
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"native document owner %d: %s\n",__LINE__,#x);return 1;}} while(0)
static int test_create(struct app *a,const char *bytes,size_t size) {
    *a=(struct app){0};a->next_id=100;a->canvas=(nx_rect){0,0,300,200};
    a->workspace=ae_editor_workspace_create();uint64_t id=0;
    CHECK(a->workspace && ae_editor_workspace_new(a->workspace,&id)==AE_EDITOR_WORKSPACE_OK);
    struct ae_direct *input=ae_editor_workspace_input(a->workspace);struct ae_direct_token token;
    CHECK(ae_direct_token(input,&token) && editor_direct_ime_replace(input,&token,0,0,bytes,size)==AE_DOCUMENT_OK);
    CHECK(sync_document(a) && nx_context_create(NULL,&a->context)==NX_OK);
    return 0;
}
static void test_destroy(struct app *a) {
#ifdef EDITOR_DOCUMENT_WITH_PRESENTATION
    editor_presentation_close(&a->native_document.presentation);
#endif
    nx_context_destroy(a->context);ae_editor_workspace_destroy(a->workspace);free(a->bytes);free(a->lines);
}
static int test_frame(struct app *a) {
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,300,200},.name={"Editor",6}};
    CHECK(sync_document(a) && nx_begin_frame(a->context,300,200,0)==NX_OK && nx_semantics_add(a->context,&root,NULL)==NX_OK);
    struct ids *ids=tab_ids(a,a->document);CHECK(ids && editor_native_document_register(a,ids) && nx_end_frame(a->context)==NX_OK);
    return 0;
}
static int test_text(struct app *a,const char *bytes,size_t size) {
    CHECK(sync_document(a) && a->byte_size==size && (!size || !memcmp(a->bytes,bytes,size)));return 0;
}
static struct editor_document_binding test_binding(struct app *a) {
    return (struct editor_document_binding){a,a->native_document.token,a->document,a->native_document.serial};
}
static nx_text_document_request test_request(struct editor_document_binding b,nx_text_document_operation kind,
    size_t begin,size_t end,const char *bytes,size_t size) {
    return (nx_text_document_request){kind,b.token.identity,b.serial,begin,end,{bytes,size}};
}
static int test_public_frame(void) {
    struct app a;CHECK(!test_create(&a,"A\xc3\xa9\xf0\x9f\x8c\x8dZ",8) && !test_frame(&a));
    struct ids *ids=tab_ids(&a,a.document);uint64_t initial=a.native_document.serial;
    CHECK(nx_semantics_request_text_value(a.context,ids->canvas,(nx_utf8_span){"a\xcc\x81" "b\nline",9})==NX_OK);
    CHECK(!test_frame(&a) && a.native_document.changed && a.native_document.serial>initial && !test_text(&a,"a\xcc\x81" "b\nline",9));
    bool changed=false;struct ae_direct *input=ae_editor_workspace_input(a.workspace);
    CHECK(ae_direct_history(input,-1,&changed)==AE_DOCUMENT_OK && changed && !test_text(&a,"A\xc3\xa9\xf0\x9f\x8c\x8dZ",8));
    CHECK(ae_direct_history(input,1,&changed)==AE_DOCUMENT_OK && changed && !test_text(&a,"a\xcc\x81" "b\nline",9));
    CHECK(!test_frame(&a));struct editor_document_binding binding=test_binding(&a);nx_text_document_state out;
    nx_text_document_request request=test_request(binding,NX_TEXT_DOCUMENT_SET_SELECTION,0,3,NULL,0);
    CHECK(editor_document_apply(&binding,&request,&out)==NX_OK && out.anchor_byte==0 && out.cursor_byte==3);
    struct ae_direct_selection selected;CHECK(ae_direct_selection(input,&selected) && selected.active && selected.begin==0 && selected.end==3);
    binding=test_binding(&a);request=test_request(binding,NX_TEXT_DOCUMENT_SET_SELECTION,0,1,NULL,0);
    CHECK(editor_document_apply(&binding,&request,&out)==NX_ERROR_ARGUMENT);
    CHECK(ae_direct_selection(input,&selected) && selected.active && selected.end==3);
    test_destroy(&a);return 0;
}
static int test_policy_and_review(void) {
    struct app a;CHECK(!test_create(&a,"seed",4) && !test_frame(&a));
    struct ids *ids=tab_ids(&a,a.document);uint64_t first=a.native_document.serial;struct ae_direct_token original=a.native_document.token;
    CHECK(nx_semantics_request_text_value(a.context,ids->canvas,(nx_utf8_span){"late",4})==NX_OK);
    a.mode=PICK_OPEN;CHECK(!test_frame(&a) && !a.native_document.operations && a.native_document.serial>first && !test_text(&a,"seed",4));
    CHECK(editor_document_same(original,a.native_document.token));uint64_t blocked=a.native_document.serial;
    a.mode=EDITING;CHECK(!test_frame(&a) && a.native_document.operations && a.native_document.serial>blocked && !test_text(&a,"seed",4));
    struct ae_editor_close_token review;
    CHECK(ae_editor_workspace_request_close(a.workspace,a.document,&review)==AE_EDITOR_WORKSPACE_REVIEW);
    CHECK(!ae_editor_workspace_input(a.workspace) && !test_frame(&a) && !a.native_document.operations && !a.native_document.token.identity);
    CHECK(!test_text(&a,"seed",4));uint32_t index;nx_semantic_view view;
    CHECK(nx_semantics_find(a.context,ids->canvas,&index)==NX_OK && nx_semantics_get(a.context,index,&view)==NX_OK &&
        view.value.length==4 && !memcmp(view.value.data,"seed",4) && (view.state&NX_SEM_READONLY));
    CHECK(ae_editor_workspace_resolve_close(a.workspace,&review,AE_EDITOR_CLOSE_CANCEL)==AE_EDITOR_WORKSPACE_OK && !test_frame(&a));
    CHECK(nx_semantics_request_text_value(a.context,ids->canvas,(nx_utf8_span){"after",5})==NX_OK && !test_frame(&a) && !test_text(&a,"after",5));
    test_destroy(&a);return 0;
}
static int test_stale_tokens(void) {
    for(unsigned field=0;field<8;++field) {
        struct app a;CHECK(!test_create(&a,"seed",4) && !test_frame(&a));
        struct editor_document_binding b=test_binding(&a);nx_text_document_state out;
        nx_text_document_request r=test_request(b,NX_TEXT_DOCUMENT_REPLACE_RANGE,0,4,"changed\n",8);
        if(field==0)++b.token.identity;else if(field==1)++b.token.interaction;else if(field==2)++b.token.revision;
        else if(field==3)++b.token.cursor;else if(field==4)++b.token.anchor;else if(field==5)b.token.selecting=!b.token.selecting;
        else if(field==6)++b.document;else ++b.serial;
        test_realloc_calls=0;CHECK(editor_document_apply(&b,&r,&out)==NX_ERROR_STATE && !test_realloc_calls && !test_text(&a,"seed",4));
        test_destroy(&a);
    }
    return 0;
}
static int test_allocation_failures(void) {
    char value[8192];memset(value,'x',sizeof(value));for(unsigned i=0;i<32;++i)value[i*64]='\n';
    for(unsigned failure=0;failure<3;++failure) {
        struct app a;CHECK(!test_create(&a,"seed",4) && !test_frame(&a));
        struct ae_direct *input=ae_editor_workspace_input(a.workspace);struct ae_direct_token before;struct ae_document_history_info history,after;
        CHECK(ae_direct_token(input,&before) && ae_document_history_info(ae_direct_document(input),&history));
        struct editor_document_binding b=test_binding(&a);nx_text_document_state out;
        nx_text_document_request r=test_request(b,NX_TEXT_DOCUMENT_REPLACE_RANGE,0,4,value,sizeof(value));
        test_realloc_calls=0;test_realloc_fail_at=failure<2?failure+1u:0;test_malloc_fail=failure==2;
        nx_result result=editor_document_apply(&b,&r,&out);test_realloc_fail_at=0;test_malloc_fail=false;
        CHECK(result==NX_ERROR_MEMORY && test_realloc_calls>0 && !test_text(&a,"seed",4));
        struct ae_direct_token now;CHECK(ae_direct_token(input,&now) && editor_document_same(before,now));
        CHECK(ae_document_history_info(ae_direct_document(input),&after) && history.undo==after.undo && history.redo==after.redo && history.bytes==after.bytes);
        CHECK(editor_document_apply(&b,&r,&out)==NX_OK && out.value.length==sizeof(value) && !memcmp(out.value.data,value,sizeof(value)));
        bool changed=false;CHECK(ae_direct_history(input,-1,&changed)==AE_DOCUMENT_OK && changed && !test_text(&a,"seed",4));
        test_destroy(&a);
    }
    return 0;
}
static int test_tabs_and_limits(void) {
    struct app a;CHECK(!test_create(&a,"seed",4) && !test_frame(&a));
    struct ids *ids=tab_ids(&a,a.document);uint64_t original=a.document;
    CHECK(nx_semantics_request_text_value(a.context,ids->canvas,(nx_utf8_span){"foreign",7})==NX_OK);
    uint64_t second=0;CHECK(ae_editor_workspace_new(a.workspace,&second)==AE_EDITOR_WORKSPACE_OK && second!=original && !test_frame(&a));
    CHECK(!test_text(&a,"",0) && ae_editor_workspace_select(a.workspace,original)==AE_EDITOR_WORKSPACE_OK && !test_frame(&a) && !test_text(&a,"seed",4));
    struct editor_document_binding b=test_binding(&a);nx_text_document_state out;
    nx_text_document_request r=test_request(b,NX_TEXT_DOCUMENT_REPLACE_RANGE,0,4,"\xc3",1);
    CHECK(editor_document_apply(&b,&r,&out)==NX_ERROR_ARGUMENT && !test_text(&a,"seed",4));
    a.native_document.serial=UINT64_MAX;b=test_binding(&a);r=test_request(b,NX_TEXT_DOCUMENT_REPLACE_RANGE,0,4,"last",4);
    CHECK(editor_document_apply(&b,&r,&out)==NX_ERROR_LIMIT && !test_text(&a,"seed",4));
    test_destroy(&a);return 0;
}
#ifdef EDITOR_DOCUMENT_WITH_PRESENTATION
static int test_painted_frame(struct app *a) {
    for(unsigned i=0;i<a->layout_count;++i)nx_text_layout_destroy(a->layouts[i]);
    a->layout_count=0;
    nx_semantic_desc root={.id=1,.role=NX_ROLE_WINDOW,.bounds={0,0,300,200},.name={"Editor",6}};
    CHECK(sync_document(a) && nx_begin_frame(a->context,300,200,0)==NX_OK && nx_semantics_add(a->context,&root,NULL)==NX_OK &&
        paint_document(a) && nx_end_frame(a->context)==NX_OK && a->native_document.presentation_available);
    CHECK(a->native_document.presentation.count);
    for(size_t i=0;i<a->native_document.presentation.count;++i) {
        nx_text_document_box *b=&a->native_document.presentation.boxes[i];
        CHECK(b->start_byte<=b->end_byte && b->end_byte<=a->byte_size && b->bounds.x>=a->canvas.x &&
            b->bounds.y>=a->canvas.y && b->bounds.x+b->bounds.width<=a->canvas.x+a->canvas.width+.002f);
    }
    return 0;
}
static int test_original_paint(const char *path,bool require_ligature) {
    FILE *file=fopen(path,"rb");CHECK(file && !fseek(file,0,SEEK_END));long size=ftell(file);CHECK(size>0 && size<20000000 && !fseek(file,0,SEEK_SET));
    uint8_t *bytes=malloc((size_t)size);CHECK(bytes && fread(bytes,1,(size_t)size,file)==(size_t)size && !fclose(file));
    const char text[]="Wii office a\xcc\x81 \xd7\x90\xd7\x91\xd7\x92\r\nsecond\nthird";struct app a;
    CHECK(!test_create(&a,text,sizeof(text)-1) && nx_font_open(bytes,(size_t)size,NULL,&a.font)==NX_OK && nx_font_collection_create(NULL,&a.fonts)==NX_OK && nx_font_collection_add(a.fonts,a.font)==NX_OK);
    a.mono_fonts=a.fonts;a.em=20;a.scale=1;a.line_height=31;a.focused=true;a.focus_document=true;
    CHECK(!test_painted_frame(&a) && a.row_count==3);
    if(require_ligature) {
        bool found=false;
        for(size_t i=0;i<a.native_document.presentation.count;++i) {
            nx_text_document_box *b=&a.native_document.presentation.boxes[i];
            if(b->start_byte==5 && b->end_byte==8)found=true; /* Actual office ffi. */
        }
        CHECK(found);
    }
    uint64_t serial=a.native_document.serial;struct ids *ids=tab_ids(&a,a.document);
    CHECK(nx_semantics_request_text_value(a.context,ids->canvas,(nx_utf8_span){"changed\r\nnew",12})==NX_OK && !test_painted_frame(&a));
    CHECK(a.native_document.changed && a.native_document.serial>serial && !test_text(&a,"changed\r\nnew",12) && a.row_count==2);
    a.canvas.height=31;a.top=1;a.ensure_caret=false;CHECK(!test_painted_frame(&a) && a.row_count==1 && a.rows[0].start==9);
    for(size_t i=0;i<a.native_document.presentation.count;++i)CHECK(a.native_document.presentation.boxes[i].start_byte>=9);
    a.em=28;a.scale=2;a.line_height=43.4f;a.canvas.height=80;CHECK(!test_painted_frame(&a));
    for(unsigned i=0;i<a.layout_count;++i)nx_text_layout_destroy(a.layouts[i]);
    nx_font_collection_destroy(a.fonts);nx_font_close(a.font);test_destroy(&a);free(bytes);
    puts("Original Editor actual paint: real layout/rows/carets, accepted edit revision, CRLF, viewport omission and font scale passed.");return 0;
}
#endif
int main(int argc,char **argv) {
    CHECK(!test_public_frame() && !test_policy_and_review() && !test_stale_tokens() && !test_allocation_failures() && !test_tabs_and_limits());
#ifdef EDITOR_DOCUMENT_WITH_PRESENTATION
    if(argc==2 || argc==3) {
        CHECK(argc==2 || !strcmp(argv[2],"--require-ligature"));
        CHECK(!test_original_paint(argv[1],argc==3));
    }
    else CHECK(argc==1);
#else
    (void)argc;(void)argv;
#endif
    puts("Original Editor native document owner: installed SDK application, original history/selection, policy/review/tab tokens and UI/model allocation-before-mutation passed.");return 0;
}
