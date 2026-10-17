#include "ot_layout.h"
#include <stdlib.h>
#include <string.h>

#define NX_GC_SEQUENCE 64u
#define NX_GC_DEPTH 32u
#define NX_GC_SUBTABLES 64u

typedef struct nx_gc_lookup {
    size_t offset, *sub;
    uint16_t type,flags,set,count;
    unsigned status; /* 0 new, 1 validating, 2 ready, 3 unsupported. */
} nx_gc_lookup;
typedef struct nx_gc_node { size_t next,previous; } nx_gc_node;
typedef struct nx_gc {
    nx_ot_view v;
    size_t lookups,lookup_count,capacity,*work;
    uint16_t limit;
    nx_gc_lookup *cache;
    nx_gc_node *nodes;
    nx_shaped_glyph *glyphs;
    nx_ot_glyph_state *state;
    const nx_ot_gdef *gdef;
} nx_gc;
typedef struct nx_gc_pattern {
    size_t base,back,input,ahead,records;
    size_t classes[3];
    uint16_t back_count,input_count,ahead_count,record_count,mode;
} nx_gc_pattern;

static nx_result nx_gc_prepare(nx_gc *g,size_t index,unsigned depth);
static nx_result nx_gc_apply_at(nx_gc *g,size_t index,size_t at,unsigned depth,int *matched,size_t *next);

static nx_result nx_gc_sequence(nx_gc *g,size_t *cursor,size_t *array,uint16_t *count,int first) {
    if (!nx_ot_has(g->v,*cursor,2)) return NX_ERROR_ARGUMENT;
    *count=nx_ot_u16(g->v.data+*cursor); *cursor+=2;
    if (*count>NX_GC_SEQUENCE) return NX_ERROR_LIMIT;
    if (first && !*count) return NX_ERROR_ARGUMENT;
    size_t n=*count-(first?1u:0u);
    if (!nx_ot_array(g->v,*cursor,0,n,2)) return NX_ERROR_ARGUMENT;
    *array=*cursor; *cursor+=2*n; return nx_ot_charge(g->work,n+1u);
}
/* Formats 1/2 omit the first input glyph, which is selected by coverage/class.
   Format 3 supplies a coverage for every position, including the first. */
