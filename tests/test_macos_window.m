#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct block { void *p; size_t bytes; } block;
typedef struct heap { block blocks[128]; size_t live, calls, fail_on; } heap;
static void *allocate(void *user, size_t bytes) {
    heap *h = user; if (++h->calls == h->fail_on) return NULL;
    void *p = malloc(bytes); if (p) { if (h->live == 128) abort(); h->blocks[h->live++] = (block){p,bytes}; } return p;
}
static void deallocate(void *user, void *p, size_t bytes) {
    heap *h = user; size_t i = 0;
    while (i < h->live && h->blocks[i].p != p) ++i;
    if (i == h->live || h->blocks[i].bytes != bytes) abort();
    h->blocks[i] = h->blocks[--h->live]; free(p);
}
static int drain(nx_window *w) {
    int available; nx_event e;
    do { CHECK(nx_window_next_event(w,&e,&available) == NX_OK); } while (available);
    return 0;
}
static int find(nx_window *w, nx_event_type type, nx_event *out) {
    int available;
    for (;;) {
        CHECK(nx_window_next_event(w,out,&available) == NX_OK);
        if (!available) return 1;
        if (out->type == type) return 0;
    }
}
typedef struct thread_case { nx_window *window; nx_result poll, info, create; } thread_case;
static void *wrong_thread(void *user) {
    thread_case *t = user; nx_window_info info;
    t->poll = nx_window_poll(t->window,0); t->info = nx_window_get_info(t->window,&info);
    nx_window_destroy(t->window);
    nx_window *other = NULL; nx_window_desc d = {"wrong thread",100,100,NX_WINDOW_HIDDEN,{0}};
    t->create = nx_window_create(&d,&other); if (other) abort(); return NULL;
}
@interface NXTestDrag : NSObject { @public NSPasteboard *pasteboard; NSPoint location; }
@end
@implementation NXTestDrag
- (NSPasteboard *)draggingPasteboard { return pasteboard; }
- (NSPoint)draggingLocation { return location; }
@end

