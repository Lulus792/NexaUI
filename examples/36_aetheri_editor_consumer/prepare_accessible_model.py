"""Add atomic original-controller selection to checked generated IME models."""
import hashlib,json,sys
from pathlib import Path
source=Path(sys.argv[1]).resolve(strict=True);output=Path(sys.argv[2]).resolve()
if source==output or source in output.parents or output in source.parents:
    raise SystemExit('Separate accessible model output required')
append={
'document.c': '''
/* Pure original grapheme/CRLF preflight; never moves the document cursor. */
bool editor_accessible_document_boundary(const struct ae_document *document,size_t position) {
    size_t row;return document && boundary(document,position,&row);
}
''',
'direct.c': '''
bool editor_accessible_document_boundary(const struct ae_document *,size_t);
bool editor_direct_accessible_select_bytes(struct ae_direct *edit,
    const struct ae_direct_token *token,size_t anchor,size_t cursor) {
    struct ae_direct_token current;
    if(!token || !ae_direct_token(edit,&current) || token->identity!=current.identity ||
       token->interaction!=current.interaction || token->revision!=current.revision ||
       token->cursor!=current.cursor || token->anchor!=current.anchor ||
       token->selecting!=current.selecting || !available(edit) ||
       !editor_accessible_document_boundary(edit->document,anchor) ||
       !editor_accessible_document_boundary(edit->document,cursor))return false;
    if(!ae_document_cursor(edit->document,cursor))return false;
    edit->anchor=anchor;edit->selecting=anchor!=cursor;edit->vertical=false;accepted(edit);return true;
}
'''}
records=[];inputs={}
for name,marker in (('document.c','enum ae_document_result editor_ime_document_replace('),('direct.c','enum ae_document_result editor_direct_ime_replace(')):
    data=(source/name).read_bytes();assert data.count(marker.encode())==1,name
    inputs[name]=data
output.mkdir(parents=True,exist_ok=True)
for name,data in inputs.items():
    generated=data+append[name].encode();(output/name).write_bytes(generated)
    records.append(dict(input=str(source/name),input_sha256=hashlib.sha256(data).hexdigest(),generated=name,sha256=hashlib.sha256(generated).hexdigest(),append=append[name]))
(output/'accessible-model-delta.json').write_text(json.dumps(dict(files=records,scope='Optional original accessible model prerequisite. Same document/controller/history; atomic two-endpoint selection preflight only. No SDK transport or active Aetheri changes.'),indent=2)+'\n')