static nx_result nx_gc_pattern_read(nx_gc *g,size_t base,size_t rule,unsigned type,unsigned mode,
    const size_t classes[3],nx_gc_pattern *p) {
    memset(p,0,sizeof(*p)); p->base=base; p->mode=(uint16_t)mode;
    if (classes) memcpy(p->classes,classes,sizeof(p->classes));
    size_t cursor=rule; nx_result r;
    if (type==6) {
        r=nx_gc_sequence(g,&cursor,&p->back,&p->back_count,0); if (r!=NX_OK) return r;
    }
    if (type==5) {
        if (!nx_ot_has(g->v,cursor,4)) return NX_ERROR_ARGUMENT;
        p->input_count=nx_ot_u16(g->v.data+cursor);
        p->record_count=nx_ot_u16(g->v.data+cursor+2); cursor+=4;
        if (!p->input_count) return NX_ERROR_ARGUMENT;
        if (p->input_count>NX_GC_SEQUENCE || p->record_count>NX_GC_SEQUENCE) return NX_ERROR_LIMIT;
        size_t n=p->input_count-(mode==3?0u:1u);
        if (!nx_ot_array(g->v,cursor,0,n,2)) return NX_ERROR_ARGUMENT;
        p->input=cursor; cursor+=2*n;
        r=nx_ot_charge(g->work,n+1u); if (r!=NX_OK) return r;
    } else {
        r=nx_gc_sequence(g,&cursor,&p->input,&p->input_count,mode!=3); if (r!=NX_OK) return r;
        if (!p->input_count) return NX_ERROR_ARGUMENT;
        r=nx_gc_sequence(g,&cursor,&p->ahead,&p->ahead_count,0); if (r!=NX_OK) return r;
        if (!nx_ot_has(g->v,cursor,2)) return NX_ERROR_ARGUMENT;
        p->record_count=nx_ot_u16(g->v.data+cursor); cursor+=2;
        if (p->record_count>NX_GC_SEQUENCE) return NX_ERROR_LIMIT;
    }
    if (!nx_ot_array(g->v,cursor,0,p->record_count,4)) return NX_ERROR_ARGUMENT;
    p->records=cursor; return NX_OK;
}
static nx_result nx_gc_pattern_validate(nx_gc *g,const nx_gc_pattern *p,unsigned depth) {
    size_t arrays[3]={p->back,p->input,p->ahead};
    size_t counts[3]={p->back_count,p->input_count-(p->mode==3?0u:1u),p->ahead_count};
    for (unsigned kind=0;kind<3;++kind) {
        if (p->mode==2 && counts[kind] && !p->classes[kind]) return NX_ERROR_ARGUMENT;
        for (size_t i=0;i<counts[kind];++i) {
            if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
            uint16_t value=nx_ot_u16(g->v.data+arrays[kind]+2*i);
            if (p->mode==1 && value>=g->limit) return NX_ERROR_ARGUMENT;
            if (p->mode==3) {
                size_t at,n;
                if (!nx_ot_child(g->v,p->base,value,4,&at)) return NX_ERROR_ARGUMENT;
                nx_result r=nx_ot_coverage_validate(g->v,at,g->limit,&n,g->work); if (r!=NX_OK) return r;
            }
        }
    }
    for (size_t i=0;i<p->record_count;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        const uint8_t *r=g->v.data+p->records+4*i;
        if (nx_ot_u16(r)>=p->input_count || nx_ot_u16(r+2)>=g->lookup_count) return NX_ERROR_ARGUMENT;
        nx_result result=nx_gc_prepare(g,nx_ot_u16(r+2),depth+1); if (result!=NX_OK) return result;
    }
    return NX_OK;
}
static nx_result nx_gc_context_validate(nx_gc *g,size_t sub,unsigned type,unsigned depth) {
    unsigned format=nx_ot_u16(g->v.data+sub);
    if (format==3) {
        nx_gc_pattern p; nx_result r=nx_gc_pattern_read(g,sub,sub+2,type,3,NULL,&p);
        return r==NX_OK?nx_gc_pattern_validate(g,&p,depth):r;
    }
    if (format!=1 && format!=2) return NX_ERROR_UNSUPPORTED;
    size_t coverage,covered;
    if (!nx_ot_child(g->v,sub,nx_ot_u16(g->v.data+sub+2),4,&coverage)) return NX_ERROR_ARGUMENT;
    nx_result r=nx_ot_coverage_validate(g->v,coverage,g->limit,&covered,g->work); if (r!=NX_OK) return r;
    size_t header=format==1?6u:type==5?8u:12u,classes[3]={0};
    if (!nx_ot_has(g->v,sub,header)) return NX_ERROR_ARGUMENT;
    size_t sets=nx_ot_u16(g->v.data+sub+header-2);
    if (sets>4096) return NX_ERROR_LIMIT;
    if (!nx_ot_array(g->v,sub,header,sets,2)) return NX_ERROR_ARGUMENT;
    if (format==2) {
        for (unsigned k=0;k<3;++k) {
            if (type==5 && k!=1) continue;
            size_t offset=nx_ot_u16(g->v.data+sub+(type==5?4u:4u+2*k));
            if (!offset) { if (k==1) return NX_ERROR_ARGUMENT; continue; }
            if (!nx_ot_child(g->v,sub,offset,4,&classes[k])) return NX_ERROR_ARGUMENT;
            r=nx_ot_class_validate(g->v,classes[k],g->limit,g->work); if (r!=NX_OK) return r;
        }
    }
    /* The standard ignores excess coverage entries or rule sets. Validate all
       stored rules nevertheless, without indexing either array by the other. */
    for (size_t i=0;i<sets;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t rel=nx_ot_u16(g->v.data+sub+header+2*i),set;
        if (!rel) continue;
        if (!nx_ot_child(g->v,sub,rel,2,&set)) return NX_ERROR_ARGUMENT;
        size_t rules=nx_ot_u16(g->v.data+set);
        if (rules>4096) return NX_ERROR_LIMIT;
        if (!nx_ot_array(g->v,set,2,rules,2)) return NX_ERROR_ARGUMENT;
        for (size_t j=0;j<rules;++j) {
            if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
            size_t rule;
            if (!nx_ot_child(g->v,set,nx_ot_u16(g->v.data+set+2+2*j),2,&rule)) return NX_ERROR_ARGUMENT;
            nx_gc_pattern p; r=nx_gc_pattern_read(g,sub,rule,type,format,classes,&p);
            if (r==NX_OK) r=nx_gc_pattern_validate(g,&p,depth);
            if (r!=NX_OK) return r;
        }
    }
    return NX_OK;
}
static nx_result nx_gc_basic_validate(nx_gc *g,size_t sub,unsigned type) {
    if (!nx_ot_has(g->v,sub,6)) return NX_ERROR_ARGUMENT;
    unsigned format=nx_ot_u16(g->v.data+sub); size_t coverage,n;
    if (!nx_ot_child(g->v,sub,nx_ot_u16(g->v.data+sub+2),4,&coverage)) return NX_ERROR_ARGUMENT;
    nx_result r=nx_ot_coverage_validate(g->v,coverage,g->limit,&n,g->work); if (r!=NX_OK) return r;
    if (type==1 && format==1) return NX_OK;
    if (type==1 && format==2) {
        size_t count=nx_ot_u16(g->v.data+sub+4);
        if (count!=n || !nx_ot_array(g->v,sub,6,count,2)) return NX_ERROR_ARGUMENT;
        if (nx_ot_charge(g->work,count)!=NX_OK) return NX_ERROR_LIMIT;
        for (size_t i=0;i<count;++i) if (nx_ot_u16(g->v.data+sub+6+2*i)>=g->limit) return NX_ERROR_ARGUMENT;
        return NX_OK;
    }
    if (type!=4 || format!=1) return NX_ERROR_UNSUPPORTED;
    size_t sets=nx_ot_u16(g->v.data+sub+4);
    if (sets!=n || !nx_ot_array(g->v,sub,6,sets,2)) return NX_ERROR_ARGUMENT;
    for (size_t i=0;i<sets;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t set;
        if (!nx_ot_child(g->v,sub,nx_ot_u16(g->v.data+sub+6+2*i),2,&set)) return NX_ERROR_ARGUMENT;
        size_t count=nx_ot_u16(g->v.data+set);
        if (!nx_ot_array(g->v,set,2,count,2)) return NX_ERROR_ARGUMENT;
        for (size_t j=0;j<count;++j) {
            if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
            size_t lig;
            if (!nx_ot_child(g->v,set,nx_ot_u16(g->v.data+set+2+2*j),4,&lig)) return NX_ERROR_ARGUMENT;
            size_t components=nx_ot_u16(g->v.data+lig+2);
            if (!components) return NX_ERROR_ARGUMENT;
            if (components>32) return NX_ERROR_LIMIT;
            if (nx_ot_u16(g->v.data+lig)>=g->limit || !nx_ot_array(g->v,lig,4,components-1,2)) return NX_ERROR_ARGUMENT;
            if (nx_ot_charge(g->work,components)!=NX_OK) return NX_ERROR_LIMIT;
            for (size_t k=1;k<components;++k) if (nx_ot_u16(g->v.data+lig+2+2*k)>=g->limit) return NX_ERROR_ARGUMENT;
        }
    }
    return NX_OK;
}
static nx_result nx_gc_prepare(nx_gc *g,size_t index,unsigned depth) {
    nx_gc_lookup *l=&g->cache[index];
    if (l->status==1 || l->status==2) return NX_OK;
    if (l->status==3) return NX_ERROR_UNSUPPORTED;
    if (depth>=NX_GC_DEPTH || nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
    if (!nx_ot_child(g->v,g->lookups,nx_ot_u16(g->v.data+g->lookups+2+2*index),6,&l->offset)) return NX_ERROR_ARGUMENT;
    const uint8_t *p=g->v.data+l->offset;
    unsigned type=nx_ot_u16(p); l->type=(uint16_t)type; l->flags=nx_ot_u16(p+2); l->count=nx_ot_u16(p+4);
    if (l->count>NX_GC_SUBTABLES) return NX_ERROR_LIMIT;
    if (!nx_ot_has(g->v,l->offset,6u+2u*l->count+((l->flags&16u)?2u:0u))) return NX_ERROR_ARGUMENT;
    l->set=(l->flags&16u)?nx_ot_u16(p+6+2*l->count):0;
    nx_result r=nx_ot_filter_validate(g->gdef,l->flags,l->set); if (r!=NX_OK) return r;
    if (type!=1 && type!=4 && type!=5 && type!=6 && type!=7) { l->status=3; return NX_ERROR_UNSUPPORTED; }
    l->sub=l->count?malloc((size_t)l->count*sizeof(*l->sub)):NULL;
    if (l->count && !l->sub) return NX_ERROR_MEMORY;
    l->status=1;
    for (size_t i=0;i<l->count;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t at;
        if (!nx_ot_child(g->v,l->offset,nx_ot_u16(p+6+2*i),6,&at)) return NX_ERROR_ARGUMENT;
        if (type==7) {
            if (!nx_ot_has(g->v,at,8)) return NX_ERROR_ARGUMENT;
            if (nx_ot_u16(g->v.data+at)!=1) { l->status=3; return NX_ERROR_UNSUPPORTED; }
            unsigned target=nx_ot_u16(g->v.data+at+2);
            if (target==7 || (i && target!=l->type)) return NX_ERROR_ARGUMENT;
            l->type=(uint16_t)target;
            if (!nx_ot_child(g->v,at,nx_ot_u32(g->v.data+at+4),6,&at)) return NX_ERROR_ARGUMENT;
        }
        l->sub[i]=at;
        if (l->type==5 || l->type==6) r=nx_gc_context_validate(g,at,l->type,depth);
        else if (l->type==1 || l->type==4) r=nx_gc_basic_validate(g,at,l->type);
        else r=NX_ERROR_UNSUPPORTED;
        if (r!=NX_OK) { if (r==NX_ERROR_UNSUPPORTED) l->status=3; return r; }
    }
    l->status=2; return NX_OK;
}

static nx_result nx_gc_step(nx_gc *g,const nx_gc_lookup *l,size_t from,int backward,size_t *out) {
    size_t node=backward?g->nodes[from].previous:g->nodes[from].next;
    while (node!=SIZE_MAX) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        if (!nx_ot_ignored(g->gdef,l->flags,l->set,g->glyphs[node].glyph_index)) break;
        node=backward?g->nodes[node].previous:g->nodes[node].next;
    }
    *out=node; return NX_OK;
}
static int nx_gc_equal(nx_gc *g,const nx_gc_pattern *p,unsigned kind,size_t element,size_t node) {
    size_t array=kind==0?p->back:kind==1?p->input:p->ahead;
    uint16_t value=nx_ot_u16(g->v.data+array+2*element),glyph=g->glyphs[node].glyph_index;
    if (p->mode==1) return value==glyph;
    if (p->mode==2) return value==nx_ot_class_at(g->v,p->classes[kind],glyph);
    return nx_ot_coverage_index(g->v,p->base+value,glyph)!=SIZE_MAX;
}
static nx_result nx_gc_pattern_match(nx_gc *g,const nx_gc_lookup *l,const nx_gc_pattern *p,
    size_t at,size_t inputs[NX_GC_SEQUENCE],int *matched) {
    *matched=0; inputs[0]=at; size_t node=at;
    if (p->mode==3 && !nx_gc_equal(g,p,1,0,node)) return NX_OK;
    for (size_t i=1;i<p->input_count;++i) {
        nx_result r=nx_gc_step(g,l,node,0,&node); if (r!=NX_OK) return r;
        if (node==SIZE_MAX || !nx_gc_equal(g,p,1,i-(p->mode==3?0u:1u),node)) return NX_OK;
        inputs[i]=node;
    }
    size_t tail=node; node=at;
    for (size_t i=0;i<p->back_count;++i) {
        nx_result r=nx_gc_step(g,l,node,1,&node); if (r!=NX_OK) return r;
        if (node==SIZE_MAX || !nx_gc_equal(g,p,0,i,node)) return NX_OK;
    }
    node=tail;
    for (size_t i=0;i<p->ahead_count;++i) {
        nx_result r=nx_gc_step(g,l,node,0,&node); if (r!=NX_OK) return r;
        if (node==SIZE_MAX || !nx_gc_equal(g,p,2,i,node)) return NX_OK;
    }
    *matched=1; return NX_OK;
}
/* Deleted nodes retain a forward link. Context fences can therefore follow a
   removed end/lookahead node without scanning or moving the complete suffix. */
