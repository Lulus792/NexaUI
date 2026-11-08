"""Add bounded output policy to a checked private producer, never its export."""
import hashlib
import json
from pathlib import Path
import sys

original,base=(Path(p).resolve(strict=True) for p in sys.argv[1:3])
output=Path(sys.argv[3]).resolve()
if original==output or original in output.parents or output in original.parents:raise SystemExit('Separate output required')
output.mkdir(parents=True,exist_ok=True)
records=[]
def transform(source,name,changes):
    data=source.read_bytes();text=data.decode()
    for before,after in changes:
        if text.count(before)!=1:raise SystemExit('Changed producer boundary: '+repr(before))
        text=text.replace(before,after)
    generated=text.encode();(output/name).write_bytes(generated)
    records.append(dict(source=str(source),source_sha256=hashlib.sha256(data).hexdigest(),generated=name,sha256=hashlib.sha256(generated).hexdigest(),changes=[dict(before=a,after=b) for a,b in changes]))

transform(original/'src/compositor/main.c','main.c',[
    ('#include <wlr/types/wlr_viewporter.h>','#include <wlr/types/wlr_viewporter.h>\n#include <wlr/types/wlr_fractional_scale_v1.h>'),
    ('!wlr_data_device_manager_create(s.display) || !wlr_viewporter_create(s.display))',
     '!wlr_data_device_manager_create(s.display) || !wlr_viewporter_create(s.display) || !wlr_fractional_scale_manager_v1_create(s.display, 1))'),
    ('    int usable_height = o->output->height - o->reserved_bottom;\n    int padding_x = o->output->width >= 400 ? 40 : 0;',
     '    int logical_width, logical_height;\n    wlr_output_effective_resolution(o->output, &logical_width, &logical_height);\n    wlr_fractional_scale_v1_notify_scale(top->base->surface, o->output->scale);\n    int32_t integer_scale=(int32_t)o->output->scale;\n    if ((float)integer_scale<o->output->scale) ++integer_scale;\n    wlr_surface_set_preferred_buffer_scale(top->base->surface, integer_scale);\n    int usable_height = logical_height - o->reserved_bottom;\n    int padding_x = logical_width >= 400 ? 40 : 0;'),
    ('wlr_xdg_toplevel_set_bounds(top, o->output->width - 2 * padding_x, usable_height - 2 * padding_y);',
     'wlr_xdg_toplevel_set_bounds(top, logical_width - 2 * padding_x, usable_height - 2 * padding_y);'),
    ('wlr_xdg_toplevel_set_size(top, full || max ? o->output->width : 0,\n        full ? o->output->height : max ? usable_height : 0);',
     'wlr_xdg_toplevel_set_size(top, full || max ? logical_width : 0,\n        full ? logical_height : max ? usable_height : 0);'),
    ('    if (w->mapped) ae_desktop_refresh_visibility(w->server->desktop);',
     '    if (w->mapped) {\n        ae_desktop_refresh_visibility(w->server->desktop);\n        struct wlr_surface_state *state=&w->toplevel->base->surface->current;\n        printf("OUTPUT_BUFFER pixels=%dx%d logical=%dx%d\\n",state->buffer_width,state->buffer_height,state->width,state->height);\n    }')])
transform(base/'compositor_input_driver.c','compositor_input_driver.c',[
    ('static int receive_input(int fd, uint32_t mask, void *data) {','#include "commands.h"\nstatic int receive_input(int fd, uint32_t mask, void *data) {'),
    ('    else if (sscanf(text, "O %u %u %u%n", &output_index, &width, &height, &used)',
     '    else if (text[0]==\'V\' || text[0]==\'P\' || text[0]==\'H\' || text[0]==\'Z\') { if (!sw_output_command(d->server,text)) goto invalid; }\n    else if (sscanf(text, "O %u %u %u%n", &output_index, &width, &height, &used)'),
    ('        struct wlr_pointer_motion_absolute_event event = {.pointer = &d->pointer, .time_msec = d->time, .x = x / 8320.0, .y = y / 2160.0};',
     '        struct wlr_box bounds;wlr_output_layout_get_box(d->server->layout,NULL,&bounds);\n        if (bounds.width<=0 || bounds.height<=0 || x<bounds.x || y<bounds.y || x>=bounds.x+bounds.width || y>=bounds.y+bounds.height) goto invalid;\n        struct wlr_pointer_motion_absolute_event event = {.pointer = &d->pointer, .time_msec = d->time, .x = (x-bounds.x) / bounds.width, .y = (y-bounds.y) / bounds.height};')])
(output/'source-delta.json').write_text(json.dumps(dict(files=records,scope='Private source-checked output policy; no SDK or active Aetheri changes'),indent=2)+'\n')
