#include "renderer_backend.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; \
} } while (0)

typedef struct mock_backend {
    unsigned resizes, submits, recovers, destroys;
    nx_render_target last_target;
    nx_render_outcome next_resize, next_submit;
    nx_result recover_result, destroy_result;
    uint8_t pixels[8u * 8u * 4u];
    size_t received_count;
    nx_command_type received_type;
} mock_backend;

static nx_result mock_resize(void *user, nx_render_target target,
                             nx_render_outcome *outcome) {
    mock_backend *mock = user;
    ++mock->resizes;
    mock->last_target = target;
    if (target.pixel_width > 8 || target.pixel_height > 8)
        return NX_ERROR_LIMIT;
    *outcome = mock->next_resize;
    mock->next_resize = NX_RENDER_READY;
    return NX_OK;
}

static nx_result mock_submit(void *user, nx_render_target target,
                             const nx_command *commands, size_t count,
                             nx_color clear_color,
                             nx_render_outcome *outcome) {
    mock_backend *mock = user;
    ++mock->submits;
    mock->received_count = count;
    mock->received_type = count ? commands[0].type : 0;
    *outcome = mock->next_submit;
    mock->next_submit = NX_RENDER_READY;
    if (*outcome != NX_RENDER_READY) return NX_OK;
    return nx_headless_render(commands, count, mock->pixels,
        target.pixel_width, target.pixel_height, 8u * 4u, clear_color);
}

static nx_result mock_recover(void *user) {
    mock_backend *mock = user;
    ++mock->recovers;
    return mock->recover_result;
}

static nx_result mock_destroy(void *user) {
    mock_backend *mock = user;
    if (mock->destroy_result != NX_OK) return mock->destroy_result;
    ++mock->destroys;
    return NX_OK;
}

static nx_render_backend backend_for(mock_backend *mock) {
    return (nx_render_backend){mock, mock_resize, mock_submit,
                                mock_recover, mock_destroy};
}

static nx_command fill_command(void) {
    nx_command command = {0};
    command.type = NX_COMMAND_FILL_RECT;
    command.rect = (nx_rect){0, 0, 4, 4};
    command.color = (nx_color){1, 0, 0, 1};
    return command;
}

static int test_lifecycle(void) {
    mock_backend mock = {0};
    mock.next_resize = mock.next_submit = NX_RENDER_READY;
    nx_render_backend backend = backend_for(&mock);
    nx_renderer_driver driver = {0};
    nx_render_target target = {4, 4, 1};
    nx_color clear = {0, 0, 0, 1};
    nx_command command = fill_command();
    nx_render_outcome outcome = NX_RENDER_DEVICE_LOST;
    CHECK(nx_renderer_open(&driver, &backend) == NX_OK);
    CHECK(nx_renderer_open(&driver, &backend) == NX_ERROR_STATE);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 1 && mock.submits == 1 &&
          mock.received_count == 1 &&
          mock.received_type == NX_COMMAND_FILL_RECT &&
          mock.pixels[0] == 255 && mock.pixels[1] == 0);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 1 && mock.submits == 2);
    target.scale = 2;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 2 && mock.submits == 3);
    target.pixel_width = 0;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_SUSPENDED);
    CHECK(mock.resizes == 3 && mock.submits == 3 &&
          mock.last_target.pixel_height == 0);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_SUSPENDED);
    CHECK(mock.resizes == 3);
    target.pixel_width = target.pixel_height = 4;
    mock.next_resize = NX_RENDER_SUSPENDED;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_SUSPENDED);
    CHECK(mock.resizes == 4 && mock.submits == 3);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 5 && mock.submits == 4);
    mock.next_submit = NX_RENDER_SUSPENDED;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_SUSPENDED);
    CHECK(mock.resizes == 5 && mock.submits == 5);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 6 && mock.submits == 6);
    nx_renderer_close(&driver);
    CHECK(mock.destroys == 1);
    nx_renderer_close(&driver);
    CHECK(mock.destroys == 1);
    return 0;
}

