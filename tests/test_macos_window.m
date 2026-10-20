#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>

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
    nx_window_status status; memset(&status,0xff,sizeof(status));
    if (nx_window_get_status(t->window,&status)!=NX_ERROR_STATE || status.state || status.known || status.capabilities || status.client_chrome ||
        nx_window_minimize(t->window)!=NX_ERROR_STATE || nx_window_maximize(t->window)!=NX_ERROR_STATE || nx_window_restore(t->window)!=NX_ERROR_STATE ||
        nx_window_begin_move(t->window)!=NX_ERROR_STATE || nx_window_begin_resize(t->window,NX_WINDOW_RESIZE_LEFT)!=NX_ERROR_STATE) abort();
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

@interface NXTestCommandSink : NSResponder { @public int received; }
- (void)nexaProbeHostAction:(id)sender;
@end
@implementation NXTestCommandSink
- (void)nexaProbeHostAction:(id)sender { (void)sender; ++received; }
@end
static unsigned consumed_interpret_calls;
static void consume_interpret(id view,SEL selector,NSArray *events) {
    (void)view; (void)selector; (void)events; ++consumed_interpret_calls;
}
static int test_native_commands(nx_window *w) {
    NSView *view=nx_window_native_nsview(w); NSWindow *native=nx_window_native_nswindow(w);
    id<NSTextInputClient> client=(id<NSTextInputClient>)view;
    CHECK(nx_window_set_ime_enabled(w,1)==NX_OK && drain(w)==0);
    CHECK([native makeFirstResponder:view]);
    nx_text_buffer *model=NULL; int handled=0; size_t anchor=0,cursor=0;
    CHECK(nx_text_buffer_create("first\nsecond",12,4096,&model)==NX_OK);
    /* Model an input method that consumes the key without returning an editor
       action. The original real NXMacView keyDown implementation still runs. */
    Class original=object_getClass(view),consuming=objc_allocateClassPair(original,"NXIMEConsumerProbe",0);
    CHECK(consuming && class_getInstanceSize(consuming)==class_getInstanceSize(original));
    CHECK(class_addMethod(consuming,@selector(interpretKeyEvents:),(IMP)consume_interpret,"v@:@"));
    objc_registerClassPair(consuming);
    NSEvent *left=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"\uf702" charactersIgnoringModifiers:@"\uf702" isARepeat:NO keyCode:123];
    object_setClass(view,consuming);
    @try { [view keyDown:left]; } @finally { object_setClass(view,original); }
    CHECK(consumed_interpret_calls==1);
    nx_event event; int available=0;
    CHECK(nx_window_next_event(w,&event,&available)==NX_OK && available &&
        event.type==NX_EVENT_KEY_DOWN_CONSUMED && event.key==NX_KEY_LEFT);
    CHECK(nx_text_buffer_apply_event(model,&event,&handled)==NX_OK && !handled);
    CHECK(nx_text_buffer_selection(model,&anchor,&cursor)==NX_OK && anchor==12 && cursor==12);
    CHECK(nx_window_next_event(w,&event,&available)==NX_OK && !available);
    [client doCommandBySelector:@selector(moveWordLeftAndModifySelection:)];
    CHECK(find(w,NX_EVENT_TEXT_COMMAND,&event)==0 &&
        event.text_command==(NX_TEXT_COMMAND_MOVE_WORD_LEFT|NX_TEXT_COMMAND_EXTEND_SELECTION) && !event.modifiers);
    NSEvent *select=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagCommand timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
    CHECK([view performKeyEquivalent:select]);
    CHECK(find(w,NX_EVENT_KEY_DOWN_CONSUMED,&event)==0 && event.key==NX_KEY_A && event.modifiers==NX_MOD_SUPER);
    CHECK(find(w,NX_EVENT_TEXT_COMMAND,&event)==0 && event.text_command==NX_TEXT_COMMAND_SELECT_ALL && event.modifiers==NX_MOD_SUPER);
    NSEvent *quit=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagCommand timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"q" charactersIgnoringModifiers:@"q" isARepeat:NO keyCode:12];
    CHECK(![view performKeyEquivalent:quit]); /* Leave the application's menu equivalent available. */
    NXTestCommandSink *sink=[[[NXTestCommandSink alloc] init] autorelease];
    NSResponder *prior=[[view nextResponder] retain]; [view setNextResponder:sink];
    @try { [client doCommandBySelector:@selector(nexaProbeHostAction:)]; }
    @finally { [view setNextResponder:prior]; [prior release]; }
    CHECK(sink->received==0); /* NSTextInputClient consumes unsupported commands locally. */
    /* Exercise AppKit's actual key-binding interpreter as well as direct
       selector callbacks. This is not an installed/physical IME acceptance. */
    NSEvent *end=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:NSEventModifierFlagControl timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"\x05" charactersIgnoringModifiers:@"e" isARepeat:NO keyCode:14];
    CHECK(drain(w)==0); [view keyDown:end];
    CHECK(find(w,NX_EVENT_TEXT_COMMAND,&event)==0 &&
        (event.text_command==NX_TEXT_COMMAND_LINE_END || event.text_command==NX_TEXT_COMMAND_PARAGRAPH_END) && event.modifiers==NX_MOD_CONTROL);
    CHECK(nx_text_buffer_set_selection(model,0,0)==NX_OK);
    CHECK(nx_text_buffer_apply_event(model,&event,&handled)==NX_OK && handled);
    CHECK(nx_text_buffer_selection(model,&anchor,&cursor)==NX_OK && anchor==5 && cursor==5);
    [client doCommandBySelector:@selector(deleteWordBackward:)];
    CHECK(find(w,NX_EVENT_TEXT_COMMAND,&event)==0 && event.text_command==NX_TEXT_COMMAND_DELETE_WORD_BACKWARD);
    CHECK(nx_text_buffer_apply_event(model,&event,&handled)==NX_OK && handled);
    const char *bytes=NULL; size_t length=0;
    CHECK(nx_text_buffer_text(model,&bytes,&length)==NX_OK && length==7 && !memcmp(bytes,"\nsecond",7));
    [client setMarkedText:@"candidate" selectedRange:NSMakeRange(9,0) replacementRange:NSMakeRange(NSNotFound,0)];
    CHECK(drain(w)==0); [client doCommandBySelector:@selector(cancelOperation:)];
    CHECK(![client hasMarkedText]);
    while (nx_window_next_event(w,&event,&available)==NX_OK && available)
        CHECK(event.type!=NX_EVENT_TEXT && event.type!=NX_EVENT_TEXT_COMMAND);
    CHECK(nx_window_set_ime_enabled(w,0)==NX_OK && drain(w)==0);
    nx_text_buffer_destroy(model);
    return 0;
}
static NSEvent *mouse(NSWindow *w,NSEventType type,NSPoint screen) {
    return [NSEvent mouseEventWithType:type location:[w convertPointFromScreen:screen] modifierFlags:0 timestamp:0
        windowNumber:[w windowNumber] context:nil eventNumber:1 clickCount:1 pressure:1];
}
static int test_management(void) {
    heap h={0}; nx_window *w=NULL; nx_window_desc d={"AppKit client chrome",360,240,
        NX_WINDOW_CLIENT_CHROME|NX_WINDOW_STATE_EVENTS,{&h,allocate,deallocate}};
    CHECK(nx_window_create(&d,&w)==NX_OK);
    NSWindow *native=nx_window_native_nswindow(w); NSView *view=nx_window_native_nsview(w);
    CHECK(([native styleMask]&NSWindowStyleMaskFullSizeContentView) && [native titleVisibility]==NSWindowTitleHidden &&
        [native titlebarAppearsTransparent] && [[native standardWindowButton:NSWindowCloseButton] isHidden]);
    CHECK(fabs([view bounds].size.width-360)<1 && fabs([view bounds].size.height-240)<1);
    nx_window_status s; CHECK(nx_window_get_status(w,&s)==NX_OK && s.client_chrome && s.known==15u && s.capabilities==127u);
    CHECK(nx_window_begin_move(w)==NX_ERROR_STATE && nx_window_begin_resize(w,(nx_window_resize_edge)0)==NX_ERROR_ARGUMENT);
    NSRect base=[native frame];
    for(int i=1;i<=8;++i) {
        [native setFrame:base display:YES]; NSPoint start=NSMakePoint(base.origin.x+100,base.origin.y+80);
        [view mouseDown:mouse(native,NSEventTypeLeftMouseDown,start)];
        CHECK(nx_window_begin_resize(w,(nx_window_resize_edge)i)==NX_OK && nx_window_begin_move(w)==NX_ERROR_STATE);
        [view mouseDragged:mouse(native,NSEventTypeLeftMouseDragged,NSMakePoint(start.x+12,start.y+8))];
        NSRect frame=[native frame];
        int left=i==1 || i==7 || i==8, right=i==3 || i==4 || i==5;
        int top=i==1 || i==2 || i==3, bottom=i==5 || i==6 || i==7;
        CHECK(fabs(frame.size.width-(base.size.width+(right?12:left?-12:0)))<1 &&
            fabs(frame.size.height-(base.size.height+(top?8:bottom?-8:0)))<1);
        CHECK(fabs(frame.origin.x-(base.origin.x+(left?12:0)))<1 && fabs(frame.origin.y-(base.origin.y+(bottom?8:0)))<1);
        [view mouseUp:mouse(native,NSEventTypeLeftMouseUp,start)];
        CHECK(nx_window_begin_resize(w,NX_WINDOW_RESIZE_LEFT)==NX_ERROR_STATE);
    }
    [native setFrame:base display:YES];
    NSPoint start=NSMakePoint(base.origin.x+100,base.origin.y+80);
    [view mouseDown:mouse(native,NSEventTypeLeftMouseDown,start)];
    CHECK(nx_window_begin_resize(w,NX_WINDOW_RESIZE_RIGHT)==NX_OK);
    [view mouseDragged:mouse(native,NSEventTypeLeftMouseDragged,NSMakePoint(start.x+20,start.y))];
    [view keyDown:[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0
        windowNumber:[native windowNumber] context:nil characters:@"\033" charactersIgnoringModifiers:@"\033" isARepeat:NO keyCode:53]];
    CHECK(NSEqualRects([native frame],base) && nx_window_begin_move(w)==NX_ERROR_STATE);
    CHECK(nx_window_maximize(w)==NX_OK); for(int i=0;i<100 && ![native isZoomed];++i) CHECK(nx_window_poll(w,10)==NX_OK);
    CHECK([native isZoomed] && nx_window_get_status(w,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MAXIMIZED));
    CHECK(nx_window_restore(w)==NX_OK); for(int i=0;i<100 && [native isZoomed];++i) CHECK(nx_window_poll(w,10)==NX_OK);
    CHECK(![native isZoomed]);
    CHECK(nx_window_minimize(w)==NX_OK); for(int i=0;i<100 && ![native isMiniaturized];++i) CHECK(nx_window_poll(w,10)==NX_OK);
    CHECK([native isMiniaturized] && nx_window_get_status(w,&s)==NX_OK && (s.state&NX_WINDOW_STATE_MINIMIZED));
    CHECK(nx_window_restore(w)==NX_OK); for(int i=0;i<100 && [native isMiniaturized];++i) CHECK(nx_window_poll(w,10)==NX_OK);
    CHECK(![native isMiniaturized]);
    nx_event event; CHECK(find(w,NX_EVENT_WINDOW_STATE,&event)==0);
    [native performClose:nil]; memset(&s,0xff,sizeof(s));
    CHECK(nx_window_get_status(w,&s)==NX_ERROR_STATE && !s.state && !s.known && !s.capabilities && !s.client_chrome);
    CHECK(nx_window_minimize(w)==NX_ERROR_STATE && nx_window_maximize(w)==NX_ERROR_STATE && nx_window_restore(w)==NX_ERROR_STATE);
    nx_window_destroy(w); CHECK(!h.live);
    puts("AppKit client chrome, eight native frame resize directions, Escape cancellation and native zoom/miniaturize/restore passed; physical input remains separate.");
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
        CHECK(drain(second) == 0 && test_input(w) == 0 && test_native_commands(w)==0);
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
        int owner = 0, other = 0; CHECK(nx_window_renderer_acquire(w,&owner) == NX_OK && nx_window_renderer_acquire(w,&other) == NX_ERROR_STATE);
        nx_window_destroy(w); CHECK(nx_window_get_info(w,&info) == NX_OK);
        CHECK(nx_window_renderer_release(w,&other) == NX_ERROR_STATE && nx_window_renderer_release(w,&owner) == NX_OK);
        CHECK(test_clipboard(w) == 0 && test_drop(w) == 0);
        NSView *retained = [nx_window_native_nsview(second) retain];
        id<NSTextInputClient> client = (id<NSTextInputClient>)retained;
        CHECK(nx_window_set_ime_enabled(second,1) == NX_OK);
        [client setMarkedText:@"stale α" selectedRange:NSMakeRange(7,0) replacementRange:NSMakeRange(NSNotFound,0)];
        CHECK([client hasMarkedText]);
        h.fail_on = h.calls + 1;
        [client setMarkedText:@"rejected" selectedRange:NSMakeRange(8,0) replacementRange:NSMakeRange(NSNotFound,0)];
        h.fail_on = 0;
        CHECK(nx_window_poll(second,0) == NX_ERROR_MEMORY);
        char preedit[32]; size_t length; int32_t begin, end;
        CHECK(nx_window_get_ime_preedit(second,preedit,sizeof(preedit),&length,&begin,&end) == NX_OK &&
            !strcmp(preedit,"stale \xce\xb1") && begin == 8 && end == 8);
        nx_window_destroy(second);
        CHECK(![client hasMarkedText] && [client markedRange].location == NSNotFound && [client selectedRange].location == NSNotFound);
        [client insertText:@"late callback" replacementRange:NSMakeRange(NSNotFound,0)];
        [client doCommandBySelector:@selector(moveLeft:)];
        [retained release];
        [nx_window_native_nswindow(w) performClose:nil];
        CHECK(nx_window_get_info(w,&info) == NX_OK && info.should_close);
        nx_window_destroy(w); CHECK(!h.live);
        CHECK(test_management()==0);
        puts("AppKit window, input, preedit, clipboard, drops and ownership passed.");
    }
    return 0;
}
