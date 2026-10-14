#define _POSIX_C_SOURCE 200809L
#include "nexaui/nexaui.h"
#include "window_internal.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <locale.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct allocation_probe {
    unsigned calls, live, fail_on;
} allocation_probe;

static void *probe_allocate(void *user, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    if (++probe->calls == probe->fail_on) return NULL;
    void *pointer = malloc(size);
    if (pointer) ++probe->live;
    return pointer;
}

static void probe_deallocate(void *user, void *pointer, size_t size) {
    allocation_probe *probe = (allocation_probe *)user;
    (void)size;
    if (pointer) --probe->live;
    free(pointer);
}

static int next(nx_window *window, nx_event_type type,
                nx_event *out_event) {
    int available = 0;
    CHECK(nx_window_next_event(window, out_event, &available) == NX_OK);
    CHECK(available && out_event->type == type);
    return 0;
}

static int empty(nx_window *window) {
    nx_event event;
    int available = 1;
    CHECK(nx_window_next_event(window, &event, &available) == NX_OK &&
          !available);
    return 0;
}

static int drain(nx_window *window) {
    nx_event event;
    int available;
    do {
        CHECK(nx_window_next_event(window, &event, &available) == NX_OK);
    } while (available);
    return 0;
}

static int test_ime_preedit(nx_window *window) {
    wchar_t initial[] = {L'A', 0x00e9, 0x1f600};
    XIMText text = {0};
    text.length = 3;
    text.encoding_is_wchar = True;
    text.string.wide_char = initial;
    CHECK(nx_window_x11_test_ime_edit(window, &text, 0, 0, 3) == NX_OK);
    nx_event event;
    CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
    size_t length = 0;
    int32_t begin = -2, end = -2;
    CHECK(nx_window_get_ime_preedit(window, NULL, 0, &length,
                                    &begin, &end) == NX_OK &&
          length == 7 && begin == 7 && end == 7);
    char bytes[32] = "unchanged";
    CHECK(nx_window_get_ime_preedit(window, bytes, length, &length,
                                    &begin, &end) == NX_ERROR_LIMIT &&
          !strcmp(bytes, "unchanged"));
    CHECK(nx_window_get_ime_preedit(window, bytes, sizeof(bytes), &length,
                                    &begin, &end) == NX_OK &&
          !strcmp(bytes, "A\xc3\xa9\xf0\x9f\x98\x80"));
    wchar_t replacement[] = {0x00df};
    text.length = 1;
    text.string.wide_char = replacement;
    CHECK(nx_window_x11_test_ime_edit(window, &text, 1, 1, 2) == NX_OK);
    CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
    CHECK(nx_window_get_ime_preedit(window, bytes, sizeof(bytes), &length,
                                    &begin, &end) == NX_OK &&
          !strcmp(bytes, "A\xc3\x9f\xf0\x9f\x98\x80") &&
          begin == 3 && end == 3);
    CHECK(nx_window_x11_test_ime_edit(window, &text, 4, 0, 2) ==
          NX_ERROR_ARGUMENT);
    wchar_t surrogate[] = {0xd800};
    text.string.wide_char = surrogate;
    CHECK(nx_window_x11_test_ime_edit(window, &text, 0, 1, 1) ==
          NX_ERROR_ARGUMENT);
    if (MB_CUR_MAX > 1) {
        XIMText multi = {0};
        multi.length = 1;
        multi.string.multi_byte = "\xc3\xa9";
        CHECK(nx_window_x11_test_ime_edit(window, &multi, 0, 1, 1) == NX_OK);
        CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
        CHECK(nx_window_get_ime_preedit(window, bytes, sizeof(bytes),
              &length, &begin, &end) == NX_OK &&
              !strcmp(bytes, "\xc3\xa9\xc3\x9f\xf0\x9f\x98\x80") &&
              begin == 2 && end == 2);
    }
    CHECK(empty(window) == 0);
    CHECK(nx_window_x11_test_ime_clear(window) == NX_OK);
    CHECK(next(window, NX_EVENT_IME_PREEDIT_CHANGED, &event) == 0);
    nx_result result = nx_window_get_ime_preedit(window, bytes,
        sizeof(bytes), &length, &begin, &end);
    CHECK(result == NX_ERROR_UNSUPPORTED ||
          (result == NX_OK && length == 0 && begin == 0 && end == 0));
    CHECK(empty(window) == 0);
    return 0;
}

static int send_pointer(Display *display, Window xid, int type,
                        int x, int y, unsigned int button) {
    XEvent event;
    memset(&event, 0, sizeof(event));
    long mask;
    if (type == MotionNotify) {
        event.xmotion.type = MotionNotify;
        event.xmotion.display = display;
        event.xmotion.window = xid;
        event.xmotion.x = x;
        event.xmotion.y = y;
        event.xmotion.same_screen = True;
        mask = PointerMotionMask;
    } else {
        event.xbutton.type = type;
        event.xbutton.display = display;
        event.xbutton.window = xid;
        event.xbutton.x = x;
        event.xbutton.y = y;
        event.xbutton.button = button;
        event.xbutton.same_screen = True;
        mask = type == ButtonPress ? ButtonPressMask : ButtonReleaseMask;
    }
    return XSendEvent(display, xid, False, mask, &event) != 0;
}

