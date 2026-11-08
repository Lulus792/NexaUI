"""Expose the original atomic edit engine to a correlated native-input adapter.

Only separate generated model files change. Original entry points keep their
grapheme policy; their existing inverse-history byte path remains unchanged.
"""
import hashlib,json,subprocess,sys
from pathlib import Path
original=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve();here=Path(__file__).resolve().parent
subprocess.run([sys.executable,str(here/'../35_aetheri_files_consumer/check_source.py'),str(original),str(here/'source-inputs.json')],check=True)
if output==original or original in output.parents or output in original.parents:raise SystemExit('Separate generated model output required')
output.mkdir(parents=True,exist_ok=True);records=[]
def transform(name,changes,append):
    path=original/'src/editor'/name;data=path.read_bytes().replace(b'\r\n',b'\n');text=data.decode()
    for before,after in changes:
        if text.count(before)!=1:raise SystemExit('Changed original atomic-edit boundary: '+name+' '+repr(before))
        text=text.replace(before,after)
    generated=(text+append).encode();(output/name).write_bytes(generated)
    records.append(dict(original='src/editor/'+name,original_sha256=hashlib.sha256(data).hexdigest(),generated=name,sha256=hashlib.sha256(generated).hexdigest(),changes=[dict(before=a,after=b) for a,b in changes],append=append))
transform('document.c',[
    ('bool record,bool typing,bool stay_at_line_start) {','bool record,bool typing,bool stay_at_line_start,bool native_bytes) {'),
    ('    if (record ? (!boundary(d,begin,&first) || !boundary(d,end,&last)) : (!ae_document_row(d,begin,&first) || !ae_document_row(d,end,&last))) return AE_DOCUMENT_INVALID;',
     '''    if (native_bytes && ((begin<d->size && ((unsigned char)byte(d,begin)&0xc0u)==0x80u) ||
        (end<d->size && ((unsigned char)byte(d,end)&0xc0u)==0x80u))) return AE_DOCUMENT_INVALID;
    if (record && !native_bytes ? (!boundary(d,begin,&first) || !boundary(d,end,&last)) : (!ae_document_row(d,begin,&first) || !ae_document_row(d,end,&last))) return AE_DOCUMENT_INVALID;'''),
    ('       start inside a newly joined grapheme; only private original history can\n       select this raw byte path, with its exact logical current-state binding.',
     '       start inside a newly joined grapheme. Private original history and the\n       correlated native adapter select byte paths with original state binding.'),
    ('replace(d,begin,end,text,size,true,false,false);','replace(d,begin,end,text,size,true,false,false,false);'),
    ('replace(d,begin,end,text,size,true,false,true);','replace(d,begin,end,text,size,true,false,true,false);'),
    ('replace(d,d->cursor,d->cursor,text,size,true,true,false)','replace(d,d->cursor,d->cursor,text,size,true,true,false,false)'),
    ('replace(d,entry->begin,entry->begin+remove,entry->data+(undo ? 0 : entry->removed),insert,false,false,false);',
     'replace(d,entry->begin,entry->begin+remove,entry->data+(undo ? 0 : entry->removed),insert,false,false,false,false);')],
'''
/* Internal generated-model entry; the original controller checks its token. */
enum ae_document_result editor_ime_document_replace(struct ae_document *document,
    size_t begin,size_t end,const char *text,size_t size) {
    return replace(document,begin,end,text,size,true,false,false,true);
}
''')
transform('direct.c',[('#include "direct.h"','#include "direct.h"\n#include "ime_model.h"')],
'''
enum ae_document_result editor_ime_document_replace(struct ae_document *,size_t,size_t,const char *,size_t);
enum ae_document_result editor_direct_ime_replace(struct ae_direct *edit,
    const struct ae_direct_token *token,size_t begin,size_t end,const char *text,size_t size) {
    struct ae_direct_token current;
    if(!token || !ae_direct_token(edit,&current) || token->identity!=current.identity ||
       token->interaction!=current.interaction || token->revision!=current.revision ||
       token->cursor!=current.cursor || token->anchor!=current.anchor ||
       token->selecting!=current.selecting || (!text && size) || !available(edit)) return AE_DOCUMENT_INVALID;
    enum ae_document_result result=editor_ime_document_replace(edit->document,begin,end,text,size);
    if(result==AE_DOCUMENT_OK) {ae_document_history_break(edit->document);collapsed(edit);}
    return result;
}
''')
(output/'ime-model-delta.json').write_text(json.dumps(dict(files=records,scope='Optional original-model candidate adapter. Same gap-buffer/history/preflight engine; correlated native scalar-range entry only. No active Aetheri change or SDK API.'),indent=2)+'\n')
