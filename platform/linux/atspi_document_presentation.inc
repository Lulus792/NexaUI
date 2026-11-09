/* Included after text byte/range helpers. No callback, context or application
   pointer is touched by these native queries. */
static int32_t nx_ax_document_character(const nx_ax_node *n,size_t byte) {
    if(n->text_bytes==(size_t)n->text_count)return (int32_t)byte;
    size_t at=0;int32_t count=0;
    while(at<byte) {
        size_t ascii=nx_ascii_prefix(n->value+at,byte-at);at+=ascii;count+=(int32_t)ascii;
        if(at<byte){uint32_t scalar;if(nx_utf8_decode(n->value,n->text_bytes,&at,&scalar)!=NX_OK)return -1;++count;}
    }
    return at==byte?count:-1;
}
static const char *nx_ax_document_attribute(const nx_document_presentation *p,nx_document_attributes a,const char *name) {
    for(size_t i=0;i<a.count;++i)if(!strcmp(nx_document_attribute_name(p,a,i),name))return nx_document_attribute_value(p,a,i);
    return NULL;
}
static int nx_ax_document_attributes(DBusMessageIter *it,const nx_document_presentation *p,
    nx_document_attributes run,int defaults) {
    DBusMessageIter array,entry;
    if(!d.message_iter_open_container(it,DBUS_TYPE_ARRAY,"{ss}",&array))return 0;
    for(unsigned pass=0;pass<2;++pass) {
        nx_document_attributes a=pass?run:p->defaults;
        if(!pass && !defaults)continue;
        for(size_t i=0;i<a.count;++i) {
            const char *name=nx_document_attribute_name(p,a,i);
            if(!pass && nx_ax_document_attribute(p,run,name))continue;
            if(!d.message_iter_open_container(&array,DBUS_TYPE_DICT_ENTRY,NULL,&entry) ||
               !nx_ax_string(&entry,name) || !nx_ax_string(&entry,nx_document_attribute_value(p,a,i)) ||
               !d.message_iter_close_container(&array,&entry))return 0;
        }
    }
    return d.message_iter_close_container(it,&array);
}
static int nx_ax_document_presentation_call(nx_ax_ref r,DBusMessage *call,const char *member,DBusMessageIter *it) {
    nx_ax_node *n=nx_ax_node_at(r);const nx_document_presentation *p=n?n->presentation:NULL;
    if(!p)return -5;
    if(!strcmp(member,"GetCharacterExtents") || !strcmp(member,"GetRangeExtents")) {
        dbus_int32_t first=0,last=0;dbus_uint32_t coordinates=0;size_t start=0,end=0;int character=!strcmp(member,"GetCharacterExtents");
        int valid=character?d.message_get_args(call,NULL,DBUS_TYPE_INT32,&first,DBUS_TYPE_UINT32,&coordinates,DBUS_TYPE_INVALID):
            d.message_get_args(call,NULL,DBUS_TYPE_INT32,&first,DBUS_TYPE_INT32,&last,DBUS_TYPE_UINT32,&coordinates,DBUS_TYPE_INVALID);
        if(!valid || first<0 || (character && first>=n->text_count) || !nx_ax_text_byte(n,first,&start))return -1;
        if(!character && (last<first || !nx_ax_text_byte(n,last,&end)))return -1;
        if(coordinates>2u)return -1;
        if(!nx_ax_showing(r))return -5;
        nx_rect bounds;nx_result result=character?nx_document_presentation_character(p,start,&bounds):nx_document_presentation_range(p,start,end,&bounds);
        if(result!=NX_OK)return result==NX_ERROR_ARGUMENT?-1:-5;
        int32_t v[4];int converted=nx_ax_rectangle(r,bounds,coordinates,v);if(converted<0)return converted;
        return nx_ax_int(it,v[0]) && nx_ax_int(it,v[1]) && nx_ax_int(it,v[2]) && nx_ax_int(it,v[3]);
    }
    if(!strcmp(member,"GetOffsetAtPoint")) {
        dbus_int32_t x,y;dbus_uint32_t coordinates;
        if(!d.message_get_args(call,NULL,DBUS_TYPE_INT32,&x,DBUS_TYPE_INT32,&y,DBUS_TYPE_UINT32,&coordinates,DBUS_TYPE_INVALID))return -1;
        int inside=nx_ax_point(r,x,y,coordinates);if(inside<0)return inside;
        if(!nx_ax_showing(r))return -5;
        if(!inside)return nx_ax_int(it,-1);
        const nx_text_document_box *boxes=(const nx_text_document_box *)(const void *)((const char *)p+p->box_offset);
        for(size_t i=0;i<p->box_count;++i) {
            if(boxes[i].start_byte==boxes[i].end_byte)continue;
            int32_t v[4];int converted=nx_ax_rectangle(r,boxes[i].bounds,coordinates,v);if(converted<0)return converted;
            if(x>=v[0] && y>=v[1] && (int64_t)x<(int64_t)v[0]+v[2] && (int64_t)y<(int64_t)v[1]+v[3])
                return nx_ax_int(it,nx_ax_document_character(n,boxes[i].start_byte));
        }
        return nx_ax_int(it,-1);
    }
    if(!strcmp(member,"GetDefaultAttributes")) {
        if(!d.message_has_signature(call,""))return -1;
        return nx_ax_document_attributes(it,p,(nx_document_attributes){0},1);
    }
    if(!strcmp(member,"GetAttributes") || !strcmp(member,"GetAttributeRun") || !strcmp(member,"GetAttributeValue")) {
        dbus_int32_t index;dbus_bool_t defaults=FALSE;const char *name=NULL;int valid;
        if(!strcmp(member,"GetAttributeRun"))valid=d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_BOOLEAN,&defaults,DBUS_TYPE_INVALID);
        else if(!strcmp(member,"GetAttributeValue"))valid=d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_STRING,&name,DBUS_TYPE_INVALID);
        else valid=d.message_get_args(call,NULL,DBUS_TYPE_INT32,&index,DBUS_TYPE_INVALID);
        size_t byte,start,end;
        if(!valid || index<0 || index>=n->text_count || !nx_ax_text_byte(n,index,&byte))return -1;
        nx_document_attributes run=nx_document_presentation_attributes(p,byte,&start,&end);
        if(name) {
            const char *value=nx_ax_document_attribute(p,run,name);
            if(!value)value=nx_ax_document_attribute(p,p->defaults,name);
            return nx_ax_string(it,value?value:"");
        }
        return nx_ax_document_attributes(it,p,run,defaults!=FALSE) &&
            nx_ax_int(it,nx_ax_document_character(n,start)) && nx_ax_int(it,nx_ax_document_character(n,end));
    }
    return -5;
}
static int nx_ax_document_attribute_sets_equal(const nx_document_presentation *a,nx_document_attributes aa,
    const nx_document_presentation *b,nx_document_attributes bb) {
    if(aa.count!=bb.count)return 0;
    for(size_t i=0;i<aa.count;++i) {
        const char *value=nx_ax_document_attribute(b,bb,nx_document_attribute_name(a,aa,i));
        if(!value || strcmp(value,nx_document_attribute_value(a,aa,i)))return 0;
    }
    return 1;
}
static int nx_ax_document_styles_equal(const nx_document_presentation *a,const nx_document_presentation *b) {
    if(!a || !b)return a==b;
    if(a->run_count!=b->run_count || !nx_ax_document_attribute_sets_equal(a,a->defaults,b,b->defaults))return 0;
    const nx_document_attribute_run *ar=(const nx_document_attribute_run *)(const void *)((const char *)a+a->run_offset);
    const nx_document_attribute_run *br=(const nx_document_attribute_run *)(const void *)((const char *)b+b->run_offset);
    for(size_t i=0;i<a->run_count;++i)if(ar[i].start!=br[i].start || ar[i].end!=br[i].end ||
        !nx_ax_document_attribute_sets_equal(a,ar[i].attributes,b,br[i].attributes))return 0;
    return 1;
}
static int nx_ax_document_geometry_equal(const nx_document_presentation *a,const nx_document_presentation *b) {
    if(!a || !b)return a==b;
    if(a->box_count!=b->box_count)return 0;
    const nx_text_document_box *aa=(const nx_text_document_box *)(const void *)((const char *)a+a->box_offset);
    const nx_text_document_box *bb=(const nx_text_document_box *)(const void *)((const char *)b+b->box_offset);
    for(size_t i=0;i<a->box_count;++i)if(aa[i].start_byte!=bb[i].start_byte || aa[i].end_byte!=bb[i].end_byte ||
        aa[i].bounds.x!=bb[i].bounds.x || aa[i].bounds.y!=bb[i].bounds.y ||
        aa[i].bounds.width!=bb[i].bounds.width || aa[i].bounds.height!=bb[i].bounds.height)return 0;
    return 1;
}