static int send_key(Display *display, Window xid, int type, KeySym sym) {
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xkey.type = type;
    event.xkey.display = display;
    event.xkey.window = xid;
    event.xkey.keycode = XKeysymToKeycode(display, sym);
    event.xkey.same_screen = True;
    CHECK(event.xkey.keycode != 0);
    return XSendEvent(display, xid, False,
                      type == KeyPress ? KeyPressMask : KeyReleaseMask,
                      &event) != 0;
}

typedef struct cross_thread_case {
    nx_window *window;
    nx_result poll_result, cursor_result;
} cross_thread_case;

typedef struct incremental_owner_case {
    Display *display;
    Window owner;
    Atom clipboard, utf8, incr;
    int served;
} incremental_owner_case;

static void *incremental_owner(void *user) {
    incremental_owner_case *test = user;
    Window requestor = None;
    Atom property = None;
    int stage = -1;
    for (int attempt = 0; attempt < 500; ++attempt) {
        if (!XPending(test->display)) {
            struct timespec delay = {0, 10000000};
            nanosleep(&delay, NULL);
            continue;
        }
        XEvent event;
        XNextEvent(test->display, &event);
        if (event.type == SelectionRequest && stage < 0) {
            XSelectionRequestEvent *request = &event.xselectionrequest;
            if (request->selection != test->clipboard ||
                request->target != test->utf8) return NULL;
            requestor = request->requestor;
            property = request->property;
            XSelectInput(test->display, requestor, PropertyChangeMask);
            unsigned long size = 4;
            XChangeProperty(test->display, requestor, property,
                            test->incr, 32, PropModeReplace,
                            (unsigned char *)&size, 1);
            XEvent reply = {0};
            reply.xselection.type = SelectionNotify;
            reply.xselection.display = test->display;
            reply.xselection.requestor = requestor;
            reply.xselection.selection = test->clipboard;
            reply.xselection.target = test->utf8;
            reply.xselection.property = property;
            reply.xselection.time = request->time;
            XSendEvent(test->display, requestor, False, NoEventMask, &reply);
            XFlush(test->display);
            stage = 0;
        } else if (event.type == PropertyNotify && stage >= 0 &&
                   event.xproperty.window == requestor &&
                   event.xproperty.atom == property &&
                   event.xproperty.state == PropertyDelete) {
            static const unsigned char first[] = {'A', '\n', 0xc3};
            static const unsigned char second[] = {0xa9};
            const unsigned char *chunk = stage == 0 ? first :
                                         stage == 1 ? second : NULL;
            int length = stage == 0 ? 3 : stage == 1 ? 1 : 0;
            if (stage == 3) { test->served = 1; return NULL; }
            XChangeProperty(test->display, requestor, property,
                            test->utf8, 8, PropModeReplace, chunk, length);
            XFlush(test->display);
            ++stage;
        }
    }
    return NULL;
}

static int test_incremental_clipboard(nx_window *requestor) {
    Display *external = XOpenDisplay(NULL);
    CHECK(external != NULL);
    incremental_owner_case owner = {0};
    owner.display = external;
    owner.owner = XCreateSimpleWindow(external, DefaultRootWindow(external),
                                      0, 0, 1, 1, 0, 0, 0);
    owner.clipboard = XInternAtom(external, "CLIPBOARD", False);
    owner.utf8 = XInternAtom(external, "UTF8_STRING", False);
    owner.incr = XInternAtom(external, "INCR", False);
    CHECK(owner.owner != None && owner.clipboard != None &&
          owner.utf8 != None && owner.incr != None);
    XSetSelectionOwner(external, owner.clipboard, owner.owner, CurrentTime);
    CHECK(XGetSelectionOwner(external, owner.clipboard) == owner.owner);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, incremental_owner, &owner) == 0);
    char bytes[16] = {0};
    size_t length = 0;
    nx_result result = nx_window_clipboard_get_text(requestor,
                                                     bytes, sizeof(bytes),
                                                     &length);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(result == NX_OK && owner.served && length == 4 &&
          !memcmp(bytes, "A\n\xc3\xa9", 5));
    XDestroyWindow(external, owner.owner);
    XCloseDisplay(external);
    return 0;
}

static void *wrong_thread(void *user) {
    cross_thread_case *test = (cross_thread_case *)user;
    test->poll_result = nx_window_poll(test->window, 0);
    test->cursor_result = nx_window_set_cursor(test->window, NX_CURSOR_LINK);
    return NULL;
}

typedef struct xdnd_source_case {
    Display *display;
    Window source, target;
    Atom status, drop, finished, selection, uri, incr, copy;
    const char *payload;
    size_t length;
    int incremental, supported;
    int status_seen, served, success, bad;
    atomic_int done;
} xdnd_source_case;

