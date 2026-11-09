        # Runs in the original native driver's scope, after its IME task and
        # before all existing independent document/close tasks. The foreign
        # implementation and loader are hash checked by the existing Files task.
        import importlib.util
        clipboard_spec=importlib.util.spec_from_file_location('editor_foreign_clipboard',_candidate_clipboard_helper)
        clipboard_module=importlib.util.module_from_spec(clipboard_spec);clipboard_spec.loader.exec_module(clipboard_module)
        clipboard_result=dict(result='running',checks=[])
        foreign=clipboard_module.ClipboardTask(root,_candidate_clipboard_tools,env,
            None,None,None,None,None,wait,None,clipboard_result,None)
        seed='Aé🌍\r\nline';cut_text='A\r\nline';clipboard_peers=[]
        def clipboard_seed():
            ax_phase('clipboard-seed');wait(lambda:state(cursor=0,selected=0),'native clipboard seed through original owner')
            ax_phase('expect',seed)
            if _candidate_accessible_ime:ime_expect(seed,0)
        def clipboard_serial_key(value,cursor):
            app_cut=len(log());cut=len(log('server'));key(42)
            if _candidate_accessible_ime:
                # A key supplies a real selection serial and retires the old
                # original IME context. Wait for its processed disable/enable
                # exchange before producing preedit for the new context.
                def processed_key():
                    tail=log()[app_cut:];keys=list(re.finditer(r'wl_keyboard#\d+\.key\(\d+, \d+, 42, 1\)',tail))
                    return bool(keys and ime_contexts(tail[keys[-1].end():]))
                wait(processed_key,'original owner processed the clipboard serial key and published its new IME context')
                # This pinned producer's current_serial counts this one window's
                # protocol commits. Join the actual client/producer messages;
                # equal surrounding bytes alone could be an older context.
                commits=len(re.findall(r'-> zwp_text_input_v3#\d+\.commit\(\)',log()));assert commits
                def rebound():
                    rows=re.findall(r'^IME commit enabled=(\d+) serial=(\d+) ',log('server')[cut:],re.M)
                    return bool(rows and rows[-1][0]=='1' and int(rows[-1][1])>=commits)
                wait(rebound,'clipboard serial key IME context actually rebound')
                ime_expect(value,cursor,after=cut)
                clipboard_result.setdefault('ime_serial_barriers',[]).append(dict(client_commits=commits,provider_serial=int(re.findall(r'^IME commit enabled=1 serial=(\d+) ',log('server')[cut:],re.M)[-1])))
        def clipboard_pending(phase,expected):
            path=root/'clipboard-pending-expected.txt';path.write_bytes(expected.encode())
            stream=(root/('clipboard-native-'+str(len(clipboard_result['checks']))+'.log')).open('xb');streams.append(stream)
            peer=subprocess.Popen([str(_candidate_accessible_client),str(app.pid),phase,str(path)],env=env,
                stdout=subprocess.PIPE,stderr=stream,text=True,bufsize=1)
            # A finite request is allowed to settle during an input/IME barrier.
            # Keep it separate from the original driver's must-stay-alive list;
            # its exact exit/reply is still joined below and on every teardown.
            clipboard_peers.append(peer)
            assert select.select([peer.stdout],[],[],14)[0] and peer.stdout.readline()=='READY\n'
            return peer
        def clipboard_finish(peer):
            out,_=peer.communicate(timeout=14)
            assert peer.returncode==0 and 'Original Editor independent AT-SPI phase passed.' in out
            clipboard_peers.remove(peer)
        clipboard_close_state=None;clipboard_close_tasks=[]
        def clipboard_cleanup():
            for task in clipboard_close_tasks:task.close()
            foreign.close()
            for peer in clipboard_peers:
                if peer.poll() is None:
                    peer.terminate()
                    try:peer.wait(timeout=5)
                    except subprocess.TimeoutExpired:peer.kill();peer.wait(timeout=5)
                peer.stdout.close()
            clipboard_peers.clear()
        def clipboard_close_begin():
            if _candidate_accessible_ime:
                # Original zoom/restore leaves toolbar focus. Return through
                # the original F6 route before requesting a document IME context.
                key(64);ime_expect('retainedxz',10)
            clipboard_result['close_clipboard']={}
            task=clipboard_module.ClipboardTask(root,_candidate_clipboard_tools,env,
                None,None,None,None,None,wait,None,clipboard_result['close_clipboard'],None)
            clipboard_close_tasks.append(task)
            source=task.offer(b'close pending');clipboard_serial_key('retainedxz',10);source.send_signal(signal.SIGSTOP)
            wait(lambda:re.search(r'^State:\s+T',Path(f'/proc/{source.pid}/status').read_text(),re.M) is not None,'close clipboard provider actually stopped')
            cut=len(log());peer=clipboard_pending('clipboard-paste-cancelled','retainedxz')
            wait(lambda:'.receive(' in log()[cut:],'native clipboard read outstanding before real compositor close')
            return task,source,peer
        def clipboard_close_settled(value):
            clipboard_finish(value[2])
            assert re.search(r'^State:\s+T',Path(f'/proc/{value[1].pid}/status').read_text(),re.M)
        def clipboard_close_end(value):
            assert app.returncode==0 and re.search(r'^State:\s+T',Path(f'/proc/{value[1].pid}/status').read_text(),re.M)
            clipboard_result['checks'].append('Actual compositor close rejects the pending native request; original Save and exit0 complete while the foreign provider remains stopped')
            value[0].close()
            # The unchanged producer assigns every new toplevel round-robin.
            # Foreign temporary clipboard windows also consume those slots.
            # Retire at most two additional checked offer/read pairs so the
            # original contrast/light apps still map to outputs1/2, preserving
            # their original density assertions and every capture unchanged.
            placement={};task=clipboard_module.ClipboardTask(root,_candidate_clipboard_tools,env,
                None,None,None,None,None,wait,None,placement,None)
            clipboard_close_tasks.append(task)
            homes=lambda:list(map(int,re.findall(r'^MAP ([0-2])$',log('server'),re.M)))
            before=homes();assert before
            for _ in range(2):
                if homes()[-1]==0:break
                count=len(homes());task.offer(b'fixture placement')
                wait(lambda:len(homes())==count+2,'both temporary clipboard windows mapped and retired')
            assert homes()[-1]==0,'next original style window must retain output1'
            placement.update(before=before,after=homes(),next_original_output=1)
            task.close();clipboard_result['fixture_placement']=placement
            clipboard_result['result']='passed'
            (root/'clipboard-result.json').write_text(json.dumps(clipboard_result,indent=2)+'\n')
        try:
            key(49,ctrl=True);wait(lambda:state(docs=1,cursor=0),'fresh original clipboard document')
            clipboard_seed();clipboard_serial_key(seed,0)
            if _candidate_accessible_ime:ime_preedit(0,7)
            revision=view()['revision'];ax_phase('clipboard-copy')
            assert view()['revision']==revision and state(cursor=0)
            if _candidate_accessible_ime:
                assert state(ime=7);ime_preedit(2,0,geometry=False);ime_expect(seed,0)
            foreign.read('é🌍'.encode())
            clipboard_serial_key(seed,0);ax_phase('clipboard-cut');wait(lambda:state(cursor=1),'native Cut applied in original model')
            foreign.read('é🌍'.encode());key(44,ctrl=True);ax_phase('expect',seed)
            key(21,ctrl=True);ax_phase('expect',cut_text)
            clipboard_result['checks'].append('Native Copy preserves model revision, caret and active composition; native Cut exports exact Unicode and uses original Undo/Redo')
            payload='Fremd 🌍\r\n';foreign.offer(payload.encode());clipboard_serial_key(cut_text,1)
            pasted='A'+payload+'\r\nline';ax_phase('clipboard-paste',pasted)
            wait(lambda:state(cursor=1+len(payload.encode())),'foreign native Paste reflected in original view')
            key(44,ctrl=True);ax_phase('expect',cut_text);key(21,ctrl=True);ax_phase('expect',pasted)
            capture('independent system clipboard native Unicode Copy Cut Paste and original Undo Redo')
            clipboard_result['checks'].append('Native Paste receives independent wl-copy UTF-8/CRLF bytes and creates one original Undo entry')
            for invalidation in ('none','caret','preedit','preedit-roundtrip','review','tab'):
                if invalidation.startswith('preedit') and not _candidate_accessible_ime:continue
                clipboard_seed();source=foreign.offer(b'late');clipboard_serial_key(seed,0)
                source.send_signal(signal.SIGSTOP)
                wait(lambda:re.search(r'^State:\s+T',Path(f'/proc/{source.pid}/status').read_text(),re.M) is not None,'foreign native-paste source actually stopped')
                phase='clipboard-paste' if invalidation=='none' else 'clipboard-paste-cancelled' if invalidation in ('review','tab') else 'clipboard-paste-refused'
                revision=view()['revision'];cut=len(log());peer=clipboard_pending(phase,
                    'Alateé🌍\r\nline' if invalidation=='none' else seed)
                wait(lambda:'.receive(' in log()[cut:],'native document requested bytes from the stopped independent provider')
                assert state(cursor=0) and view()['revision']==revision
                if invalidation=='caret':key(106)
                elif invalidation.startswith('preedit'):
                    ime_preedit(0,7)
                    if invalidation=='preedit-roundtrip':ime_preedit(2,0,geometry=False)
                elif invalidation=='review':
                    key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'original review invalidates pending native paste')
                    key(1);wait(lambda:state(review=0),'original review Cancel before late clipboard completion')
                elif invalidation=='tab':
                    active=view()['active'];key(49,ctrl=True);wait(lambda:state(docs=2,cursor=0),'new tab retires native clipboard target')
                    key(15,ctrl=True);wait(lambda:state(active=active),'original clipboard tab restored before late completion')
                source.send_signal(signal.SIGCONT);clipboard_finish(peer)
                ax_phase('expect','Alateé🌍\r\nline' if invalidation=='none' else seed)
                if invalidation=='preedit':
                    assert state(ime=7);ime_preedit(2,0,geometry=False)
                if invalidation=='tab':
                    key(15,ctrl=True);key(17,ctrl=True);wait(lambda:state(docs=1),'empty temporary clipboard tab closes')
                clipboard_result['checks'].append(dict(delayed_native_paste=invalidation,accepted=invalidation=='none'))
            clipboard_seed();source=foreign.offer(b'late');clipboard_serial_key(seed,0);source.send_signal(signal.SIGSTOP)
            wait(lambda:re.search(r'^State:\s+T',Path(f'/proc/{source.pid}/status').read_text(),re.M) is not None,'original keyboard paste provider stopped')
            cut=len(log());key(47,ctrl=True)
            wait(lambda:'PASTE begin\n' in log()[cut:] and '.receive(' in log()[cut:],'original public keyboard paste outstanding')
            ax_phase('clipboard-paste-refused',seed)
            assert 'PASTE cancel=' not in log()[cut:]
            source.send_signal(signal.SIGCONT);wait(lambda:state(cursor=4),'original keyboard paste survives refused native read')
            ax_phase('expect','late'+seed)
            clipboard_result['checks'].append('Refused native clipboard read does not displace the original outstanding keyboard paste token')
            capture('delayed independent native clipboard requests respect original input IME review and tab tokens')
            key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'original clipboard test document close review')
            click('Discard');wait(lambda:state(docs=0,review=0),'original clipboard test document discarded')
            clipboard_result['result']='transfers-passed-close-pending'
        finally:
            foreign.close()
            (root/'clipboard-result.json').write_text(json.dumps(clipboard_result,indent=2)+'\n')
