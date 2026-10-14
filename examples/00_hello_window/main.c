#include "nexaui/nexaui.h"

#include <stdio.h>
#include <string.h>

static int run(int smoke) {
    nx_window_desc desc = {0};
    desc.title_utf8 = "NexaUI 00: Hello window";
    desc.width = 640;
    desc.height = 360;
    desc.flags = smoke ? NX_WINDOW_HIDDEN : 0;
    nx_window *window = NULL;
    if (nx_window_create(&desc, &window) != NX_OK) return 0;

    int okay = 1;
    unsigned polls = 0;
    float last_width = 0, last_height = 0, last_scale = 0;
    while (okay) {
        if (nx_window_poll(window, smoke ? 0u : 16u) != NX_OK) {
            okay = 0;
            break;
        }
        nx_window_info info;
        if (nx_window_get_info(window, &info) != NX_OK ||
            info.scale <= 0 || info.width < 0 || info.height < 0 ||
            (smoke && (info.width == 0 || info.height == 0))) {
            okay = 0;
            break;
        }
        if (info.width != last_width || info.height != last_height ||
            info.scale != last_scale) {
            printf("client %.0f x %.0f logical pixels, scale %.2f\n",
                   info.width, info.height, info.scale);
            last_width = info.width;
            last_height = info.height;
            last_scale = info.scale;
        }
        nx_event event;
        int available = 0;
        do {
            if (nx_window_next_event(window, &event, &available) != NX_OK) {
                okay = 0;
                break;
            }
            if (!smoke && available) {
                if (event.type == NX_EVENT_POINTER_DOWN)
                    printf("pointer down at %.1f, %.1f\n", event.x, event.y);
                else if (event.type == NX_EVENT_KEY_DOWN)
                    printf("key down %d\n", (int)event.key);
                else if (event.type == NX_EVENT_TEXT)
                    printf("text U+%04X\n", event.scalar);
            }
        } while (available);
        if (info.should_close || (smoke && ++polls == 3u)) break;
    }
    nx_window_destroy(window);
    return okay;
}

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--smoke") != 0)) {
        fputs("usage: nexaui_00_hello_window [--smoke]\n", stderr);
        return 2;
    }
    if (!run(argc == 2)) {
        fputs("hello window failed\n", stderr);
        return 1;
    }
    return 0;
}
