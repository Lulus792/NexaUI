"""Real xdg close and copied geometry observations on one mapped test window."""
import hashlib,json
from pathlib import Path
import sys
source=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve()
editor_driver=Path(sys.argv[3]).resolve(strict=True);editor_bytes=editor_driver.read_bytes();editor_text=editor_bytes.decode()
scale_start='    else if (sscanf(text,"Z %u %u%n",&output_index,&width,&used)'
scale_end='    else if (sscanf(text, "O %u %u %u%n", &output_index, &width, &height, &used)'
assert editor_text.count(scale_start)==editor_text.count(scale_end)==1
scale_branch=editor_text[editor_text.index(scale_start):editor_text.index(scale_end)]
output.mkdir(parents=True,exist_ok=True);data=source.read_bytes();text=data.decode()
before='static int receive_input(int fd, uint32_t mask, void *data) {'
helper='''static bool editor_close_command(struct server *s,const char *text) {
    struct window *w,*only=NULL;
    wl_list_for_each(w,&s->windows,link) if(w->mapped) {if(only)return false;only=w;}
    if(!only || s->nested || s->drm) return false;
    if(!strcmp(text,"C")) {
        wlr_xdg_toplevel_send_close(only->toplevel);
        printf("EDITOR_NATIVE_CLOSE window=%u\\n",only->id);
    } else if(!strcmp(text,"Q")) {
        struct wlr_box box=only->toplevel->base->geometry;
        int x,y;if(!wlr_scene_node_coords(&only->tree->node,&x,&y))return false;
        printf("EDITOR_WINDOW id=%u x=%d y=%d width=%d height=%d max=%u\\n",only->id,x,y,box.width,box.height,only->maximized?1u:0u);
    } else return false;
    fflush(stdout);return true;
}
'''
needle="    else if (text[0]=='V'"
assert text.count(before)==text.count(needle)==1
text=text.replace(before,helper+before).replace(needle,'    else if (!strcmp(text,"C") || !strcmp(text,"Q")) { if(!editor_close_command(d->server,text)) goto invalid; }\n'+scale_branch+needle)
generated=text.encode();(output/'compositor_input_driver.c').write_bytes(generated)
(output/'source-delta.json').write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(data).hexdigest(),generated_sha256=hashlib.sha256(generated).hexdigest(),editor_driver=str(editor_driver),editor_driver_sha256=hashlib.sha256(editor_bytes).hexdigest(),original_integer_scale_branch_sha256=hashlib.sha256(scale_branch.encode()).hexdigest(),scope='C sends an actual xdg close and Q reports current scene/committed geometry; exactly one mapped window and private headless ownership required. Integer Z density branch is copied unchanged from the checked original Editor test producer.'),indent=2)+'\n')
