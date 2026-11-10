"""Add read-only native expectations before four original Files captures.

The original native driver and its fixed capture delay remain intact. These
checks use the existing independent IME text peer and original focus helpers;
they do not add keyboard actions or modify application state.
"""

BUILD = """        compiled=subprocess.run(command,capture_output=True,text=True);(root/'client-build.log').write_text(compiled.stdout+compiled.stderr);assert compiled.returncode==0,compiled.stderr"""
BUILD_AFTER = BUILD + """
        capture_peer=root/'capture-peer'
        capture_command=list(command)
        capture_command[capture_command.index(str(ROOT/'tests/files_view_atspi.c'))]=str(_capture_observer_source)
        capture_command[capture_command.index('-o')+1]=str(capture_peer)
        capture_command+=['-I'+str(ROOT/'tests')]
        built=subprocess.run(capture_command,capture_output=True,text=True)
        (root/'capture-peer-build.log').write_text(built.stdout+built.stderr)
        assert built.returncode==0,built.stderr
"""

CAPTURE = """    def capture(label,checks=()):
        time.sleep(.08);sequence=len(images)//3+1;server.send_signal(signal.SIGUSR1)"""
CAPTURE_AFTER = """    capture_observations=[]
    def observe_capture(label):
        values={
            'body press is not adopted as caption and Close is not triggered':('focus','Grid'),
            'inline grid folder':('Name','work','3'),
            'native mouse query selection':('Search this folder','seed','4','0','4'),
            'minimum German 200 inline naming':('Name','draft','5'),
        }
        if label not in values: return
        observed_command=[str(capture_peer),str(app.pid),*values[label]]
        observed=subprocess.run(observed_command,env=env,capture_output=True,text=True,timeout=12)
        observation=dict(case=label,command=observed_command,exit=observed.returncode,
                         stdout=observed.stdout,stderr=observed.stderr)
        capture_observations.append(observation)
        (root/'capture-observations.json').write_text(json.dumps(capture_observations,indent=2)+'\\n')
        assert observed.returncode==0,observation
    def capture(label,checks=()):
        observe_capture(label)
        time.sleep(.08);sequence=len(images)//3+1;server.send_signal(signal.SIGUSR1)"""

RESULT = """        result['peer_sha256']=digest(client) if fixture else None"""
RESULT_AFTER = RESULT + """
        result['capture_observations']=capture_observations
        result['capture_peer']={'path':str(capture_peer),'sha256':digest(capture_peer),
                                'source':str(_capture_observer_source),
                                'source_sha256':digest(_capture_observer_source)}
"""


def prepare(source):
    changes = []
    for before, after in ((BUILD, BUILD_AFTER), (CAPTURE, CAPTURE_AFTER),
                          (RESULT, RESULT_AFTER)):
        if source.count(before) != 1:
            raise SystemExit('Changed original Files capture-observation boundary')
        source = source.replace(before, after)
        changes.append(dict(before=before, after=after))
    return source, changes
