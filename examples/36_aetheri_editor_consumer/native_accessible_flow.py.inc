        import select
        ax_checks=[];ax_sequence=0
        def ax_phase(phase,expected=None):
            nonlocal ax_sequence
            ax_sequence+=1;command=[str(_candidate_accessible_client),str(app.pid),phase]
            if expected is not None:
                path=expected if isinstance(expected,Path) else root/'ax-expected.txt'
                if not isinstance(expected,Path):path.write_bytes(expected.encode())
                command.append(str(path))
            path=root/('ax-phase-'+str(ax_sequence)+'.log')
            with path.open('xb') as stream:completed=subprocess.run(command,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=14)
            assert completed.returncode==0,(phase,path.read_text(errors='replace'))
            ax_checks.append(dict(phase=phase,log=path.name,exit=completed.returncode,client_sha256=digest(_candidate_accessible_client)))
        def ax_hold():
            stream=(root/('ax-held-'+str(len(ax_checks))+'.log')).open('xb');streams.append(stream)
            peer=subprocess.Popen([str(_candidate_accessible_client),str(app.pid),'hold'],env=env,
                stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=stream,text=True,bufsize=1)
            children.append(peer)
            assert select.select([peer.stdout],[],[],14)[0] and peer.stdout.readline()=='READY\n'
            return peer
        def ax_reject(peer):
            peer.stdin.write('reject\n');peer.stdin.flush()
            assert select.select([peer.stdout],[],[],14)[0] and peer.stdout.readline()=='REJECTED\n'
            ax_checks.append('held original document reference rejected and retired')
        def ax_finish(peer):
            out,_=peer.communicate('quit\n',timeout=14)
            assert peer.returncode==0 and 'Original Editor independent AT-SPI phase passed.' in out
            children.remove(peer)
        def ax_style(name):
            key(30,ctrl=True);wait(lambda:state(selected=1,begin=0,end=7),name+' original selection before assistive style task')
            ax_phase('style');wait(lambda:state(cursor=4,selected=0,dirty=1),name+' assistive edit uses original model')
            if _candidate_native_presentation:ax_phase('geometry-visible')
            capture(name+' native accessible document edit')
            key(44,ctrl=True);wait(lambda:state(cursor=7,dirty=0),name+' assistive Undo restores original clean document')
            key(30,ctrl=True);wait(lambda:state(selected=1,begin=0,end=7),name+' original selection restored for subsequent native tasks')
        # Native clipboard task boundary.
        key(49,ctrl=True);wait(lambda:state(docs=1,cursor=0),'new original accessible document')
        typing('abcde');wait(lambda:state(cursor=5),'original assistive seed')
        if _candidate_native_presentation:ax_phase('geometry')
        if _candidate_accessible_ime:
            ime_expect('abcde',5);ime_preedit(0,7);ime_snapshot('abcde',5)
        ax_phase('edits');wait(lambda:state(cursor=1,selected=0,dirty=1),'native ranged edit reflected in original view')
        if _candidate_accessible_ime:
            wait(lambda:state(ime=0),'assistive model edit cancels the previous original composition');ime_expect('A🌍\nline',1)
        ax_phase('expect','A🌍\nline');capture('independent native Unicode edits use the original document')
        previous=int(view()['revision']);key(44,ctrl=True)
        wait(lambda:int(view()['revision'])>previous and state(cursor=5),'one original Undo restores the native deletion')
        ax_phase('expect','AéΩ🌍\nline');key(21,ctrl=True);wait(lambda:state(cursor=1),'original Redo reapplies native deletion')
        ax_phase('expect','A🌍\nline');ax_phase('grapheme');ax_phase('final');wait(lambda:state(cursor=15,selected=0),'original model receives final assistive replacement')
        key(31,ctrl=True);wait(lambda:state(mode=2,draft=0),'original first save after assistive editing')
        typing('accessible.txt');wait(lambda:state(draft=14),'original assistive save name');key(28)
        wait(lambda:state(mode=0,io=0,dirty=0) and (folder/'accessible.txt').exists(),'original worker saves assistive bytes')
        assert (folder/'accessible.txt').read_bytes()==b'assistive\nsaved'
        held=ax_hold();typing('x');wait(lambda:state(cursor=16,dirty=1),'ordinary input remains original after native editing')
        key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'original close review blocks the accessible controller')
        ax_reject(held);ax_phase('readonly','assistive\nsavedx');capture('original close review retains read-only accessible text')
        key(1);wait(lambda:state(review=0,docs=1,cursor=16),'original review Cancel retains document')
        ax_reject(held);ax_finish(held);ax_phase('expect','assistive\nsavedx')
        key(44,ctrl=True);wait(lambda:state(cursor=15,dirty=0),'original Undo returns to saved assistive state')
        original=view()['active'];held=ax_hold();key(49,ctrl=True);wait(lambda:state(docs=2,cursor=0),'original new tab omits old accessible target')
        ax_reject(held);key(15,ctrl=True);wait(lambda:state(active=original,cursor=15),'original tab and model restored')
        ax_reject(held);ax_finish(held);ax_phase('expect','assistive\nsaved')
        key(17,ctrl=True);wait(lambda:state(docs=1,cursor=0),'clean original assistive tab closes')
        key(17,ctrl=True);wait(lambda:state(docs=0),'remaining empty tab closes')
        large=bytearray(b'x'*(8*1024*1024));large[131:-1:132]=b'\n'*len(large[131:-1:132]);large[0]=65;large[-1]=90
        large_path=folder/'ax-large.txt';large_path.write_bytes(large);del large
        key(24,ctrl=True);wait(lambda:state(mode=1),'original Open of maximum-size accessible document')
        click('ax-large.txt');key(28);wait(lambda:state(mode=0,docs=1,dirty=0),'original maximum-size document opened')
        ax_phase('large',large_path);wait(lambda:state(cursor=1,dirty=1),'native bounded edits of original maximum-size model')
        previous=int(view()['revision']);key(44,ctrl=True)
        wait(lambda:int(view()['revision'])>previous,'original large-document insert Undo')
        previous=int(view()['revision']);key(44,ctrl=True)
        wait(lambda:int(view()['revision'])>previous and state(dirty=0),'original large-document delete Undo restores clean state')
        ax_phase('expect',large_path);capture('full original eight-MiB accessible document after original Undo')
        key(17,ctrl=True);wait(lambda:state(docs=0,review=0),'clean maximum-size original document closes')
        assert (folder/'accessible.txt').read_bytes()==b'assistive\nsaved'
        (root/'accessible-result.json').write_text(json.dumps(dict(result='passed',checks=ax_checks,maximum_document_bytes=8*1024*1024,
            scope='Independent OS peer against the original owner/controller/history/render/save/close, including original grapheme/CRLF refusal, withheld-input review, retained references, tab reuse and full maximum-size bytes. Controlled private producer; no human/physical/performance or normal owner adoption.'),indent=2)+'\n')
