#ifndef NEXAUI_MACOS_ACCESSIBILITY_H
#define NEXAUI_MACOS_ACCESSIBILITY_H
#import <AppKit/AppKit.h>
#include "nexaui/nexaui.h"

/* Private AppKit transport. Every entry point and native callback is main-thread
   only. create returns +1; destroy retires objects and releases that reference. */
@class NXMacAXBridge;
NXMacAXBridge *nx_mac_ax_create(NSView *view, nx_allocator allocator);
void nx_mac_ax_destroy(NXMacAXBridge *bridge);
nx_result nx_mac_ax_publish(NXMacAXBridge *bridge, const nx_context *context);
int nx_mac_ax_matches(NXMacAXBridge *bridge, const nx_context *context);
nx_result nx_mac_ax_route(NXMacAXBridge *bridge, nx_context *context, size_t *count);
NSArray *nx_mac_ax_children(NXMacAXBridge *bridge);
id nx_mac_ax_hit(NXMacAXBridge *bridge, NSPoint point);
id nx_mac_ax_focus(NXMacAXBridge *bridge);
#endif
