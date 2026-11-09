#include "accessible_model.h"

static bool same(struct ae_direct_token a,struct ae_direct_token b) {
    return a.identity==b.identity && a.interaction==b.interaction && a.revision==b.revision &&
        a.cursor==b.cursor && a.anchor==b.anchor && a.selecting==b.selecting;
}
static bool current(const struct ae_direct *input,const struct ae_direct_token *token) {
    struct ae_direct_token now;
    return token && ae_direct_token(input,&now) && same(*token,now);
}
static unsigned char byte(const struct ae_document_slice *slice,size_t offset) {
    return (unsigned char)(offset<slice->first_size?slice->first[offset]:slice->second[offset-slice->first_size]);
}
/* Original document validity guarantees strict UTF-8. Scan the borrowed gap
   spans directly; neither a full-document copy nor a second text model exists. */
static bool positions(const struct ae_direct *input,const struct ae_direct_token *token,
    int64_t first,int64_t second,bool allow_end,size_t *a,size_t *b) {
    if(!current(input,token) || first<0 || second<(allow_end?-1:0))return false;
    struct ae_document_info info;struct ae_document_slice slice;
    const struct ae_document *document=ae_direct_document(input);
    if(!ae_document_info(document,&info) || !ae_document_slice(document,0,info.size,&slice))return false;
    bool got_a=false,got_b=second==-1;size_t count=0;
    size_t next_a=0,next_b=info.size;
    for(size_t offset=0;offset<info.size;++offset) {
        if((byte(&slice,offset)&0xc0u)==0x80u)continue;
        if((uint64_t)first==count) {next_a=offset;got_a=true;}
        if(second>=0 && (uint64_t)second==count) {next_b=offset;got_b=true;}
        ++count;
    }
    if((uint64_t)first==count) {next_a=info.size;got_a=true;}
    if(second>=0 && (uint64_t)second==count) {next_b=info.size;got_b=true;}
    if(!got_a || !got_b)return false;
    *a=next_a;*b=next_b;return true;
}
bool editor_accessible_snapshot(const struct ae_direct *input,struct editor_accessible_state *out) {
    if(!out)return false;
    struct editor_accessible_state state={0};struct ae_document_info info;struct ae_document_slice slice;
    const struct ae_document *document=ae_direct_document(input);
    if(!ae_direct_token(input,&state.token) || !ae_document_info(document,&info) ||
       !ae_document_slice(document,0,info.size,&slice))return false;
    size_t anchor=state.token.selecting?state.token.anchor:state.token.cursor;
    for(size_t offset=0;offset<info.size;++offset) {
        if((byte(&slice,offset)&0xc0u)==0x80u)continue;
        if(offset==anchor)state.anchor=state.characters;
        if(offset==state.token.cursor)state.cursor=state.characters;
        ++state.characters;
    }
    if(anchor==info.size)state.anchor=state.characters;
    if(state.token.cursor==info.size)state.cursor=state.characters;
    state.bytes=info.size;*out=state;return true;
}
enum ae_document_result editor_accessible_read(const struct ae_direct *input,
    const struct ae_direct_token *token,int64_t start,int64_t end,
    char *out,size_t capacity,size_t *length) {
    if(!length || (!out && capacity))return AE_DOCUMENT_INVALID;
    *length=0;size_t begin=0,finish=0;
    if(!positions(input,token,start,end,true,&begin,&finish) || begin>finish)return AE_DOCUMENT_INVALID;
    size_t required=finish-begin;*length=required;
    if(!out)return AE_DOCUMENT_OK;
    if(capacity<required)return AE_DOCUMENT_LIMIT;
    return ae_document_copy(ae_direct_document(input),begin,required,out,capacity)?AE_DOCUMENT_OK:AE_DOCUMENT_INVALID;
}
enum ae_document_result editor_accessible_replace(struct ae_direct *input,
    const struct ae_direct_token *token,int64_t start,int64_t end,const char *text,size_t length) {
    size_t begin=0,finish=0;
    if((!text && length) || !positions(input,token,start,end,true,&begin,&finish) || begin>finish)return AE_DOCUMENT_INVALID;
    return editor_direct_ime_replace(input,token,begin,finish,text,length);
}
bool editor_accessible_select(struct ae_direct *input,const struct ae_direct_token *token,
    int64_t anchor,int64_t cursor) {
    size_t begin=0,end=0;
    return positions(input,token,anchor,cursor,false,&begin,&end) &&
        editor_direct_accessible_select_bytes(input,token,begin,end);
}