static void *xdnd_source_thread(void *user) {
    xdnd_source_case *test = user;
    Window requestor = None;
    Atom property = None;
    int stage = -1;
    for (int attempt = 0; attempt < 700; ++attempt) {
        if (!XPending(test->display)) {
            struct timespec delay = {0, 10000000};
            nanosleep(&delay, NULL);
            continue;
        }
        XEvent event;
        XNextEvent(test->display, &event);
        if (event.type == ClientMessage &&
            event.xclient.message_type == test->status) {
            ++test->status_seen;
            if (((event.xclient.data.l[1] & 1L) != 0) != test->supported ||
                (test->supported && (Atom)event.xclient.data.l[4] !=
                    test->copy)) test->bad = 1;
            XEvent message = {0};
            message.xclient.type = ClientMessage;
            message.xclient.display = test->display;
            message.xclient.window = test->target;
            message.xclient.message_type = test->drop;
            message.xclient.format = 32;
            message.xclient.data.l[0] = (long)test->source;
            message.xclient.data.l[2] = CurrentTime;
            XSendEvent(test->display, test->target, False, NoEventMask,
                       &message);
            XFlush(test->display);
        } else if (event.type == SelectionRequest) {
            XSelectionRequestEvent *request = &event.xselectionrequest;
            if (!test->supported || request->selection != test->selection ||
                request->target != test->uri) test->bad = 1;
            requestor = request->requestor;
            property = request->property != None ? request->property :
                       request->target;
            if (test->incremental) {
                XSelectInput(test->display, requestor, PropertyChangeMask);
                unsigned long announced = (unsigned long)test->length;
                XChangeProperty(test->display, requestor, property,
                                test->incr, 32, PropModeReplace,
                                (const unsigned char *)&announced, 1);
                stage = 0;
            } else {
                XChangeProperty(test->display, requestor, property,
                                test->uri, 8, PropModeReplace,
                                (const unsigned char *)test->payload,
                                (int)test->length);
                test->served = 1;
            }
            XEvent reply = {0};
            reply.xselection.type = SelectionNotify;
            reply.xselection.display = test->display;
            reply.xselection.requestor = requestor;
            reply.xselection.selection = request->selection;
            reply.xselection.target = request->target;
            reply.xselection.property = property;
            reply.xselection.time = request->time;
            XSendEvent(test->display, requestor, False, NoEventMask, &reply);
            XFlush(test->display);
        } else if (event.type == PropertyNotify && stage >= 0 &&
                   event.xproperty.window == requestor &&
                   event.xproperty.atom == property &&
                   event.xproperty.state == PropertyDelete) {
            size_t split = test->length / 2u;
            const unsigned char *chunk = stage == 0 ?
                (const unsigned char *)test->payload :
                stage == 1 ? (const unsigned char *)test->payload + split :
                NULL;
            int count = stage == 0 ? (int)split :
                        stage == 1 ? (int)(test->length - split) : 0;
            if (stage <= 2) {
                XChangeProperty(test->display, requestor, property,
                                test->uri, 8, PropModeReplace, chunk, count);
                XFlush(test->display);
                ++stage;
                if (stage == 3) test->served = 1;
            }
        } else if (event.type == ClientMessage &&
                   event.xclient.message_type == test->finished) {
            test->success = (event.xclient.data.l[1] & 1L) != 0;
            atomic_store(&test->done, 1);
            return NULL;
        }
    }
    test->bad = 1;
    atomic_store(&test->done, 1);
    return NULL;
}

