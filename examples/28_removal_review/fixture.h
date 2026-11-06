#ifndef NEXAUI_EXAMPLE_REMOVAL_FIXTURE_H
#define NEXAUI_EXAMPLE_REMOVAL_FIXTURE_H
#include "review.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Declared test inventory only. This does not inspect or remove host objects. */
static removal_plan removal_fixture(removal_entry storage[8]){
    removal_plan plan={0};plan.plan[15]=1;plan.installation[15]=2;plan.scope[15]=3;
    plan.revision=42;plan.inventory_complete=1;plan.count=8;plan.entries=storage;
    strcpy(plan.component,"Example application");strcpy(plan.scope_label,"Selected user's managed installation");
    const char *labels[]={"Application code","Private settings and cache","Credential registration","Owned background service",
        "Shared support library","Profile deliberately kept","Independent project document","Backup outside this action"};
    const char *reasons[]={"Owned by this installation","Managed private state in the selected user scope",
        "Revoke the app-owned credential registration","Stop and unregister only this app's background work",
        "Another installed application still requires this component","The user explicitly chose to retain this profile",
        "User-created document outside automatic app-state removal","This request does not remove backup or remote copies"};
    const removal_disposition kinds[]={REMOVAL_REMOVE,REMOVAL_REMOVE,REMOVAL_REMOVE,REMOVAL_REMOVE,
        REMOVAL_SHARED,REMOVAL_KEEP,REMOVAL_DOCUMENT,REMOVAL_OTHER_COPY};
    for(size_t i=0;i<plan.count;++i){
        removal_entry *entry=&plan.entries[i];*entry=(removal_entry){0};entry->semantic_id=1000+i;entry->object[15]=(unsigned char)(10+i);
        entry->disposition=kinds[i];entry->logical_bytes=4096*(i+1);
        strcpy(entry->label,labels[i]);strcpy(entry->reason,reasons[i]);
    }
    return plan;
}
static inline removal_entry *removal_large_fixture(size_t count){
    if(!count || count>REMOVAL_MAX_ENTRIES)return NULL;
    removal_entry seed[8];(void)removal_fixture(seed);
    removal_entry *entries=calloc(count,sizeof(*entries));if(!entries)return NULL;
    const unsigned representative[]={0,4,5,6,7};
    for(size_t i=0;i<count;++i){
        entries[i]=seed[representative[i%5]];entries[i].semantic_id=1000+i;uint64_t identity=(uint64_t)i+1;
        memset(entries[i].object,0,16);
        for(unsigned byte=0;byte<8;++byte)entries[i].object[15-byte]=(unsigned char)(identity>>(8*byte));
        entries[i].disposition=(removal_disposition)(i%5);
        snprintf(entries[i].label,sizeof(entries[i].label),"managed-object-%06zu",i);
    }
    return entries;
}
#endif
