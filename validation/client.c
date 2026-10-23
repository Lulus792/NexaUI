/* Independent C17 OS AX process: no NexaUI or Objective-C linkage. */
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static unsigned children_events,rows_events;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1.e9;}
static void tick(void){CFRunLoopRunInMode(kCFRunLoopDefaultMode,.02,true);}
static void observed(AXObserverRef observer,AXUIElementRef element,CFStringRef notification,void *user){
    (void)observer;(void)element;(void)user;
    if(CFEqual(notification,CFSTR("AXSelectedChildrenChanged")))++children_events;
    if(CFEqual(notification,CFSTR("AXSelectedRowsChanged")))++rows_events;
}
static int identified(AXUIElementRef e,unsigned id){
    CFTypeRef v=NULL;char bytes[40];snprintf(bytes,sizeof(bytes),"nexa-%016llx",(unsigned long long)id);
    CFStringRef wanted=CFStringCreateWithCString(NULL,bytes,kCFStringEncodingUTF8);
    AXError error=AXUIElementCopyAttributeValue(e,CFSTR("AXIdentifier"),&v);
    int yes=error==kAXErrorSuccess && v && CFGetTypeID(v)==CFStringGetTypeID() && CFEqual(v,wanted);
    if(v)CFRelease(v);CFRelease(wanted);return yes;
}
static AXUIElementRef find(AXUIElementRef e,unsigned id,unsigned depth,unsigned *budget){
    if(depth>64 || !*budget)return NULL;--*budget;if(identified(e,id))return (AXUIElementRef)CFRetain(e);
    CFTypeRef value=NULL;AXUIElementRef result=NULL;
    if(AXUIElementCopyAttributeValue(e,kAXChildrenAttribute,&value)==kAXErrorSuccess && value && CFGetTypeID(value)==CFArrayGetTypeID())
        for(CFIndex i=0;i<CFArrayGetCount(value) && !result;++i){CFTypeRef c=CFArrayGetValueAtIndex(value,i);if(CFGetTypeID(c)==AXUIElementGetTypeID())result=find((AXUIElementRef)c,id,depth+1,budget);}
    if(value)CFRelease(value);return result;
}
static AXUIElementRef wait_find(AXUIElementRef e,unsigned id){
    double end=now()+5;AXUIElementRef result=NULL;
    do{unsigned budget=8192;result=find(e,id,0,&budget);if(!result)tick();}while(!result && now()<end);return result;
}
static int equals(AXUIElementRef e,CFStringRef attribute,const unsigned *ids,size_t count){
    CFTypeRef value=NULL;int okay=0;
    if(AXUIElementCopyAttributeValue(e,attribute,&value)==kAXErrorSuccess && value && CFGetTypeID(value)==CFArrayGetTypeID() && CFArrayGetCount(value)==(CFIndex)count){
        okay=1;for(size_t i=0;i<count;++i){int found=0;for(CFIndex j=0;j<CFArrayGetCount(value);++j)if(identified((AXUIElementRef)CFArrayGetValueAtIndex(value,j),ids[i]))++found;if(found!=1)okay=0;}
    }
    if(value)CFRelease(value);return okay;
}
static int wait_set(AXUIElementRef e,CFStringRef attribute,const unsigned *ids,size_t count){
    double end=now()+5;do{if(equals(e,attribute,ids,count))return 1;tick();}while(now()<end);return 0;
}
static AXError set(AXUIElementRef e,CFStringRef attribute,AXUIElementRef *items,size_t count){
    CFArrayRef a=CFArrayCreate(NULL,(const void**)items,(CFIndex)count,&kCFTypeArrayCallBacks);
    AXError error=AXUIElementSetAttributeValue(e,attribute,a);CFRelease(a);return error;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    int trusted=AXIsProcessTrusted();printf("AXIsProcessTrusted=%d; no prompt requested\n",trusted);fflush(stdout);
    if(!trusted)return 77;if(!strcmp(argv[1],"--trust"))return 0;
    pid_t pid=(pid_t)strtol(argv[1],NULL,10);AXUIElementRef app=AXUIElementCreateApplication(pid);CHECK(app);
    CHECK(AXUIElementSetMessagingTimeout(app,1.0)==kAXErrorSuccess);
    AXUIElementRef owner=wait_find(app,100);
    if(!owner){puts("OS AX cannot discover test owner in this window visibility state");CFRelease(app);return 78;}
    AXUIElementRef table=wait_find(app,200),items[4],table_items[3];CHECK(table);
    for(unsigned i=0;i<4;++i){items[i]=wait_find(app,1001+i);CHECK(items[i]);}
    for(unsigned i=0;i<3;++i){table_items[i]=wait_find(app,2000+i);CHECK(table_items[i]);}
    AXObserverRef observer=NULL;CHECK(AXObserverCreate(pid,observed,&observer)==kAXErrorSuccess);
    CHECK(AXObserverAddNotification(observer,owner,CFSTR("AXSelectedChildrenChanged"),NULL)==kAXErrorSuccess);
    CHECK(AXObserverAddNotification(observer,owner,CFSTR("AXSelectedRowsChanged"),NULL)==kAXErrorSuccess);
    CHECK(AXObserverAddNotification(observer,table,CFSTR("AXSelectedChildrenChanged"),NULL)==kAXErrorSuccess);
    CHECK(AXObserverAddNotification(observer,table,CFSTR("AXSelectedRowsChanged"),NULL)==kAXErrorSuccess);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),AXObserverGetRunLoopSource(observer),kCFRunLoopDefaultMode);
    CHECK(wait_set(owner,kAXSelectedChildrenAttribute,NULL,0));
    unsigned pair[]={1001,1002},triple[]={1001,1002,1003},after[]={1002,1003},one[]={1004};
    CHECK(set(owner,kAXSelectedChildrenAttribute,items,2)==kAXErrorSuccess && wait_set(owner,kAXSelectedChildrenAttribute,pair,2) && wait_set(owner,CFSTR("AXSelectedRows"),pair,2));
    CHECK(AXUIElementSetAttributeValue(items[2],kAXSelectedAttribute,kCFBooleanTrue)==kAXErrorSuccess && wait_set(owner,kAXSelectedChildrenAttribute,triple,3));
    CHECK(AXUIElementSetAttributeValue(items[0],kAXSelectedAttribute,kCFBooleanFalse)==kAXErrorSuccess && wait_set(owner,kAXSelectedChildrenAttribute,after,2));
    CHECK(set(owner,CFSTR("AXSelectedRows"),items+3,1)==kAXErrorSuccess && wait_set(owner,kAXSelectedChildrenAttribute,one,1));
    CHECK(set(owner,kAXSelectedChildrenAttribute,NULL,0)==kAXErrorSuccess && wait_set(owner,kAXSelectedChildrenAttribute,NULL,0));
    AXUIElementRef mixed[]={table_items[0],table_items[2]};unsigned mixed_ids[]={2000,2002},row[]={2000},changed[]={2001,2002},custom[]={2002};
    CHECK(set(table,kAXSelectedChildrenAttribute,mixed,2)==kAXErrorSuccess && wait_set(table,kAXSelectedChildrenAttribute,mixed_ids,2) && wait_set(table,CFSTR("AXSelectedRows"),row,1));
    CHECK(set(table,CFSTR("AXSelectedRows"),table_items+1,1)==kAXErrorSuccess && wait_set(table,kAXSelectedChildrenAttribute,changed,2));
    CHECK(set(table,CFSTR("AXSelectedRows"),NULL,0)==kAXErrorSuccess && wait_set(table,kAXSelectedChildrenAttribute,custom,1) && wait_set(table,CFSTR("AXSelectedRows"),NULL,0));
    double end=now()+5;while((children_events<8 || rows_events<8) && now()<end)tick();
    printf("independent observer children=%u rows=%u\n",children_events,rows_events);CHECK(children_events>=8 && rows_events>=8);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(),AXObserverGetRunLoopSource(observer),kCFRunLoopDefaultMode);CFRelease(observer);
    for(unsigned i=0;i<4;++i)CFRelease(items[i]);for(unsigned i=0;i<3;++i)CFRelease(table_items[i]);CFRelease(table);CFRelease(owner);CFRelease(app);
    puts("Independent OS AX complete set, offscreen add/remove, table row subset and actual observer delivery passed");return 0;
}