static int test_xdnd(nx_window *target, int many_types,
                     int incremental, int supported) {
    Display *target_display = nx_window_x11_display(target);
    Window target_xid = nx_window_x11_id(target);
    Atom aware = XInternAtom(target_display, "XdndAware", True);
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *property_data = NULL;
    CHECK(aware != None && XGetWindowProperty(target_display, target_xid,
          aware, 0, 1, False, XA_ATOM, &actual, &format, &count,
          &after, &property_data) == Success && actual == XA_ATOM &&
          format == 32 && count == 1 &&
          ((unsigned long *)property_data)[0] == 5);
    XFree(property_data);
    Display *external = XOpenDisplay(NULL);
    CHECK(external != NULL);
    xdnd_source_case test = {0};
    test.display = external;
    test.target = target_xid;
    test.source = XCreateSimpleWindow(external,
        DefaultRootWindow(external), 0, 0, 1, 1, 0, 0, 0);
    test.status = XInternAtom(external, "XdndStatus", False);
    test.drop = XInternAtom(external, "XdndDrop", False);
    test.finished = XInternAtom(external, "XdndFinished", False);
    test.selection = XInternAtom(external, "XdndSelection", False);
    test.uri = XInternAtom(external, "text/uri-list", False);
    test.incr = XInternAtom(external, "INCR", False);
    test.copy = XInternAtom(external, "XdndActionCopy", False);
    Atom enter = XInternAtom(external, "XdndEnter", False);
    Atom position = XInternAtom(external, "XdndPosition", False);
    Atom type_list = XInternAtom(external, "XdndTypeList", False);
    Atom plain = XInternAtom(external, "text/plain", False);
    test.payload = "file:///tmp/one%20file\r\n"
                   "file:///tmp/caf%C3%A9\r\n";
    test.length = strlen(test.payload);
    char hosted_payload[512];
    if (supported && !incremental) {
        char hostname[256] = {0};
        CHECK(gethostname(hostname, sizeof(hostname)) == 0);
        hostname[sizeof(hostname) - 1u] = '\0';
        int written = snprintf(hosted_payload, sizeof(hosted_payload),
            "file://%s/tmp/one%%20file\r\n"
            "file:///tmp/caf%%C3%%A9\r\n", hostname);
        CHECK(written > 0 && (size_t)written < sizeof(hosted_payload));
        test.payload = hosted_payload;
        test.length = (size_t)written;
    }
    char *bulk = NULL;
    size_t expected_count = 2;
    if (incremental) {
        const char line[] = "file:///tmp/item\n";
        expected_count = 5000;
        test.length = expected_count * (sizeof(line) - 1u);
        bulk = malloc(test.length);
        CHECK(bulk != NULL);
        for (size_t i = 0; i < expected_count; ++i)
            memcpy(bulk + i * (sizeof(line) - 1u), line,
                   sizeof(line) - 1u);
        test.payload = bulk;
    }
    test.incremental = incremental;
    test.supported = supported;
    CHECK(test.source != None);
    XSetSelectionOwner(external, test.selection, test.source, CurrentTime);
    CHECK(XGetSelectionOwner(external, test.selection) == test.source);
    if (many_types) {
        Atom types[] = {plain, XA_STRING, XA_ATOM,
                        supported ? test.uri : None};
        XChangeProperty(external, test.source, type_list, XA_ATOM, 32,
                        PropModeReplace, (const unsigned char *)types, 4);
    }
    XEvent message = {0};
    message.xclient.type = ClientMessage;
    message.xclient.display = external;
    message.xclient.window = target_xid;
    message.xclient.message_type = enter;
    message.xclient.format = 32;
    message.xclient.data.l[0] = (long)test.source;
    message.xclient.data.l[1] = (5L << 24) | (many_types ? 1L : 0L);
    message.xclient.data.l[2] = (long)(many_types ? plain :
                                         supported ? test.uri : plain);
    message.xclient.data.l[3] = (long)(many_types ? XA_STRING : None);
    message.xclient.data.l[4] = (long)(many_types ? XA_ATOM : None);
    XSendEvent(external, target_xid, False, NoEventMask, &message);
    int root_x = 0, root_y = 0;
    Window child = None;
    CHECK(XTranslateCoordinates(external, target_xid,
          DefaultRootWindow(external), 15, 25,
          &root_x, &root_y, &child));
    message.xclient.message_type = position;
    message.xclient.data.l[1] = 0;
    message.xclient.data.l[2] =
        (long)(((uint32_t)(uint16_t)root_x << 16) |
               (uint16_t)root_y);
    message.xclient.data.l[3] = CurrentTime;
    message.xclient.data.l[4] = (long)test.copy;
    XSendEvent(external, target_xid, False, NoEventMask, &message);
    XFlush(external);
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, xdnd_source_thread, &test) == 0);
    for (int attempt = 0; attempt < 250 && !atomic_load(&test.done);
         ++attempt)
        CHECK(nx_window_poll(target, 20) == NX_OK);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(atomic_load(&test.done) && !test.bad && test.status_seen == 1 &&
          test.served == supported && test.success == supported);
    size_t length = 0;
    nx_file_drop_info info = {0};
    int available = 0;
    CHECK(nx_window_next_file_drop(target, NULL, 0, &length, &info,
                                    &available) == NX_OK &&
          available == supported);
    if (supported) {
        char path[64];
        CHECK(info.index == 0 && info.count == expected_count &&
              info.x == 15.0f && info.y == 25.0f);
        CHECK(nx_window_next_file_drop(target, path, length, &length,
              &info, &available) == NX_ERROR_LIMIT && available);
        for (size_t i = 0; i < expected_count; ++i) {
            CHECK(nx_window_next_file_drop(target, path, sizeof(path),
                  &length, &info, &available) == NX_OK && available &&
                  info.index == i && info.count == expected_count);
            if (incremental) CHECK(!strcmp(path, "/tmp/item"));
            else if (i == 0) CHECK(!strcmp(path, "/tmp/one file"));
            else CHECK(!strcmp(path, "/tmp/caf\xc3\xa9"));
        }
    }
    free(bulk);
    XDestroyWindow(external, test.source);
    XCloseDisplay(external);
    return 0;
}

