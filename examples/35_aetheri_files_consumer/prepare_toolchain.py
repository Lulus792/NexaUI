"""Keep Clang's strict diagnostics with one equivalent original float predicate."""
import hashlib
import json
from pathlib import Path
import sys

original = Path(sys.argv[1]).resolve(strict=True)
output = Path(sys.argv[2]).resolve()
if original == output or original in output.parents or output in original.parents:
    raise SystemExit('Separate toolchain output required')
source = original/'src/files/layout.c'
data = source.read_bytes(); text = data.decode()
before = 'body_width-(info ? info+20*s : 0)'
after = 'body_width-(info!=0.0f ? info+20*s : 0)'
if text.count(before) != 1:
    raise SystemExit('Changed original Files layout predicate')
generated = text.replace(before, after).encode()
output.mkdir(parents=True, exist_ok=True)
(output/'layout.c').write_bytes(generated)
(output/'source-delta.json').write_text(json.dumps(dict(
    original=str(source), original_sha256=hashlib.sha256(data).hexdigest(),
    generated_sha256=hashlib.sha256(generated).hexdigest(),
    changes=[dict(before=before, after=after)],
    scope='One equivalent zero/nonzero float predicate for the explicit SDK0.1.33 Clang consumer profile. Original export, file-operation policy and strict conversion warnings retained.'), indent=2)+'\n')