static int test_device_loss(void) {
    mock_backend mock = {0};
    mock.next_resize = mock.next_submit = NX_RENDER_READY;
    nx_render_backend backend = backend_for(&mock);
    nx_renderer_driver driver = {0};
    nx_render_target target = {4, 4, 1};
    nx_color clear = {0, 0, 0, 1};
    nx_command command = fill_command();
    nx_render_outcome outcome;
    CHECK(nx_renderer_open(&driver, &backend) == NX_OK);
    mock.next_resize = NX_RENDER_DEVICE_LOST;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK &&
          outcome == NX_RENDER_DEVICE_LOST);
    CHECK(mock.resizes == 1 && mock.submits == 0);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK &&
          outcome == NX_RENDER_DEVICE_LOST && mock.resizes == 1);
    mock.recover_result = NX_ERROR_PLATFORM;
    CHECK(nx_renderer_recover(&driver) == NX_ERROR_PLATFORM);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK &&
          outcome == NX_RENDER_DEVICE_LOST);
    mock.recover_result = NX_OK;
    CHECK(nx_renderer_recover(&driver) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 2 && mock.submits == 1);
    mock.next_submit = NX_RENDER_DEVICE_LOST;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK &&
          outcome == NX_RENDER_DEVICE_LOST);
    CHECK(nx_renderer_recover(&driver) == NX_OK);
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 3 && mock.submits == 3);
    nx_renderer_close(&driver);
    return 0;
}

static int test_preflight(void) {
    mock_backend mock = {0};
    mock.next_resize = mock.next_submit = NX_RENDER_READY;
    nx_render_backend backend = backend_for(&mock);
    nx_renderer_driver driver = {0};
    nx_render_target target = {4, 4, 1};
    nx_color clear = {0, 0, 0, 1};
    nx_command command = fill_command();
    nx_render_outcome outcome;
    CHECK(nx_renderer_open(&driver, &backend) == NX_OK);
    command.type = NX_COMMAND_POP_CLIP;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_ERROR_COMMAND);
    CHECK(mock.resizes == 0 && mock.submits == 0);
    command = fill_command();
    command.color.r = -1;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_ERROR_COMMAND);
    CHECK(mock.resizes == 0 && mock.submits == 0);
    command = fill_command();
    target.scale = 0;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_ERROR_ARGUMENT);
    CHECK(mock.resizes == 0 && mock.submits == 0);
    target.scale = 1;
    CHECK(nx_renderer_run(&driver, target, &command, 1, clear,
                           &outcome) == NX_OK && outcome == NX_RENDER_READY);
    CHECK(mock.resizes == 1 && mock.submits == 1);
    nx_renderer_close(&driver);
    return 0;
}

static int test_close_retry(void) {
    mock_backend mock = {0};
    nx_render_backend backend = backend_for(&mock);
    nx_renderer_driver driver = {0};
    CHECK(nx_renderer_open(&driver, &backend) == NX_OK);
    mock.destroy_result = NX_ERROR_LIMIT;
    CHECK(nx_renderer_close(&driver) == NX_ERROR_LIMIT);
    CHECK(driver.open && driver.closing && driver.backend.user == &mock && !mock.destroys);
    nx_render_outcome outcome;
    CHECK(nx_renderer_run(&driver, (nx_render_target){4, 4, 1}, NULL, 0,
        (nx_color){0, 0, 0, 1}, &outcome) == NX_ERROR_STATE);
    driver.device_lost = 1;
    CHECK(nx_renderer_recover(&driver) == NX_ERROR_STATE && !mock.recovers);
    CHECK(!mock.resizes && !mock.submits);
    mock.destroy_result = NX_OK;
    CHECK(nx_renderer_close(&driver) == NX_OK);
    CHECK(!driver.open && !driver.backend.user && mock.destroys == 1);
    CHECK(nx_renderer_close(&driver) == NX_OK && mock.destroys == 1);
    return 0;
}

int main(void) {
    return test_lifecycle() || test_device_loss() || test_preflight() || test_close_retry();
}