static int test_clipboard(nx_window *first, nx_window *second) {
    const char text[] = "A\n\xc3\xa9";
    size_t length = 99;
    char result[32] = "untouched";
    CHECK(nx_window_clipboard_set_text(first, "\xc0", 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_clipboard_set_text(first, "a\0b", 3) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_clipboard_get_text(first, NULL, 0, &length) ==
          NX_ERROR_STATE && length == 0);
    CHECK(nx_window_clipboard_set_text(first, text, sizeof(text) - 1u) ==
          NX_OK);
    Display *external = XOpenDisplay(NULL);
    CHECK(external != NULL);
    Window requestor = XCreateSimpleWindow(external,
        DefaultRootWindow(external), 0, 0, 1, 1, 0, 0, 0);
    Atom clipboard = XInternAtom(external, "CLIPBOARD", False);
    Atom targets = XInternAtom(external, "TARGETS", False);
    Atom utf8 = XInternAtom(external, "UTF8_STRING", False);
    Atom property = XInternAtom(external, "NEXAUI_TEST_TARGETS", False);
    CHECK(requestor != None && clipboard != None && targets != None &&
          utf8 != None && property != None);
    XConvertSelection(external, clipboard, targets, property,
                      requestor, CurrentTime);
    XFlush(external);
    for (int attempt = 0; attempt < 80 && !XPending(external); ++attempt)
        CHECK(nx_window_poll(first, 25) == NX_OK);
    CHECK(XPending(external) != 0);
    XEvent reply;
    XNextEvent(external, &reply);
    CHECK(reply.type == SelectionNotify &&
          reply.xselection.property == property);
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *offered = NULL;
    CHECK(XGetWindowProperty(external, requestor, property, 0, 16, True,
          XA_ATOM, &actual, &format, &count, &after, &offered) == Success &&
          actual == XA_ATOM && format == 32 && count == 2 && after == 0);
    Atom *offered_atoms = (Atom *)offered;
    CHECK(offered_atoms[0] == targets && offered_atoms[1] == utf8);
    XFree(offered);
    XDestroyWindow(external, requestor);
    XCloseDisplay(external);
    CHECK(nx_window_clipboard_get_text(first, NULL, 0, &length) == NX_OK &&
          length == sizeof(text) - 1u);
    CHECK(nx_window_clipboard_get_text(second, result, length, &length) ==
          NX_ERROR_LIMIT && length == sizeof(text) - 1u &&
          !strcmp(result, "untouched"));
    CHECK(nx_window_clipboard_get_text(second, result, sizeof(result),
                                       &length) == NX_OK &&
          length == sizeof(text) - 1u &&
          !memcmp(result, text, sizeof(text)));
    CHECK(nx_window_clipboard_set_text(first, "\xc0", 1) ==
          NX_ERROR_ARGUMENT);
    CHECK(nx_window_clipboard_get_text(second, result, sizeof(result),
                                       &length) == NX_OK &&
          length == sizeof(text) - 1u &&
          !memcmp(result, text, sizeof(text)));
    nx_text_clipboard adapter = {0};
    CHECK(nx_text_clipboard_for_window(second, &adapter) == NX_OK &&
          adapter.read && adapter.write);
    CHECK(adapter.read(adapter.user, result, sizeof(result), &length) ==
          NX_OK && !memcmp(result, text, sizeof(text)));
    CHECK(nx_window_clipboard_set_text(second, "second", 6) == NX_OK);
    CHECK(nx_window_clipboard_get_text(first, result, sizeof(result),
                                       &length) == NX_OK &&
          length == 6 && !strcmp(result, "second"));
    CHECK(nx_window_clipboard_set_text(second, NULL, 0) == NX_OK);
    CHECK(nx_window_clipboard_get_text(first, result, sizeof(result),
                                       &length) == NX_OK &&
          length == 0 && result[0] == '\0');
    CHECK(nx_window_clipboard_set_text(second, "second", 6) == NX_OK);
    allocation_probe probe = {0};
    nx_window_desc desc = {0};
    desc.width = desc.height = 40;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&probe, probe_allocate,
                                    probe_deallocate};
    nx_window *failing = NULL;
    CHECK(nx_window_create(&desc, &failing) == NX_OK && failing);
    probe.fail_on = probe.calls + 1u;
    CHECK(nx_window_clipboard_set_text(failing, "rejected", 8) ==
          NX_ERROR_MEMORY);
    CHECK(nx_window_clipboard_get_text(first, result, sizeof(result),
                                       &length) == NX_OK &&
          length == 6 && !strcmp(result, "second"));
    nx_window_destroy(failing);
    CHECK(probe.live == 0);
    return 0;
}

