/* Independent OS AX client: no NexaUI implementation or Objective-C linkage. */
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
static double now(void) {
    struct timespec t; if (clock_gettime(CLOCK_MONOTONIC,&t)) { perror("clock_gettime"); exit(1); }
    return (double)t.tv_sec+(double)t.tv_nsec/1.e9;
}
static void pause_poll(void) { struct timespec t={0,20000000}; nanosleep(&t,NULL); }
static int string_is(AXUIElementRef element,CFStringRef attribute,CFStringRef expected) {
    CFTypeRef value=NULL; AXError error=AXUIElementCopyAttributeValue(element,attribute,&value);
    int result=error==kAXErrorSuccess && value && CFGetTypeID(value)==CFStringGetTypeID() && CFEqual(value,expected);
    if (value) CFRelease(value); return result;
}
static AXUIElementRef find(AXUIElementRef root,CFStringRef identifier,unsigned depth,unsigned *budget) {
    if (depth>64 || !*budget) return NULL; --*budget;
    if (string_is(root,CFSTR("AXIdentifier"),identifier)) return (AXUIElementRef)CFRetain(root);
    CFTypeRef children=NULL; AXUIElementRef found=NULL;
    if (AXUIElementCopyAttributeValue(root,kAXChildrenAttribute,&children)==kAXErrorSuccess &&
        children && CFGetTypeID(children)==CFArrayGetTypeID()) {
        for (CFIndex i=0;i<CFArrayGetCount((CFArrayRef)children) && !found;++i) {
            CFTypeRef child=CFArrayGetValueAtIndex((CFArrayRef)children,i);
            if (child && CFGetTypeID(child)==AXUIElementGetTypeID()) found=find((AXUIElementRef)child,identifier,depth+1,budget);
        }
    }
    if (children) CFRelease(children); return found;
}
static AXUIElementRef wait_find(AXUIElementRef root,unsigned id_value) {
    char bytes[48]; snprintf(bytes,sizeof(bytes),"nexa-%016llx",(unsigned long long)id_value);
    CFStringRef identifier=CFStringCreateWithCString(NULL,bytes,kCFStringEncodingUTF8);
    if (!identifier) return NULL;
    double end=now()+10; AXUIElementRef found=NULL;
    do {
        unsigned budget=4096; found=find(root,identifier,0,&budget);
        if (!found) pause_poll();
    } while (!found && now()<end);
    CFRelease(identifier); return found;
}
static int wait_string(AXUIElementRef e,CFStringRef attribute,CFStringRef expected) {
    double end=now()+10;
    do { if (string_is(e,attribute,expected)) return 1; pause_poll(); } while (now()<end);
    return 0;
}
static int wait_one(AXUIElementRef e) {
    double end=now()+10;
    do {
        CFTypeRef value=NULL; int result=0;
        if (AXUIElementCopyAttributeValue(e,kAXValueAttribute,&value)==kAXErrorSuccess && value) {
            if (CFGetTypeID(value)==CFBooleanGetTypeID()) result=CFBooleanGetValue((CFBooleanRef)value);
            else if (CFGetTypeID(value)==CFNumberGetTypeID()) {
                int number=0; if (CFNumberGetValue((CFNumberRef)value,kCFNumberIntType,&number)) result=number==1;
            }
        }
        if (value) CFRelease(value); if (result) return 1; pause_poll();
    } while (now()<end);
    return 0;
}
int main(int argc,char **argv) {
    if (argc!=2) return 2;
    if (!AXIsProcessTrusted()) {
        puts("External AX client skipped: macOS accessibility trust is unavailable (no prompt requested).");
        return 77;
    }
    AXUIElementRef app=AXUIElementCreateApplication((pid_t)strtol(argv[1],NULL,10)); CHECK(app);
    CHECK(AXUIElementSetMessagingTimeout(app,2.0f)==kAXErrorSuccess);
    AXUIElementRef window=NULL; double end=now()+10;
    do {
        CFTypeRef windows=NULL;
        if (AXUIElementCopyAttributeValue(app,kAXWindowsAttribute,&windows)==kAXErrorSuccess &&
            windows && CFGetTypeID(windows)==CFArrayGetTypeID()) {
            for (CFIndex i=0;i<CFArrayGetCount((CFArrayRef)windows) && !window;++i) {
                AXUIElementRef candidate=(AXUIElementRef)CFArrayGetValueAtIndex((CFArrayRef)windows,i);
                if (CFGetTypeID(candidate)==AXUIElementGetTypeID() && string_is(candidate,kAXTitleAttribute,CFSTR("AX fixture"))) window=(AXUIElementRef)CFRetain(candidate);
            }
        }
        if (windows) CFRelease(windows); if (!window) pause_poll();
    } while (!window && now()<end);
    CHECK(window);
    AXUIElementRef button=wait_find(window,2),toggle=wait_find(window,3),secret=wait_find(window,5);
    AXUIElementRef count=wait_find(window,6),tab=wait_find(window,52);
    CHECK(button && toggle && secret && count && tab);
    CHECK(string_is(button,kAXRoleAttribute,kAXButtonRole));
    CHECK(string_is(button,kAXTitleAttribute,CFSTR("Öffnen 📁")));
    CHECK(string_is(secret,kAXValueAttribute,CFSTR("")));
    CHECK(AXUIElementPerformAction(button,kAXPressAction)==kAXErrorSuccess);
    CHECK(wait_string(count,kAXValueAttribute,CFSTR("1"))); /* Published application model, not an enqueue acknowledgement. */
    CHECK(AXUIElementPerformAction(toggle,kAXPressAction)==kAXErrorSuccess && wait_one(toggle));
    CHECK(AXUIElementPerformAction(tab,kAXPressAction)==kAXErrorSuccess && wait_one(tab));
    CFRelease(tab); CFRelease(count); CFRelease(secret); CFRelease(toggle); CFRelease(button); CFRelease(window); CFRelease(app);
    puts("Independent OS AX client passed names/roles/protection/press/toggle/selection model checks");
    return 0;
}
