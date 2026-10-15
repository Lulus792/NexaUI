#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"
#include "window_internal.h"
#include "event_queue.h"
#include "drop_queue.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

@class NXMacView, NXMacWindowDelegate;

struct nx_window {
    nx_allocator allocator;
    NSWindow *native;
    NXMacView *view;
    NXMacWindowDelegate *delegate;
    nx_platform_event_queue events;
    nx_platform_drop_queue drops;
    const void *renderer_owner;
    nx_window *next;
    int should_close, ime_enabled, discarding_marked;
    nx_ime_purpose ime_purpose;
    nx_rect caret;
    nx_cursor_shape cursor;
    uint32_t modifiers, buttons;
    nx_key pressed[128];
    char *preedit;
    size_t preedit_length;
    int32_t preedit_begin, preedit_end;
};

/* AppKit is process-wide and main-thread-only. This list routes Quit only for
   windows created here; every UI context and input queue remains independent. */
static nx_window *nx_macos_windows;
static int nx_macos_owns_application;
static id nx_macos_application_delegate;

static void *nx_mac_allocate(void *user, size_t size) { (void)user; return malloc(size); }
static void nx_mac_free(void *user, void *p, size_t size) { (void)user; (void)size; free(p); }
static int nx_mac_owner(nx_window *w) { return w && pthread_main_np(); }
static void nx_mac_push(nx_window *w, nx_event event) {
    if (w) nx_platform_event_queue_push(&w->events, event);
}
static uint32_t nx_mac_modifiers(NSEventModifierFlags flags) {
    return ((flags & NSEventModifierFlagShift) ? NX_MOD_SHIFT : 0u) |
           ((flags & NSEventModifierFlagControl) ? NX_MOD_CONTROL : 0u) |
           ((flags & NSEventModifierFlagOption) ? NX_MOD_ALT : 0u) |
           ((flags & NSEventModifierFlagCommand) ? NX_MOD_SUPER : 0u);
}
static NSCursor *nx_mac_cursor(nx_cursor_shape shape) {
    switch (shape) {
    case NX_CURSOR_TEXT: return [NSCursor IBeamCursor];
    case NX_CURSOR_LINK: return [NSCursor pointingHandCursor];
    case NX_CURSOR_CROSSHAIR: return [NSCursor crosshairCursor];
    case NX_CURSOR_RESIZE_HORIZONTAL: return [NSCursor resizeLeftRightCursor];
    case NX_CURSOR_RESIZE_VERTICAL: return [NSCursor resizeUpDownCursor];
    case NX_CURSOR_RESIZE_DIAGONAL_NW_SE:
    case NX_CURSOR_RESIZE_DIAGONAL_NE_SW: return [NSCursor crosshairCursor];
    case NX_CURSOR_MOVE: return [NSCursor openHandCursor];
    case NX_CURSOR_NOT_ALLOWED: return [NSCursor operationNotAllowedCursor];
    default: return [NSCursor arrowCursor]; /* Busy has no public AppKit cursor. */
    }
}
static void nx_mac_resize(nx_window *w) {
    if (!w) return;
    NSSize size = [[w->native contentView] bounds].size;
    nx_event event = {0}; event.type = NX_EVENT_RESIZE;
    event.width = [w->native isMiniaturized] ? 0 : (float)size.width;
    event.height = [w->native isMiniaturized] ? 0 : (float)size.height;
    event.scale = (float)[w->native backingScaleFactor];
    nx_mac_push(w, event);
}
static void nx_mac_clear_preedit(nx_window *w) {
    if (!w || !w->preedit) return;
    w->allocator.deallocate(w->allocator.user, w->preedit, w->preedit_length + 1u);
    w->preedit = NULL; w->preedit_length = 0; w->preedit_begin = w->preedit_end = 0;
    nx_event event = {0}; event.type = NX_EVENT_IME_PREEDIT_CHANGED; nx_mac_push(w, event);
}
static int nx_mac_utf16_boundary(NSString *text, NSUInteger offset) {
    if (offset > [text length]) return 0;
    if (!offset || offset == [text length]) return 1;
    unichar before = [text characterAtIndex:offset - 1], after = [text characterAtIndex:offset];
    return !(before >= 0xd800 && before <= 0xdbff && after >= 0xdc00 && after <= 0xdfff);
}
static nx_result nx_mac_preedit(nx_window *w, NSString *text, NSRange selected) {
    NSUInteger units = [text length];
    if (units > 524288u) return NX_ERROR_LIMIT;
    if (selected.location != NSNotFound &&
        (selected.location > units || selected.length > units - selected.location ||
         !nx_mac_utf16_boundary(text, selected.location) ||
         !nx_mac_utf16_boundary(text, selected.location + selected.length))) return NX_ERROR_ARGUMENT;
    NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if (!data) return NX_ERROR_ARGUMENT;
    size_t length = (size_t)[data length];
    if (length > 1048576u) return NX_ERROR_LIMIT;
    if (!length) { nx_mac_clear_preedit(w); return NX_OK; }
    char *copy = w->allocator.allocate(w->allocator.user, length + 1u);
    if (!copy) return NX_ERROR_MEMORY;
    memcpy(copy, [data bytes], length); copy[length] = 0;
    int32_t begin = -1, end = -1;
    if (selected.location != NSNotFound) {
        begin = (int32_t)[[text substringToIndex:selected.location] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        end = (int32_t)[[text substringToIndex:selected.location + selected.length] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    }
    if (w->preedit) w->allocator.deallocate(w->allocator.user, w->preedit, w->preedit_length + 1u);
    w->preedit = copy; w->preedit_length = length; w->preedit_begin = begin; w->preedit_end = end;
    nx_event event = {0}; event.type = NX_EVENT_IME_PREEDIT_CHANGED; nx_mac_push(w, event);
    return NX_OK;
}
static void nx_mac_text(nx_window *w, NSString *text) {
    if (!w || !text) return;
    NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
    if (!data || [data length] > 1048576u) { w->events.error = NX_ERROR_ARGUMENT; return; }
    size_t offset = 0, length = (size_t)[data length];
    const char *bytes = [data bytes];
    while (offset < length) {
        uint32_t scalar;
        if (nx_utf8_decode(bytes, length, &offset, &scalar) != NX_OK) { w->events.error = NX_ERROR_ARGUMENT; return; }
        if (scalar < 32u || (scalar >= 0x7fu && scalar < 0xa0u)) continue;
        nx_event event = {0}; event.type = NX_EVENT_TEXT; event.scalar = scalar;
        nx_mac_push(w, event); /* A committed scalar is independent of shortcut state. */
    }
}
static nx_key nx_mac_key(NSEvent *event) {
    switch ([event keyCode]) {
    case 48: return NX_KEY_TAB; case 36: case 76: return NX_KEY_ENTER;
    case 53: return NX_KEY_ESCAPE; case 49: return NX_KEY_SPACE;
    case 51: return NX_KEY_BACKSPACE; case 117: return NX_KEY_DELETE;
    case 114: return NX_KEY_INSERT;
    case 123: return NX_KEY_LEFT; case 124: return NX_KEY_RIGHT;
    case 125: return NX_KEY_DOWN; case 126: return NX_KEY_UP;
    case 115: return NX_KEY_HOME; case 119: return NX_KEY_END;
    case 116: return NX_KEY_PAGE_UP; case 121: return NX_KEY_PAGE_DOWN;
    case 122: return NX_KEY_F1; case 120: return NX_KEY_F2; case 99: return NX_KEY_F3;
    case 118: return NX_KEY_F4; case 96: return NX_KEY_F5; case 97: return NX_KEY_F6;
    case 98: return NX_KEY_F7; case 100: return NX_KEY_F8; case 101: return NX_KEY_F9;
    case 109: return NX_KEY_F10; case 103: return NX_KEY_F11; case 111: return NX_KEY_F12;
    default: break;
    }
    NSString *characters = [event charactersIgnoringModifiers];
    if (![characters length]) return NX_KEY_UNKNOWN;
    unichar c = [characters characterAtIndex:0];
    if (c >= 'a' && c <= 'z') return (nx_key)(NX_KEY_A + c - 'a');
    if (c >= 'A' && c <= 'Z') return (nx_key)(NX_KEY_A + c - 'A');
    if (c >= '0' && c <= '9') return (nx_key)(NX_KEY_0 + c - '0');
    return NX_KEY_UNKNOWN;
}
static void nx_mac_focus(nx_window *w, int focused);

@interface NXMacApplicationDelegate : NSObject <NSApplicationDelegate>
@end
@implementation NXMacApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender;
    for (nx_window *w = nx_macos_windows; w; w = w->next) w->should_close = 1;
    return NSTerminateCancel; /* The application retires renderers before exit. */
}
@end

@interface NXMacView : NSView <NSTextInputClient, NSDraggingDestination> {
@public
    nx_window *owner;
    NSTrackingArea *tracking;
    NSString *marked;
    NSRange marked_selection;
}
@end

@implementation NXMacView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { (void)event; return YES; }
- (void)drawRect:(NSRect)rect {
    if (!owner || owner->renderer_owner) return;
    [[NSColor windowBackgroundColor] setFill]; NSRectFill(rect);
}
- (void)updateTrackingAreas {
    if (tracking) { [self removeTrackingArea:tracking]; [tracking release]; tracking = nil; }
    tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:
        NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingCursorUpdate |
        NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect owner:self userInfo:nil];
    [self addTrackingArea:tracking]; [super updateTrackingAreas];
}
- (void)resetCursorRects { if (owner) [self addCursorRect:[self bounds] cursor:nx_mac_cursor(owner->cursor)]; }
- (void)cursorUpdate:(NSEvent *)event { (void)event; if (owner) [nx_mac_cursor(owner->cursor) set]; }
- (void)nxPointer:(NSEvent *)native type:(nx_event_type)type button:(uint32_t)button {
    if (!owner) return;
    NSPoint point = [self convertPoint:[native locationInWindow] fromView:nil];
    nx_event event = {0}; event.type = type; event.x = (float)point.x; event.y = (float)point.y;
    event.button = button; event.modifiers = nx_mac_modifiers([native modifierFlags]);
    if (type == NX_EVENT_POINTER_DOWN) owner->buttons |= button;
    if (type == NX_EVENT_POINTER_UP) owner->buttons &= ~button;
    nx_mac_push(owner, event);
}
- (void)mouseMoved:(NSEvent *)event { [self nxPointer:event type:NX_EVENT_POINTER_MOVE button:0]; }
- (void)mouseEntered:(NSEvent *)event { [self mouseMoved:event]; }
- (void)mouseExited:(NSEvent *)event { [self mouseMoved:event]; }
- (void)mouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)rightMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)otherMouseDragged:(NSEvent *)event { [self mouseMoved:event]; }
- (void)mouseDown:(NSEvent *)event { [self nxPointer:event type:NX_EVENT_POINTER_DOWN button:NX_BUTTON_LEFT]; }
- (void)mouseUp:(NSEvent *)event { [self nxPointer:event type:NX_EVENT_POINTER_UP button:NX_BUTTON_LEFT]; }
- (void)rightMouseDown:(NSEvent *)event { [self nxPointer:event type:NX_EVENT_POINTER_DOWN button:NX_BUTTON_RIGHT]; }
- (void)rightMouseUp:(NSEvent *)event { [self nxPointer:event type:NX_EVENT_POINTER_UP button:NX_BUTTON_RIGHT]; }
- (void)otherMouseDown:(NSEvent *)event {
    NSUInteger n = [event buttonNumber];
    uint32_t button = n == 2 ? NX_BUTTON_MIDDLE : n == 3 ? NX_BUTTON_X1 : n == 4 ? NX_BUTTON_X2 : 0;
    if (button) [self nxPointer:event type:NX_EVENT_POINTER_DOWN button:button];
}
- (void)otherMouseUp:(NSEvent *)event {
    NSUInteger n = [event buttonNumber];
    uint32_t button = n == 2 ? NX_BUTTON_MIDDLE : n == 3 ? NX_BUTTON_X1 : n == 4 ? NX_BUTTON_X2 : 0;
    if (button) [self nxPointer:event type:NX_EVENT_POINTER_UP button:button];
}
- (void)scrollWheel:(NSEvent *)native {
    if (!owner) return;
    NSPoint p = [self convertPoint:[native locationInWindow] fromView:nil];
    double factor = [native hasPreciseScrollingDeltas] ? 1.0 / 40.0 : 1.0;
    nx_event event = {0}; event.type = NX_EVENT_WHEEL;
    event.x = (float)p.x; event.y = (float)p.y; event.modifiers = nx_mac_modifiers([native modifierFlags]);
    event.delta_x = (float)([native scrollingDeltaX] * factor);
    event.delta_y = (float)([native scrollingDeltaY] * factor);
    if (isfinite(event.delta_x) && isfinite(event.delta_y)) nx_mac_push(owner, event);
}
- (void)keyDown:(NSEvent *)native {
    if (!owner) return;
    owner->modifiers = nx_mac_modifiers([native modifierFlags]);
    nx_key key = nx_mac_key(native); unsigned code = [native keyCode];
    if (code < 128 && owner->pressed[code] != NX_KEY_UNKNOWN) key = owner->pressed[code];
    if (code < 128) owner->pressed[code] = key;
    if (key != NX_KEY_UNKNOWN) {
        nx_event event = {0}; event.type = NX_EVENT_KEY_DOWN; event.key = key;
        event.modifiers = owner->modifiers; event.repeat = [native isARepeat]; nx_mac_push(owner, event);
    }
    if (!(owner->modifiers & (NX_MOD_SUPER | NX_MOD_CONTROL))) [self interpretKeyEvents:@[native]];
}
- (void)keyUp:(NSEvent *)native {
    if (!owner) return;
    owner->modifiers = nx_mac_modifiers([native modifierFlags]);
    unsigned code = [native keyCode];
    nx_key key = code < 128 ? owner->pressed[code] : nx_mac_key(native);
    if (code < 128) owner->pressed[code] = NX_KEY_UNKNOWN;
    if (key == NX_KEY_UNKNOWN) return;
    nx_event event = {0}; event.type = NX_EVENT_KEY_UP; event.key = key;
    event.modifiers = owner->modifiers; nx_mac_push(owner, event);
}
- (void)flagsChanged:(NSEvent *)native {
    if (!owner) return;
    uint32_t flags = nx_mac_modifiers([native modifierFlags]), changed = flags ^ owner->modifiers;
    const uint32_t masks[] = {NX_MOD_SHIFT, NX_MOD_CONTROL, NX_MOD_ALT, NX_MOD_SUPER};
    const nx_key keys[] = {NX_KEY_SHIFT, NX_KEY_CONTROL, NX_KEY_ALT, NX_KEY_SUPER};
    for (unsigned i = 0; i < 4; ++i) if (changed & masks[i]) {
        nx_event event = {0}; event.type = flags & masks[i] ? NX_EVENT_KEY_DOWN : NX_EVENT_KEY_UP;
        event.key = keys[i]; event.modifiers = flags; nx_mac_push(owner, event);
    }
    owner->modifiers = flags;
}
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if (owner && ([event modifierFlags] & NSEventModifierFlagCommand)) {
        [self keyDown:event]; return YES;
    }
    return NO;
}
- (BOOL)hasMarkedText { return owner && [marked length] != 0; }
- (NSRange)markedRange { return owner && [marked length] ? NSMakeRange(0, [marked length]) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return !owner ? NSMakeRange(NSNotFound,0) : [marked length] ? marked_selection : NSMakeRange(0,0); }
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }
- (void)setMarkedText:(id)value selectedRange:(NSRange)selected replacementRange:(NSRange)replacement {
    if (!owner || !owner->ime_enabled) return;
    NSString *text = [value isKindOfClass:[NSAttributedString class]] ? [value string] : value;
    if (![text isKindOfClass:[NSString class]]) return;
    if (replacement.location != NSNotFound &&
        (replacement.location != 0 || replacement.length != [marked length])) return;
    nx_result result = nx_mac_preedit(owner, text, selected);
    if (result != NX_OK) { if (result == NX_ERROR_MEMORY) owner->events.error = result; return; }
    [marked release]; marked = [text copy]; marked_selection = selected;
}
- (void)unmarkText {
    if (owner && !owner->discarding_marked && [marked length]) nx_mac_text(owner, marked);
    [marked release]; marked = nil; marked_selection = NSMakeRange(0,0); nx_mac_clear_preedit(owner);
}
- (void)insertText:(id)value replacementRange:(NSRange)replacement {
    if (!owner) return;
    NSString *text = [value isKindOfClass:[NSAttributedString class]] ? [value string] : value;
    if (![text isKindOfClass:[NSString class]]) return;
    /* Full-document replacement needs revision-bound surrounding text, which
       is deliberately unsupported in this initial AppKit adapter. */
    if (replacement.location != NSNotFound &&
        (replacement.location != 0 || replacement.length != [marked length])) return;
    [marked release]; marked = nil; marked_selection = NSMakeRange(0,0);
    nx_mac_clear_preedit(owner); nx_mac_text(owner, text);
}
- (void)doCommandBySelector:(SEL)selector { (void)selector; /* Physical actions were already queued. */ }
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual {
    if (actual) *actual = NSMakeRange(NSNotFound,0);
    if (!owner || owner->ime_purpose == NX_IME_PURPOSE_PASSWORD || !marked || range.location > [marked length] ||
        range.length > [marked length] - range.location || !nx_mac_utf16_boundary(marked, range.location) ||
        !nx_mac_utf16_boundary(marked, range.location + range.length)) return nil;
    if (actual) *actual = range;
    return [[[NSAttributedString alloc] initWithString:[marked substringWithRange:range]] autorelease];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return NSNotFound; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    if (actual) *actual = range;
    if (!owner) return NSZeroRect;
    nx_rect caret = owner->caret;
    NSRect local = NSMakeRect(caret.x, caret.y, caret.width, caret.height);
    return [[self window] convertRectToScreen:[self convertRect:local toView:nil]];
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    if (!owner) return NSDragOperationNone;
    NSArray *urls = [[sender draggingPasteboard] readObjectsForClasses:@[[NSURL class]]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    return [urls count] ? NSDragOperationCopy : NSDragOperationNone;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    if (!owner) return NO;
    NSArray *urls = [[sender draggingPasteboard] readObjectsForClasses:@[[NSURL class]]
        options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    size_t count = (size_t)[urls count];
    if (!count || count > UINT32_MAX || count > SIZE_MAX / sizeof(nx_platform_drop_entry)) return NO;
    size_t bytes = count * sizeof(nx_platform_drop_entry);
    nx_platform_drop_entry *batch = owner->allocator.allocate(owner->allocator.user, bytes);
    if (!batch) { owner->events.error = NX_ERROR_MEMORY; return NO; }
    memset(batch, 0, bytes);
    NSPoint p = [self convertPoint:[sender draggingLocation] fromView:nil];
    nx_result result = NX_OK;
    for (size_t i = 0; i < count; ++i) {
        NSURL *url = [urls objectAtIndex:i]; NSString *path = [url path];
        NSData *data = [path dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
        size_t length = (size_t)[data length];
        if (![url isFileURL] || !data || !length || length > 1048576u || memchr([data bytes], 0, length)) { result = NX_ERROR_ARGUMENT; break; }
        batch[i].path = owner->allocator.allocate(owner->allocator.user, length + 1u);
        if (!batch[i].path) { result = NX_ERROR_MEMORY; break; }
        memcpy(batch[i].path, [data bytes], length); batch[i].path[length] = 0; batch[i].length = length;
        batch[i].info = (nx_file_drop_info){(float)p.x, (float)p.y, (uint32_t)i, (uint32_t)count};
    }
    if (result == NX_OK) result = nx_platform_drop_queue_append_batch(&owner->drops, batch, count);
    if (result != NX_OK) for (size_t i = 0; i < count; ++i)
        if (batch[i].path) owner->allocator.deallocate(owner->allocator.user, batch[i].path, batch[i].length + 1u);
    owner->allocator.deallocate(owner->allocator.user, batch, bytes);
    if (result == NX_ERROR_MEMORY || result == NX_ERROR_LIMIT) owner->events.error = result;
    return result == NX_OK;
}
- (void)dealloc {
    owner = NULL; [marked release];
    if (tracking) { [self removeTrackingArea:tracking]; [tracking release]; }
    [super dealloc];
}
@end

static void nx_mac_discard(nx_window *w) {
    w->discarding_marked = 1;
    [[w->view inputContext] discardMarkedText]; [w->view unmarkText];
    w->discarding_marked = 0;
}
static void nx_mac_focus(nx_window *w, int focused) {
    if (!w) return;
    if (!focused) {
        nx_mac_discard(w); w->modifiers = 0; memset(w->pressed, 0, sizeof(w->pressed));
        if (w->buttons) { nx_event cancel = {0}; cancel.type = NX_EVENT_POINTER_CANCEL; nx_mac_push(w, cancel); w->buttons = 0; }
    }
    nx_event event = {0}; event.type = NX_EVENT_FOCUS; event.focused = focused; nx_mac_push(w, event);
}

@interface NXMacWindowDelegate : NSObject <NSWindowDelegate> { @public nx_window *owner; }
@end
@implementation NXMacWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender { (void)sender; if (owner) owner->should_close = 1; return NO; }
- (void)windowDidBecomeKey:(NSNotification *)note { (void)note; nx_mac_focus(owner,1); }
- (void)windowDidResignKey:(NSNotification *)note { (void)note; nx_mac_focus(owner,0); }
- (void)windowDidResize:(NSNotification *)note { (void)note; nx_mac_resize(owner); }
- (void)windowDidChangeBackingProperties:(NSNotification *)note { (void)note; nx_mac_resize(owner); }
- (void)windowDidMiniaturize:(NSNotification *)note { (void)note; nx_mac_focus(owner,0); nx_mac_resize(owner); }
- (void)windowDidDeminiaturize:(NSNotification *)note { (void)note; nx_mac_resize(owner); }
@end

nx_result nx_window_create(const nx_window_desc *desc, nx_window **out_window) {
    if (!out_window) return NX_ERROR_ARGUMENT;
    *out_window = NULL;
    if (!desc || !isfinite(desc->width) || !isfinite(desc->height) || desc->width <= 0 || desc->height <= 0 ||
        (desc->flags & ~NX_WINDOW_HIDDEN) || (!!desc->allocator.allocate != !!desc->allocator.deallocate)) return NX_ERROR_ARGUMENT;
    if (!pthread_main_np()) return NX_ERROR_STATE;
    if (desc->width > 32768 || desc->height > 32768) return NX_ERROR_LIMIT;
    const char *title = desc->title_utf8 ? desc->title_utf8 : "NexaUI";
    size_t title_length = strlen(title);
    if (title_length > 1048576u || nx_utf8_validate(title, title_length, NULL) != NX_OK) return NX_ERROR_ARGUMENT;
    nx_allocator a = desc->allocator;
    if (!a.allocate) a = (nx_allocator){NULL,nx_mac_allocate,nx_mac_free};
    nx_window *w = a.allocate(a.user, sizeof(*w));
    if (!w) return NX_ERROR_MEMORY;
    *w = (nx_window){0}; w->allocator = a;
    nx_platform_drop_queue_init(&w->drops, a);
    nx_result result = nx_platform_event_queue_init(&w->events, a, 64);
    if (result != NX_OK) { a.deallocate(a.user,w,sizeof(*w)); return result; }
    @autoreleasepool {
        if (!NSApp) {
            [NSApplication sharedApplication]; nx_macos_owns_application = 1;
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
            NSMenu *menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
            NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:@"" action:NULL keyEquivalent:@""] autorelease];
            NSMenu *application = [[[NSMenu alloc] initWithTitle:@""] autorelease];
            [application addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
            [item setSubmenu:application]; [menu addItem:item]; [NSApp setMainMenu:menu];
        }
        if (nx_macos_owns_application && !nx_macos_application_delegate && ![NSApp delegate]) {
            nx_macos_application_delegate = [[NXMacApplicationDelegate alloc] init];
            [NSApp setDelegate:nx_macos_application_delegate];
        }
        w->native = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,desc->width,desc->height)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                      NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
        [w->native setReleasedWhenClosed:NO];
        w->view = [[NXMacView alloc] initWithFrame:NSMakeRect(0,0,desc->width,desc->height)];
        w->delegate = [[NXMacWindowDelegate alloc] init];
        if (!w->native || !w->view || !w->delegate) result = NX_ERROR_MEMORY;
        if (result == NX_OK) {
            w->view->owner = w; w->delegate->owner = w;
            [w->native setReleasedWhenClosed:NO]; [w->native setDelegate:w->delegate];
            [w->native setTitle:[[[NSString alloc] initWithBytes:title length:title_length encoding:NSUTF8StringEncoding] autorelease]];
            [w->view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
            [w->view registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
            [w->native setContentView:w->view]; [w->native setAcceptsMouseMovedEvents:YES];
            [w->native makeFirstResponder:w->view]; [w->native center];
            w->next = nx_macos_windows; nx_macos_windows = w;
            if (!(desc->flags & NX_WINDOW_HIDDEN)) [w->native makeKeyAndOrderFront:nil];
            nx_mac_resize(w);
        }
    }
    if (result != NX_OK) { nx_window_destroy(w); return result; }
    *out_window = w; return NX_OK;
}

nx_result nx_window_poll(nx_window *w, uint32_t timeout_ms) {
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool {
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:(double)timeout_ms / 1000.0];
        for (unsigned count = 0; count < 4096; ++count) {
            NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:count ? [NSDate distantPast] : deadline
                inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
    return w->events.error;
}
nx_result nx_window_get_info(nx_window *w, nx_window_info *out) {
    if (!out) return NX_ERROR_ARGUMENT;
    *out = (nx_window_info){0};
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool {
        NSSize size = [w->view bounds].size;
        out->width = [w->native isMiniaturized] ? 0 : (float)size.width;
        out->height = [w->native isMiniaturized] ? 0 : (float)size.height;
        out->scale = (float)[w->native backingScaleFactor]; out->should_close = w->should_close;
        return NX_OK;
    }
}
nx_result nx_window_next_event(nx_window *w, nx_event *event, int *available) {
    if (!event || !available) return NX_ERROR_ARGUMENT;
    *event = (nx_event){0}; *available = 0;
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    return nx_platform_event_queue_next(&w->events,event,available);
}
nx_result nx_window_route_events(nx_window *w, nx_context *context, size_t *count) {
    if (!count) return NX_ERROR_ARGUMENT;
    *count = 0;
    if (!w || !context) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    return nx_platform_event_queue_route(&w->events,context,count);
}
nx_result nx_window_next_file_drop(nx_window *w, char *path, size_t capacity, size_t *length, nx_file_drop_info *info, int *available) {
    if (!length || !info || !available || (!path && capacity)) return NX_ERROR_ARGUMENT;
    *length = 0; *info = (nx_file_drop_info){0}; *available = 0;
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    return nx_platform_drop_queue_next(&w->drops,path,capacity,length,info,available);
}
nx_result nx_window_set_cursor(nx_window *w, nx_cursor_shape shape) {
    if (!w || shape < NX_CURSOR_DEFAULT || shape > NX_CURSOR_BUSY) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool { w->cursor = shape; [w->native invalidateCursorRectsForView:w->view]; return NX_OK; }
}
nx_result nx_window_get_reduced_motion(nx_window *w, int *enabled) {
    if (!enabled) return NX_ERROR_ARGUMENT;
    *enabled = 0;
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool { *enabled = [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion]; return NX_OK; }
}
nx_result nx_window_clipboard_set_text(nx_window *w, const char *bytes, size_t length) {
    if (!w || (!bytes && length)) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    if (length > 1048576u) return NX_ERROR_LIMIT;
    if ((bytes && memchr(bytes,0,length)) || nx_utf8_validate(bytes,length,NULL) != NX_OK) return NX_ERROR_ARGUMENT;
    @autoreleasepool {
        NSString *text = [[[NSString alloc] initWithBytes:bytes ? bytes : "" length:length encoding:NSUTF8StringEncoding] autorelease];
        if (!text) return NX_ERROR_ARGUMENT;
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard]; [pasteboard clearContents];
        return [pasteboard setString:text forType:NSPasteboardTypeString] ? NX_OK : NX_ERROR_PLATFORM;
    }
}
nx_result nx_window_clipboard_get_text(nx_window *w, char *bytes, size_t capacity, size_t *length) {
    if (!length || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *length = 0;
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool {
        NSString *text = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (!text) return NX_ERROR_STATE;
        NSData *data = [text dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
        if (!data || [data length] > 1048576u || ([data length] && memchr([data bytes],0,[data length]))) return NX_ERROR_LIMIT;
        *length = (size_t)[data length];
        if (!bytes) return NX_OK;
        if (capacity <= *length) return NX_ERROR_LIMIT;
        if (*length) memcpy(bytes,[data bytes],*length); bytes[*length] = 0; return NX_OK;
    }
}
nx_result nx_window_set_ime_rect(nx_window *w, nx_rect caret) {
    if (!w || !isfinite(caret.x) || !isfinite(caret.y) || !isfinite(caret.width) || !isfinite(caret.height) ||
        caret.width < 0 || caret.height < 0) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool { w->caret = caret; [[w->view inputContext] invalidateCharacterCoordinates]; return NX_OK; }
}
nx_result nx_window_set_ime_enabled(nx_window *w, int enabled) {
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool { w->ime_enabled = !!enabled; if (!enabled) nx_mac_discard(w); return NX_OK; }
}
nx_result nx_window_set_ime_purpose(nx_window *w, nx_ime_purpose purpose) {
    if (!w || (purpose != NX_IME_PURPOSE_NORMAL && purpose != NX_IME_PURPOSE_PASSWORD)) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    @autoreleasepool {
        if (purpose != w->ime_purpose) nx_mac_discard(w);
        w->ime_purpose = purpose;
        return purpose == NX_IME_PURPOSE_NORMAL ? NX_OK : NX_ERROR_UNSUPPORTED;
    }
}
nx_result nx_window_get_ime_preedit(nx_window *w, char *bytes, size_t capacity, size_t *length, int32_t *begin, int32_t *end) {
    if (!length || !begin || !end || (!bytes && capacity)) return NX_ERROR_ARGUMENT;
    *length = 0; *begin = *end = 0;
    if (!w) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w)) return NX_ERROR_STATE;
    *length = w->preedit_length; *begin = w->preedit_begin; *end = w->preedit_end;
    if (!bytes) return NX_OK;
    if (capacity <= *length) return NX_ERROR_LIMIT;
    if (*length) memcpy(bytes,w->preedit,*length); bytes[*length] = 0; return NX_OK;
}
nx_result nx_window_set_ime_surrounding_text(nx_window *w, uint64_t target, uint64_t revision,
    const char *bytes, size_t length, size_t cursor, size_t anchor) {
    (void)target; (void)revision; (void)bytes; (void)length; (void)cursor; (void)anchor;
    if (!w) return NX_ERROR_ARGUMENT;
    return nx_mac_owner(w) ? NX_ERROR_UNSUPPORTED : NX_ERROR_STATE;
}
NSWindow *nx_window_native_nswindow(nx_window *w) { return nx_mac_owner(w) ? w->native : nil; }
NSView *nx_window_native_nsview(nx_window *w) { return nx_mac_owner(w) ? w->view : nil; }
nx_result nx_window_renderer_acquire(nx_window *w, const void *owner) {
    if (!w || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w) || w->renderer_owner) return NX_ERROR_STATE;
    w->renderer_owner = owner; return NX_OK;
}
nx_result nx_window_renderer_release(nx_window *w, const void *owner) {
    if (!w || !owner) return NX_ERROR_ARGUMENT;
    if (!nx_mac_owner(w) || w->renderer_owner != owner) return NX_ERROR_STATE;
    @autoreleasepool { w->renderer_owner = NULL; [w->view setNeedsDisplay:YES]; return NX_OK; }
}
void nx_window_destroy(nx_window *w) {
    if (!nx_mac_owner(w) || w->renderer_owner) return;
    @autoreleasepool {
        nx_window **slot = &nx_macos_windows;
        while (*slot && *slot != w) slot = &(*slot)->next;
        if (*slot) *slot = w->next;
        if (w->view && w->view->owner == w) nx_mac_discard(w);
        if (w->view) w->view->owner = NULL;
        if (w->delegate) w->delegate->owner = NULL;
        [w->native setDelegate:nil]; [w->native setContentView:nil]; [w->native close];
        [w->view release]; [w->delegate release]; [w->native release];
        if (w->preedit) w->allocator.deallocate(w->allocator.user,w->preedit,w->preedit_length+1u);
        nx_platform_drop_queue_destroy(&w->drops); nx_platform_event_queue_destroy(&w->events);
        nx_allocator a = w->allocator; a.deallocate(a.user,w,sizeof(*w));
        if (!nx_macos_windows && nx_macos_application_delegate) {
            if ([NSApp delegate] == nx_macos_application_delegate) [NSApp setDelegate:nil];
            [nx_macos_application_delegate release]; nx_macos_application_delegate = nil;
        }
    }
}

nx_result nx_window_publish_semantics(nx_window *window, const nx_context *context) {
    (void)context;
    return window ? NX_ERROR_UNSUPPORTED : NX_ERROR_ARGUMENT;
}