static int test_clipboard(nx_window *w) {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSMutableArray *saved = [NSMutableArray array];
    for (NSPasteboardItem *original in [pb pasteboardItems]) {
        NSPasteboardItem *copy = [[[NSPasteboardItem alloc] init] autorelease];
        for (NSPasteboardType type in [original types]) {
            NSData *data = [original dataForType:type]; if (data) [copy setData:data forType:type];
        }
        [saved addObject:copy];
    }
    @try {
        const char text[] = "alpha \xce\xb1 \xf0\x9f\x8c\x8d\r\nline\n";
        CHECK(nx_window_clipboard_set_text(w,text,sizeof(text)-1) == NX_OK);
        size_t length; CHECK(nx_window_clipboard_get_text(w,NULL,0,&length) == NX_OK && length == sizeof(text)-1);
        char output[128] = "unchanged";
        CHECK(nx_window_clipboard_get_text(w,output,length,&length) == NX_ERROR_LIMIT && !strcmp(output,"unchanged"));
        CHECK(nx_window_clipboard_get_text(w,output,sizeof(output),&length) == NX_OK && !strcmp(output,text));
        CHECK(nx_window_clipboard_set_text(w,"a\0b",3) == NX_ERROR_ARGUMENT);
        CHECK(nx_window_clipboard_set_text(w,NULL,0) == NX_OK);
        CHECK(nx_window_clipboard_get_text(w,output,sizeof(output),&length) == NX_OK && !length && !output[0]);
    } @finally { [pb clearContents]; if ([saved count]) [pb writeObjects:saved]; }
    return 0;
}
static int test_drop(nx_window *w) {
    NSView *view = nx_window_native_nsview(w);
    NXTestDrag *drag = [[[NXTestDrag alloc] init] autorelease];
    drag->pasteboard = [NSPasteboard pasteboardWithUniqueName];
    drag->location = NSMakePoint(22, [view bounds].size.height - 31);
    NSArray *urls = @[[NSURL fileURLWithPath:@"/tmp/NexaUI α.txt"], [NSURL fileURLWithPath:@"/tmp/🌍.png"]];
    CHECK([drag->pasteboard writeObjects:urls]);
    CHECK([(id<NSDraggingDestination>)view draggingEntered:(id<NSDraggingInfo>)drag] == NSDragOperationCopy);
    CHECK([(id<NSDraggingDestination>)view performDragOperation:(id<NSDraggingInfo>)drag]);
    nx_file_drop_info info; size_t length; int available;
    CHECK(nx_window_next_file_drop(w,NULL,0,&length,&info,&available) == NX_OK && available && info.count == 2 && info.index == 0);
    char path[128] = "unchanged";
    CHECK(nx_window_next_file_drop(w,path,length,&length,&info,&available) == NX_ERROR_LIMIT && !strcmp(path,"unchanged"));
    CHECK(nx_window_next_file_drop(w,path,sizeof(path),&length,&info,&available) == NX_OK &&
        !strcmp(path,"/tmp/NexaUI \xce\xb1.txt") && info.x == 22 && info.y == 31);
    CHECK(nx_window_next_file_drop(w,path,sizeof(path),&length,&info,&available) == NX_OK &&
        !strcmp(path,"/tmp/\xf0\x9f\x8c\x8d.png") && info.index == 1);
    CHECK(nx_window_next_file_drop(w,path,sizeof(path),&length,&info,&available) == NX_OK && !available);
    [drag->pasteboard releaseGlobally]; return 0;
}
static int test_input(nx_window *w) {
    NSWindow *native = nx_window_native_nswindow(w); NSView *view = nx_window_native_nsview(w);
    CHECK(drain(w) == 0);
    NSEvent *mouse = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
        location:NSMakePoint(20,[view bounds].size.height - 30) modifierFlags:NSEventModifierFlagShift
        timestamp:0 windowNumber:[native windowNumber] context:nil eventNumber:1 clickCount:1 pressure:1];
    [view mouseDown:mouse];
    nx_event e; CHECK(find(w,NX_EVENT_POINTER_DOWN,&e) == 0 && e.x == 20 && e.y == 30 &&
        e.button == NX_BUTTON_LEFT && e.modifiers == NX_MOD_SHIFT);
    NSEvent *key = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
        modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:[native windowNumber]
        context:nil characters:@"f" charactersIgnoringModifiers:@"f" isARepeat:YES keyCode:3];
    [view keyDown:key];
    CHECK(find(w,NX_EVENT_KEY_DOWN,&e) == 0 && e.key == NX_KEY_F && e.repeat && e.modifiers == NX_MOD_SUPER);
    key = [NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint modifierFlags:0 timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"F" charactersIgnoringModifiers:@"F" isARepeat:NO keyCode:3];
    [view keyUp:key]; CHECK(find(w,NX_EVENT_KEY_UP,&e) == 0 && e.key == NX_KEY_F);
    id<NSTextInputClient> client = (id<NSTextInputClient>)view;
    [client insertText:@"A🌍é" replacementRange:NSMakeRange(NSNotFound,0)];
    CHECK(find(w,NX_EVENT_TEXT,&e) == 0 && e.scalar == 'A' && !e.modifiers);
    CHECK(find(w,NX_EVENT_TEXT,&e) == 0 && e.scalar == 0x1f30d);
    CHECK(find(w,NX_EVENT_TEXT,&e) == 0 && e.scalar == 0xe9);
    CHECK(nx_window_set_ime_enabled(w,1) == NX_OK);
    [client setMarkedText:@"a🌍é" selectedRange:NSMakeRange(3,1) replacementRange:NSMakeRange(NSNotFound,0)];
    size_t length; int32_t begin, end; char text[32] = "unchanged";
    CHECK(nx_window_get_ime_preedit(w,NULL,0,&length,&begin,&end) == NX_OK && length == 7 && begin == 5 && end == 7);
    CHECK(nx_window_get_ime_preedit(w,text,7,&length,&begin,&end) == NX_ERROR_LIMIT && !strcmp(text,"unchanged"));
    [client setMarkedText:@"a🌍é" selectedRange:NSMakeRange(2,0) replacementRange:NSMakeRange(NSNotFound,0)];
    CHECK(nx_window_get_ime_preedit(w,text,sizeof(text),&length,&begin,&end) == NX_OK &&
        !strcmp(text,"a\xf0\x9f\x8c\x8d\xc3\xa9") && begin == 5 && end == 7);
    CHECK([client markedRange].length == 4 && [client selectedRange].location == 3);
    CHECK(nx_window_set_ime_rect(w,(nx_rect){12,16,2,20}) == NX_OK);
    NSRange actual; NSRect rect = [client firstRectForCharacterRange:NSMakeRange(0,1) actualRange:&actual];
    CHECK(isfinite(rect.origin.x) && rect.size.width == 2 && rect.size.height == 20);
    CHECK(nx_window_set_ime_purpose(w,NX_IME_PURPOSE_PASSWORD) == NX_ERROR_UNSUPPORTED);
    CHECK(![client hasMarkedText]);
    CHECK([client attributedSubstringForProposedRange:NSMakeRange(0,0) actualRange:&actual] == nil);
    CHECK(nx_window_set_ime_purpose(w,NX_IME_PURPOSE_NORMAL) == NX_OK);
    [client setMarkedText:@"é" selectedRange:NSMakeRange(1,0) replacementRange:NSMakeRange(NSNotFound,0)];
    CHECK(drain(w) == 0); [client unmarkText];
    CHECK(find(w,NX_EVENT_TEXT,&e) == 0 && e.scalar == 0xe9 && ![client hasMarkedText]);
    [client setMarkedText:@"cancel" selectedRange:NSMakeRange(6,0) replacementRange:NSMakeRange(NSNotFound,0)];
    CHECK(drain(w) == 0); CHECK(nx_window_set_ime_enabled(w,0) == NX_OK);
    int available;
    while (nx_window_next_event(w,&e,&available) == NX_OK && available) CHECK(e.type != NX_EVENT_TEXT);
    return 0;
}
int main(void) {
    @autoreleasepool {
        for (size_t fail = 1; fail <= 2; ++fail) {
            heap h = {0}; h.fail_on = fail;
            nx_window_desc d = {"failed",100,100,NX_WINDOW_HIDDEN,{&h,allocate,deallocate}};
            nx_window *w = NULL; CHECK(nx_window_create(&d,&w) == NX_ERROR_MEMORY && !w && !h.live);
        }
        heap h = {0}; nx_allocator a = {&h,allocate,deallocate};
        nx_window_desc d = {"NexaUI AppKit α 🌍",320,200,NX_WINDOW_HIDDEN,a};
        nx_window *w = NULL, *second = NULL;
        CHECK(nx_window_create(&d,&w) == NX_OK && nx_window_create(&d,&second) == NX_OK);
        CHECK(nx_window_poll(w,0) == NX_OK);
        nx_window_info info; CHECK(nx_window_get_info(w,&info) == NX_OK && info.width == 320 && info.height == 200 && info.scale > 0);
        printf("AppKit native client %.0fx%.0f points, backing %.2f\n",info.width,info.height,info.scale);
        CHECK(!strcmp([[nx_window_native_nswindow(w) title] UTF8String],d.title_utf8));
        CHECK(drain(second) == 0 && test_input(w) == 0);
        nx_event e; int available; CHECK(nx_window_next_event(second,&e,&available) == NX_OK && !available);
        [nx_window_native_nswindow(w) setContentSize:NSMakeSize(420,230)];
        CHECK(nx_window_poll(w,0) == NX_OK && nx_window_get_info(w,&info) == NX_OK && info.width == 420 && info.height == 230);
        for (int shape = NX_CURSOR_DEFAULT; shape <= NX_CURSOR_BUSY; ++shape)
            CHECK(nx_window_set_cursor(w,(nx_cursor_shape)shape) == NX_OK);
        int motion; CHECK(nx_window_get_reduced_motion(w,&motion) == NX_OK && (motion == 0 || motion == 1));
        thread_case t = {w,0,0,0}; pthread_t thread;
        CHECK(!pthread_create(&thread,NULL,wrong_thread,&t) && !pthread_join(thread,NULL));
        CHECK(t.poll == NX_ERROR_STATE && t.info == NX_ERROR_STATE && t.create == NX_ERROR_STATE);
        CHECK(nx_window_get_info(w,&info) == NX_OK);
        int owner, other; CHECK(nx_window_renderer_acquire(w,&owner) == NX_OK && nx_window_renderer_acquire(w,&other) == NX_ERROR_STATE);
        nx_window_destroy(w); CHECK(nx_window_get_info(w,&info) == NX_OK);
        CHECK(nx_window_renderer_release(w,&other) == NX_ERROR_STATE && nx_window_renderer_release(w,&owner) == NX_OK);
        CHECK(test_clipboard(w) == 0 && test_drop(w) == 0);
        NSView *retained = [nx_window_native_nsview(second) retain];
        nx_window_destroy(second);
        [(id<NSTextInputClient>)retained insertText:@"late callback" replacementRange:NSMakeRange(NSNotFound,0)];
        [retained release];
        [nx_window_native_nswindow(w) performClose:nil];
        CHECK(nx_window_get_info(w,&info) == NX_OK && info.should_close);
        nx_window_destroy(w); CHECK(!h.live);
        puts("AppKit window, input, preedit, clipboard, drops and ownership passed.");
    }
    return 0;
}