static int test_large_clipboard(nx_window *owner, nx_window *reader) {
    Display *display = nx_window_x11_display(owner);
    long units = XMaxRequestSize(display);
    CHECK(units > 64 && (size_t)(units - 64) * 4u <
          32u * 1024u * 1024u - 8192u);
    size_t length = (size_t)(units - 64) * 4u + 8192u;
    char *source = malloc(length);
    char *received = malloc(length + 1u);
    CHECK(source && received);
    for (size_t i = 0; i < length; ++i)
        source[i] = (char)('a' + i % 26u);
    CHECK(nx_window_clipboard_set_text(owner, source, length) == NX_OK);
    size_t needed = 0;
    CHECK(nx_window_clipboard_get_text(reader, NULL, 0, &needed) == NX_OK &&
          needed == length);
    CHECK(nx_window_clipboard_get_text(reader, received, length + 1u,
                                       &needed) == NX_OK &&
          needed == length && !memcmp(received, source, length) &&
          received[length] == '\0');

    Display *external = XOpenDisplay(NULL);
    CHECK(external != NULL);
    Window requestor = XCreateSimpleWindow(external,
        DefaultRootWindow(external), 0, 0, 1, 1, 0, 0, 0);
    Atom clipboard = XInternAtom(external, "CLIPBOARD", False);
    Atom utf8 = XInternAtom(external, "UTF8_STRING", False);
    Atom incr = XInternAtom(external, "INCR", False);
    Atom property = XInternAtom(external, "NEXAUI_TEST_LARGE", False);
    CHECK(requestor != None && clipboard != None && utf8 != None &&
          incr != None && property != None);
    XSelectInput(external, requestor, PropertyChangeMask);
    XConvertSelection(external, clipboard, utf8, property,
                      requestor, CurrentTime);
    XFlush(external);
    int notified = 0;
    for (int attempt = 0; attempt < 500 && !notified; ++attempt) {
        CHECK(nx_window_poll(owner, 10) == NX_OK);
        while (XPending(external)) {
            XEvent event;
            XNextEvent(external, &event);
            if (event.type == SelectionNotify) {
                CHECK(event.xselection.property == property);
                notified = 1;
            }
        }
    }
    CHECK(notified);
    Atom actual = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *data = NULL;
    CHECK(XGetWindowProperty(external, requestor, property, 0, 1, False,
          AnyPropertyType, &actual, &format, &count, &after, &data) ==
          Success && actual == incr && format == 32 && count == 1 &&
          after == 0 && ((unsigned long *)data)[0] == length);
    XFree(data);
    XDeleteProperty(external, requestor, property);
    XFlush(external);
    size_t offset = 0;
    int chunks = 0, finished = 0;
    for (int attempt = 0; attempt < 800 && !finished; ++attempt) {
        CHECK(nx_window_poll(owner, 10) == NX_OK);
        while (XPending(external)) {
            XEvent event;
            XNextEvent(external, &event);
            if (event.type != PropertyNotify ||
                event.xproperty.atom != property ||
                event.xproperty.state != PropertyNewValue) continue;
            data = NULL;
            CHECK(XGetWindowProperty(external, requestor, property, 0,
                  (long)((length + 3u) / 4u), True, AnyPropertyType,
                  &actual, &format, &count, &after, &data) == Success &&
                  actual == utf8 && format == 8 && after == 0);
            if (count) {
                CHECK(count <= length - offset &&
                      !memcmp(data, source + offset, count));
                offset += count;
                ++chunks;
                if (chunks == 1)
                    CHECK(nx_window_clipboard_set_text(reader, "new", 3) ==
                          NX_OK);
            } else finished = 1;
            if (data) XFree(data);
        }
    }
    CHECK(finished && chunks >= 2 && offset == length);
    CHECK(nx_window_poll(owner, 0) == NX_OK);
    CHECK(nx_window_clipboard_get_text(owner, received, length + 1u,
                                       &needed) == NX_OK &&
          needed == 3 && !strcmp(received, "new"));
    allocation_probe probe = {0};
    nx_window_desc desc = {0};
    desc.width = desc.height = 40;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&probe, probe_allocate,
                                    probe_deallocate};
    nx_window *failing_owner = NULL;
    CHECK(nx_window_create(&desc, &failing_owner) == NX_OK && failing_owner);
    CHECK(nx_window_clipboard_set_text(failing_owner, source, length) == NX_OK);
    probe.fail_on = probe.calls + 1u;
    CHECK(nx_window_clipboard_get_text(reader, NULL, 0, &needed) ==
          NX_ERROR_STATE);
    probe.fail_on = probe.calls + 2u;
    CHECK(nx_window_clipboard_get_text(reader, NULL, 0, &needed) ==
          NX_ERROR_STATE);
    probe.fail_on = 0;
    CHECK(nx_window_clipboard_get_text(reader, NULL, 0, &needed) == NX_OK &&
          needed == length);
    nx_window_destroy(failing_owner);
    CHECK(probe.live == 0);
    XDestroyWindow(external, requestor);
    XCloseDisplay(external);
    free(received);
    free(source);
    return 0;
}