static void nx_gc_remove(nx_gc *g,size_t node) {
    size_t before=g->nodes[node].previous,after=g->nodes[node].next;
    if (before!=SIZE_MAX) g->nodes[before].next=after;
    if (after!=SIZE_MAX) g->nodes[after].previous=before;
    g->nodes[node].previous=g->capacity;
}
static nx_result nx_gc_actions(nx_gc *g,const nx_gc_pattern *p,size_t *inputs,unsigned depth,size_t *next) {
    size_t live=p->input_count,fence=g->nodes[inputs[live-1]].next;
    for (size_t i=0;i<p->record_count;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        const uint8_t *r=g->v.data+p->records+4*i;
        size_t position=nx_ot_u16(r);
        /* Sequence positions refer to the input sequence after prior actions;
           removed components no longer occupy a sequence slot. */
        if (position>=live) continue;
        int matched; size_t ignored;
        nx_result result=nx_gc_apply_at(g,nx_ot_u16(r+2),inputs[position],depth+1,&matched,&ignored);
        if (result!=NX_OK) return result;
        if (nx_ot_charge(g->work,live)!=NX_OK) return NX_ERROR_LIMIT;
        size_t write=0;
        for (size_t j=0;j<live;++j) if (g->nodes[inputs[j]].previous!=g->capacity) inputs[write++]=inputs[j];
        live=write;
    }
    while (fence!=SIZE_MAX && g->nodes[fence].previous==g->capacity) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        fence=g->nodes[fence].next;
    }
    *next=fence; return NX_OK;
}
static nx_result nx_gc_context_at(nx_gc *g,const nx_gc_lookup *l,size_t sub,size_t at,unsigned depth,int *matched,size_t *next) {
    unsigned format=nx_ot_u16(g->v.data+sub);
    if (format==3) {
        nx_gc_pattern p; nx_result r=nx_gc_pattern_read(g,sub,sub+2,l->type,3,NULL,&p);
        size_t inputs[NX_GC_SEQUENCE];
        if (r==NX_OK) r=nx_gc_pattern_match(g,l,&p,at,inputs,matched);
        return r==NX_OK && *matched?nx_gc_actions(g,&p,inputs,depth,next):r;
    }
    size_t coverage=sub+nx_ot_u16(g->v.data+sub+2);
    size_t selected=nx_ot_coverage_index(g->v,coverage,g->glyphs[at].glyph_index);
    if (selected==SIZE_MAX) return NX_OK;
    size_t header=format==1?6u:l->type==5?8u:12u,classes[3]={0};
    if (format==2) {
        if (l->type==5) classes[1]=sub+nx_ot_u16(g->v.data+sub+4);
        else for (unsigned i=0;i<3;++i) { size_t rel=nx_ot_u16(g->v.data+sub+4+2*i); classes[i]=rel?sub+rel:0; }
        selected=nx_ot_class_at(g->v,classes[1],g->glyphs[at].glyph_index);
    }
    size_t sets=nx_ot_u16(g->v.data+sub+header-2);
    if (selected>=sets) return NX_OK;
    size_t relative=nx_ot_u16(g->v.data+sub+header+2*selected);
    if (!relative) return NX_OK;
    size_t set=sub+relative,rules=nx_ot_u16(g->v.data+set);
    for (size_t i=0;i<rules;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t rule=set+nx_ot_u16(g->v.data+set+2+2*i),inputs[NX_GC_SEQUENCE];
        nx_gc_pattern p; nx_result r=nx_gc_pattern_read(g,sub,rule,l->type,format,classes,&p);
        if (r==NX_OK) r=nx_gc_pattern_match(g,l,&p,at,inputs,matched);
        if (r!=NX_OK) return r;
        if (*matched) return nx_gc_actions(g,&p,inputs,depth,next);
    }
    return NX_OK;
}
static nx_result nx_gc_ligature(nx_gc *g,const nx_gc_lookup *l,size_t sub,size_t at,size_t covered,int *matched,size_t *next) {
    size_t set=sub+nx_ot_u16(g->v.data+sub+6+2*covered),count=nx_ot_u16(g->v.data+set);
    for (size_t i=0;i<count;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t lig=set+nx_ot_u16(g->v.data+set+2+2*i),n=nx_ot_u16(g->v.data+lig+2),ids[32]; ids[0]=at;
        size_t node=at,k=1;
        for (;k<n;++k) {
            nx_result r=nx_gc_step(g,l,node,0,&node); if (r!=NX_OK) return r;
            if (node==SIZE_MAX || g->glyphs[node].glyph_index!=nx_ot_u16(g->v.data+lig+2+2*k)) break;
            ids[k]=node;
        }
        if (k!=n) continue;
        *matched=1;
        if (n==1) { g->glyphs[at].glyph_index=nx_ot_u16(g->v.data+lig); return NX_OK; }
        nx_ot_glyph_state old[32]; uint32_t starts[32],total=0;
        for (k=0;k<n;++k) {
            old[k]=g->state[ids[k]]; starts[k]=total;
            if (old[k].components>UINT32_MAX-total) return NX_ERROR_LIMIT;
            total+=old[k].components;
        }
        size_t end=ids[n-1],after=g->nodes[end].next,id=old[0].origin,cluster=g->glyphs[at].cluster_byte_start;
        g->glyphs[at].glyph_index=nx_ot_u16(g->v.data+lig); g->glyphs[at].byte_end=g->glyphs[end].byte_end;
        g->state[at].ligature_id=id; g->state[at].components=total; g->state[at].component=0; g->state[at].ligated=1;
        size_t component=0;
        for (node=g->nodes[at].next;node!=after;) {
            if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
            size_t following=g->nodes[node].next;
            if (component+1<n && node==ids[component+1]) { ++component; nx_gc_remove(g,node); }
            else {
                nx_ot_glyph_state *m=&g->state[node];
                if (nx_ot_glyph_class(g->gdef,&g->glyphs[node],m)==3) {
                    uint32_t local=old[component].components;
                    if (m->ligature_id && m->ligature_id==old[component].ligature_id && m->component)
                        local=m->component<local?m->component:local;
                    m->ligature_id=id; m->component=starts[component]+local;
                }
                g->glyphs[node].cluster_byte_start=cluster;
            }
            node=following;
        }
        for (node=after;node!=SIZE_MAX && nx_ot_glyph_class(g->gdef,&g->glyphs[node],&g->state[node])==3;node=g->nodes[node].next) {
            if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
            nx_ot_glyph_state *m=&g->state[node]; uint32_t local=old[n-1].components;
            if (m->ligature_id && m->ligature_id==old[n-1].ligature_id && m->component) local=m->component<local?m->component:local;
            m->ligature_id=id; m->component=starts[n-1]+local; g->glyphs[node].cluster_byte_start=cluster;
        }
        *next=after; return NX_OK;
    }
    return NX_OK;
}
static nx_result nx_gc_apply_at(nx_gc *g,size_t index,size_t at,unsigned depth,int *matched,size_t *next) {
    *matched=0; *next=g->nodes[at].next;
    if (depth>=NX_GC_DEPTH || nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
    nx_gc_lookup *l=&g->cache[index];
    if (l->status!=2) return NX_ERROR_UNSUPPORTED;
    for (size_t i=0;i<l->count && !*matched;++i) {
        if (nx_ot_charge(g->work,1)!=NX_OK) return NX_ERROR_LIMIT;
        size_t sub=l->sub[i];
        if (l->type==5 || l->type==6) {
            nx_result r=nx_gc_context_at(g,l,sub,at,depth,matched,next); if (r!=NX_OK) return r;
        } else {
            size_t coverage=sub+nx_ot_u16(g->v.data+sub+2);
            size_t entry=nx_ot_coverage_index(g->v,coverage,g->glyphs[at].glyph_index);
            if (entry==SIZE_MAX) continue;
            if (l->type==1) {
                uint16_t value=nx_ot_u16(g->v.data+sub)==1?
                    (uint16_t)(g->glyphs[at].glyph_index+(int16_t)nx_ot_u16(g->v.data+sub+4)):
                    nx_ot_u16(g->v.data+sub+6+2*entry);
                if (value>=g->limit) return NX_ERROR_ARGUMENT;
                g->glyphs[at].glyph_index=value; *matched=1;
            } else {
                nx_result r=nx_gc_ligature(g,l,sub,at,entry,matched,next); if (r!=NX_OK) return r;
            }
        }
    }
    return NX_OK;
}
nx_result nx_gsub_context_apply(nx_ot_view v,size_t lookups,size_t lookup_count,size_t root,
    uint16_t limit,nx_shaped_glyph *glyphs,nx_ot_glyph_state *state,size_t *count,const nx_ot_gdef *gdef,size_t *work) {
    if (root>=lookup_count || *count>SIZE_MAX/sizeof(nx_gc_node)) return NX_ERROR_LIMIT;
    nx_gc g={0}; g.v=v; g.lookups=lookups; g.lookup_count=lookup_count; g.limit=limit;
    g.capacity=*count; g.glyphs=glyphs; g.state=state; g.gdef=gdef; g.work=work;
    g.cache=calloc(lookup_count,sizeof(*g.cache));
    if (!g.cache) return NX_ERROR_MEMORY;
    nx_result r=nx_gc_prepare(&g,root,0);
    if (r!=NX_OK) goto done;
    g.nodes=*count?malloc(*count*sizeof(*g.nodes)):NULL;
    if (*count && !g.nodes) { r=NX_ERROR_MEMORY; goto done; }
    if (nx_ot_charge(work,*count)!=NX_OK) { r=NX_ERROR_LIMIT; goto done; }
    for (size_t i=0;i<*count;++i) g.nodes[i]=(nx_gc_node){i+1<*count?i+1:SIZE_MAX,i?i-1:SIZE_MAX};
    for (size_t at=*count?0:SIZE_MAX;at!=SIZE_MAX;) {
        size_t next; int matched;
        r=nx_gc_apply_at(&g,root,at,0,&matched,&next); if (r!=NX_OK) goto done;
        at=next;
    }
    {
        size_t write=0;
        for (size_t at=*count?0:SIZE_MAX;at!=SIZE_MAX;at=g.nodes[at].next) {
            glyphs[write]=glyphs[at]; state[write++]=state[at];
        }
        *count=write;
    }
done:
    for (size_t i=0;i<lookup_count;++i) free(g.cache[i].sub);
    free(g.cache); free(g.nodes); return r;
}
