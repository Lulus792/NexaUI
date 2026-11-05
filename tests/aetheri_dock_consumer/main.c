#include "nexaui/nexaui.h"
#include "nexaui/software.h"
#include <stdio.h>
#include <unistd.h>

/* A public SDK consumer: the controller owns stdin and compositor lifecycle. */
int main(void) {
    nx_window_desc desc = {"NexaUI bounds probe", 2100, 1200, 0, {0}};
    nx_window *window = NULL;
    nx_software_view *view = NULL;
    int okay = nx_window_create(&desc, &window) == NX_OK &&
               nx_software_view_open(window, &view) == NX_OK;
    float previous_width = 0, previous_height = 0;
    int running = 1;
    while (okay && running) {
        uint32_t events = 0;
        nx_window_info info;
        if (nx_window_poll_with_fd(window, 1000, STDIN_FILENO, &events) != NX_OK ||
            nx_window_get_info(window, &info) != NX_OK || info.should_close) {
            okay = 0; break;
        }
        if (info.width != previous_width || info.height != previous_height) {
            nx_pixel_size pixels;
            nx_software_view_outcome outcome;
            nx_color color = {0.2f, 0.4f, 0.8f, 1.0f};
            if (nx_window_get_pixel_size(window, &pixels) != NX_OK ||
                nx_software_view_render(view, NULL, 0, color, &outcome) != NX_OK ||
                outcome != NX_SOFTWARE_VIEW_READY) { okay = 0; break; }
            printf("FRAME %.0f %.0f %u %u\n", info.width, info.height,
                   pixels.width, pixels.height);
            fflush(stdout);
            previous_width = info.width; previous_height = info.height;
        }
        if (events & NX_WINDOW_FD_READABLE) {
            char command;
            if (read(STDIN_FILENO, &command, 1) != 1) { okay = 0; break; }
            if (command == 'm') okay = nx_window_maximize(window) == NX_OK;
            else if (command == 'r') okay = nx_window_restore(window) == NX_OK;
            else if (command == 'q') running = 0;
            else okay = 0;
        } else if (events & (NX_WINDOW_FD_HANGUP | NX_WINDOW_FD_ERROR)) {
            okay = 0;
        }
    }
    if (view && nx_software_view_close(&view) != NX_OK) okay = 0;
    if (window) nx_window_destroy(window);
    return okay ? 0 : 1;
}