int main(void) {
    setlocale(LC_CTYPE, "");
    CHECK(setenv("NEXAUI_PLATFORM", "x11", 1) == 0);
    const char *display_name = getenv("DISPLAY");
    if (!display_name || !*display_name) {
        fputs("X11 display unavailable; native test skipped\n", stderr);
        return 77;
    }

    nx_window_desc desc = {0};
    desc.width = 240;
    desc.height = 120;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.title_utf8 = "NexaUI X11 caf\xc3\xa9";
    nx_window *first = NULL, *second = NULL;
    CHECK(nx_window_create(&desc, &first) == NX_OK);
    desc.flags = 0;
    desc.title_utf8 = "NexaUI X11 second";
    CHECK(nx_window_create(&desc, &second) == NX_OK);
    Display *display = nx_window_x11_display(first);
    Window xid = nx_window_x11_id(first);
    Display *second_display = nx_window_x11_display(second);
    Window second_xid = nx_window_x11_id(second);
    CHECK(display && second_display && display != second_display &&
          xid != None && second_xid != None);
    XSync(display, False);
    XSync(second_display, False);
    XWindowAttributes attrs;
    CHECK(XGetWindowAttributes(display, xid, &attrs) &&
          attrs.map_state == IsUnmapped && attrs.width == 240 &&
          attrs.height == 120);
    for (int attempt = 0; attempt < 80; ++attempt) {
        CHECK(XGetWindowAttributes(second_display, second_xid, &attrs));
        if (attrs.map_state == IsViewable) break;
        struct timespec delay = {0, 25000000};
        nanosleep(&delay, NULL);
    }
    CHECK(attrs.map_state == IsViewable);

    Atom net_name = XInternAtom(display, "_NET_WM_NAME", True);
    Atom utf8 = XInternAtom(display, "UTF8_STRING", True);
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0, bytes_after = 0;
    unsigned char *title = NULL;
    CHECK(net_name != None && utf8 != None &&
          XGetWindowProperty(display, xid, net_name, 0, 64, False,
                             AnyPropertyType, &actual_type, &actual_format,
                             &items, &bytes_after, &title) == Success);
    CHECK(actual_type == utf8 && actual_format == 8 &&
          items == strlen("NexaUI X11 caf\xc3\xa9") &&
          !memcmp(title, "NexaUI X11 caf\xc3\xa9", items));
    XFree(title);

    XResizeWindow(display, xid, 320, 200);
    XSync(display, False);
    nx_window_info info;
    for (int attempt = 0; attempt < 80; ++attempt) {
        CHECK(nx_window_poll(first, 25) == NX_OK);
        CHECK(nx_window_get_info(first, &info) == NX_OK);
        if (info.width == 320 && info.height == 200) break;
    }
    CHECK(info.width == 320 && info.height == 200 && info.scale == 1.0f);
    CHECK(XGetWindowAttributes(display, xid, &attrs) &&
          attrs.width == 320 && attrs.height == 200);
    nx_event event;
    CHECK(next(first, NX_EVENT_RESIZE, &event) == 0 &&
          event.width == 320 && event.height == 200 && event.scale == 1.0f);
    CHECK(empty(first) == 0);
    CHECK(nx_window_poll(second, 0) == NX_OK);
    CHECK(drain(second) == 0);

    CHECK(send_pointer(display, xid, MotionNotify, 10, 20, 0));
    CHECK(send_pointer(display, xid, MotionNotify, 12, 24, 0));
    CHECK(send_pointer(display, xid, ButtonPress, 12, 24, Button1));
    CHECK(send_pointer(display, xid, ButtonRelease, 12, 24, Button1));
    CHECK(send_pointer(display, xid, ButtonPress, 12, 24, Button4));
    XSync(display, False);
    CHECK(nx_window_poll(first, 0) == NX_OK);
    CHECK(next(first, NX_EVENT_POINTER_MOVE, &event) == 0 &&
          event.x == 12 && event.y == 24);
    CHECK(next(first, NX_EVENT_POINTER_DOWN, &event) == 0 &&
          event.button == NX_BUTTON_LEFT && event.x == 12);
    CHECK(next(first, NX_EVENT_POINTER_UP, &event) == 0 &&
          event.button == NX_BUTTON_LEFT);
    CHECK(next(first, NX_EVENT_WHEEL, &event) == 0 &&
          event.delta_y == 1.0f);
    CHECK(empty(first) == 0 && empty(second) == 0);

    CHECK(send_key(display, xid, KeyPress, XK_a));
    CHECK(send_key(display, xid, KeyPress, XK_a));
    CHECK(send_key(display, xid, KeyRelease, XK_a));
    XSync(display, False);
    CHECK(nx_window_poll(first, 0) == NX_OK);
    CHECK(next(first, NX_EVENT_KEY_DOWN, &event) == 0 &&
          event.key == NX_KEY_A && !event.repeat);
    CHECK(next(first, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'a');
    CHECK(next(first, NX_EVENT_KEY_DOWN, &event) == 0 &&
          event.key == NX_KEY_A && event.repeat);
    CHECK(next(first, NX_EVENT_TEXT, &event) == 0 && event.scalar == 'a');
    CHECK(next(first, NX_EVENT_KEY_UP, &event) == 0 && event.key == NX_KEY_A);
    CHECK(empty(first) == 0);

    XEvent focus;
    memset(&focus, 0, sizeof(focus));
    focus.xfocus.type = FocusIn;
    focus.xfocus.display = display;
    focus.xfocus.window = xid;
    focus.xfocus.mode = NotifyNormal;
    focus.xfocus.detail = NotifyNonlinear;
    CHECK(XSendEvent(display, xid, False, FocusChangeMask, &focus));
    focus.xfocus.type = FocusOut;
    CHECK(XSendEvent(display, xid, False, FocusChangeMask, &focus));
    XSync(display, False);
    CHECK(nx_window_poll(first, 0) == NX_OK);
    CHECK(next(first, NX_EVENT_FOCUS, &event) == 0 && event.focused);
    CHECK(next(first, NX_EVENT_FOCUS, &event) == 0 && !event.focused);

    XEvent close;
    memset(&close, 0, sizeof(close));
    close.xclient.type = ClientMessage;
    close.xclient.display = second_display;
    close.xclient.window = second_xid;
    close.xclient.message_type = XInternAtom(second_display, "WM_PROTOCOLS", False);
    close.xclient.format = 32;
    close.xclient.data.l[0] = (long)XInternAtom(second_display,
                                                "WM_DELETE_WINDOW", False);
    CHECK(XSendEvent(second_display, second_xid, False, NoEventMask, &close));
    XSync(second_display, False);
    CHECK(nx_window_poll(second, 0) == NX_OK);
    CHECK(nx_window_get_info(second, &info) == NX_OK && info.should_close);
    CHECK(nx_window_get_info(first, &info) == NX_OK && !info.should_close);

    for (int shape = NX_CURSOR_DEFAULT; shape <= NX_CURSOR_BUSY; ++shape)
        CHECK(nx_window_set_cursor(first, (nx_cursor_shape)shape) == NX_OK);
    XSync(display, False);
    CHECK(nx_window_set_cursor(first, (nx_cursor_shape)999) ==
          NX_ERROR_ARGUMENT);
    nx_rect ime_caret = {12.0f, 16.0f, 1.0f, 20.0f};
    nx_result ime_result = nx_window_set_ime_rect(first, ime_caret);
    CHECK(ime_result == NX_OK || ime_result == NX_ERROR_UNSUPPORTED);
    ime_caret.x = 40000.0f;
    CHECK(nx_window_set_ime_rect(first, ime_caret) ==
          (ime_result == NX_OK ? NX_ERROR_LIMIT : NX_ERROR_UNSUPPORTED));
    ime_caret.x = 12.0f;
    ime_caret.height = 0.0f;
    CHECK(nx_window_set_ime_rect(first, ime_caret) == NX_ERROR_ARGUMENT);
    nx_result enabled = nx_window_set_ime_enabled(first, 1);
    CHECK(enabled == NX_OK || enabled == NX_ERROR_UNSUPPORTED);
    CHECK(nx_window_set_ime_enabled(first, 0) == NX_OK);
    CHECK(test_ime_preedit(first) == 0);
    cross_thread_case wrong = {first, NX_OK, NX_OK};
    pthread_t thread;
    CHECK(pthread_create(&thread, NULL, wrong_thread, &wrong) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
    CHECK(wrong.poll_result == NX_ERROR_STATE &&
          wrong.cursor_result == NX_ERROR_STATE);
    const char *private_display = getenv("NEXAUI_TEST_PRIVATE_X11");
    if (private_display && !strcmp(private_display, "1"))
        CHECK(test_clipboard(first, second) == 0);
    if (private_display && !strcmp(private_display, "1"))
        CHECK(test_large_clipboard(first, second) == 0);
    if (private_display && !strcmp(private_display, "1"))
        CHECK(test_incremental_clipboard(first) == 0);
    if (private_display && !strcmp(private_display, "1")) {
        CHECK(test_xdnd(second, 0, 0, 1) == 0);
        CHECK(test_xdnd(second, 1, 1, 1) == 0);
        CHECK(test_xdnd(second, 0, 0, 0) == 0);
    }
    nx_window_destroy(second);
    if (private_display && !strcmp(private_display, "1")) {
        size_t missing_length = 99;
        CHECK(nx_window_clipboard_get_text(first, NULL, 0,
                                           &missing_length) == NX_ERROR_STATE &&
              missing_length == 0);
    }
    nx_window_destroy(first);

    allocation_probe allocation = {0};
    allocation.fail_on = 2;
    desc.flags = NX_WINDOW_HIDDEN;
    desc.allocator = (nx_allocator){&allocation, probe_allocate,
                                    probe_deallocate};
    nx_window *failed = NULL;
    CHECK(nx_window_create(&desc, &failed) == NX_ERROR_MEMORY &&
          failed == NULL && allocation.live == 0);
    puts("native X11 lifecycle, input, clipboard, drops and isolation verified");
    return 0;
}
