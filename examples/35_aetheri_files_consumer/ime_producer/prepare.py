"""Generate a private, source-checked native IME producer; never edit its export."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

original=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve();here=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here.parent/'check_source.py'),str(original),str(here.parent/'ime-producer-inputs.json')],check=True)
if original==output or original in output.parents or output in original.parents:raise SystemExit('Use a separate producer output')
output.mkdir(parents=True,exist_ok=True);records=[]
def transform(name,changes):
    data=(original/'tests'/name).read_bytes();text=data.decode()
    for before,after in changes:
        if text.count(before)!=1:raise SystemExit('Changed original producer input: '+name+' '+repr(before))
        text=text.replace(before,after)
    generated=text.encode();(output/name).write_bytes(generated);records.append(dict(original='tests/'+name,original_sha256=hashlib.sha256(data).hexdigest(),generated=name,sha256=hashlib.sha256(generated).hexdigest(),changes=[dict(before=a,after=b) for a,b in changes]))

transform('compositor_text_input.c',[
    ('        t->current.cursor_rectangle.x,t->current.cursor_rectangle.y,t->current.cursor_rectangle.width,t->current.cursor_rectangle.height);',
     '''        t->current.cursor_rectangle.x,t->current.cursor_rectangle.y,t->current.cursor_rectangle.width,t->current.cursor_rectangle.height);
    const unsigned char *text=(const unsigned char *)(t->current.surrounding.text?t->current.surrounding.text:"");
    printf("IME state enabled=%d serial=%u cursor=%ld anchor=%ld hex=",t->current_enabled,t->current_serial,
        (long)t->current.surrounding.cursor,(long)t->current.surrounding.anchor);
    for(size_t i=0;text[i];++i) printf("%02x",(unsigned)text[i]);
    putchar('\\n');'''),
    ('if (!f || command>6) return false;','if (!f || (command>6 && (command<16 || command>29))) return false;'),
    ('    wlr_text_input_v3_send_done(t);printf("IME produced=%u\\n",command);return true;',r'''    if (command==16 || command==17 || command==18 || command==19) {
        wlr_text_input_v3_send_delete_surrounding_text(t,1,0);
        if (command!=19) wlr_text_input_v3_send_commit_string(t,command==16?"x\xc3\xa9":command==17?"bad\nname":"");
    } else if (command==20) {
        wlr_text_input_v3_send_commit_string(t,"x");wlr_text_input_v3_send_done(t);
        wlr_text_input_v3_send_commit_string(t,"y");
    } else if (command==21) wlr_text_input_v3_send_commit_string(t,"");
    else if (command==22) wlr_text_input_v3_send_commit_string(t,"bad/name");
    else if (command==23) {
        char long_text[257];memset(long_text,'x',256);long_text[256]=0;wlr_text_input_v3_send_commit_string(t,long_text);
    } else if (command==24) wlr_text_input_v3_send_commit_string(t,"e\xcc\x81");
    else if (command==25 || command==27) {
        wlr_text_input_v3_send_delete_surrounding_text(t,0,1);wlr_text_input_v3_send_commit_string(t,"q");
        if (command==27) {
            wlr_text_input_v3_send_done(t);
            wlr_text_input_v3_send_delete_surrounding_text(t,1,0);wlr_text_input_v3_send_commit_string(t,"z");
        }
    } else if (command==26) wlr_text_input_v3_send_commit_string(t,"aX\xcc\x81" "b");
    else if (command==28) {
        char pressure[301];memset(pressure,'x',300);pressure[300]=0;
        wlr_text_input_v3_send_commit_string(t,pressure);
    } else if (command==29) {
        wlr_text_input_v3_send_commit_string(t,"bad/name");wlr_text_input_v3_send_done(t);
        wlr_text_input_v3_send_commit_string(t,"good");
    }
    wlr_text_input_v3_send_done(t);printf("IME produced=%u\n",command);return true;''')])

transform('compositor_input_driver.c',[(
    '    else if (sscanf(text,"I %u%n",&code,&used)==1 && used==length)',
    '''    else if (!strcmp(text,"I 8") && d->live) {
        struct wlr_keyboard_key_event event={.time_msec=d->time,.keycode=15,.state=WL_KEYBOARD_KEY_STATE_PRESSED,.update_state=true};
        wlr_keyboard_notify_key(&d->keyboard,&event);
        if (!ae_test_text_input_command(d->ime,1)) goto invalid;
        event.state=WL_KEYBOARD_KEY_STATE_RELEASED;wlr_keyboard_notify_key(&d->keyboard,&event);puts("IME boundary=8");
    }
    else if (sscanf(text,"I %u%n",&code,&used)==1 && used==length)''')])
(output/'source-delta.json').write_text(json.dumps(dict(files=records,scope='Private test producer only; frozen d56 compositor sources/export and active Aetheri unchanged'),indent=2)+'\n')
print('GENERATED two private IME producer files from checked d56 inputs')
