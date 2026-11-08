/* Application-owned admission. The SDK supplies the normalized complete splice;
   use the original whole-draft rule before it touches text or history. */
static void native_reject(struct ae_files_view *);
static int paste_valid(nx_utf8_span current,size_t start,size_t end,nx_utf8_span insertion,void *data) {
    const struct paste_rule *rule=data;struct ae_files_view *v=rule->view;
    bool accepted=false;
    if(start<=end && end<=current.length && current.length-(end-start)<=AE_FILES_NAME &&
       insertion.length<=AE_FILES_NAME-(current.length-(end-start))) {
        char proposed[AE_FILES_NAME+1];size_t length=current.length-(end-start)+insertion.length;
        if(start)memcpy(proposed,current.data,start);
        if(insertion.length)memcpy(proposed+start,insertion.data,insertion.length);
        if(current.length>end)memcpy(proposed+start+insertion.length,current.data+end,current.length-end);
        proposed[length]=0;
        bool pending=v->native_reject_pending && v->native_reject_scope==v->base;
        struct field_validation validation={rule->name,pending || (rule->name?v->s->name_bad:v->s->query_bad)};
        accepted=field_valid((nx_utf8_span){proposed,length},&validation)!=0;
    }
    if(!accepted)native_reject(v);
    return accepted;
}
