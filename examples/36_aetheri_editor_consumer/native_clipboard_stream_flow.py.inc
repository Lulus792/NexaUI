            # Additional original-model task; independent wl-copy keeps its
            # actual writer pipe open after partial and complete payload bytes.
            clipboard_result['stream_task']=stream_result=dict(result='running',cases=[])
            stream_task=stream_task_type(root,_candidate_clipboard_tools,env,
                None,None,None,None,None,wait,None,stream_result,None,gate=_candidate_clipboard_stream_gate)
            clipboard_stream_tasks.append(stream_task)
            stream_payload='é🌍\r\nlate';stream_bytes=stream_payload.encode()
            stream_value='A'+stream_payload+'é🌍\r\nline'
            def stream_frames(source,peer,revision,phase):
                observed=[]
                for ordinal in range(2):
                    before=log().count('FRAME ');event(f'M {500+ordinal} {400+ordinal}')
                    wait(lambda:log().count('FRAME ')>before,'original frame while '+phase+' clipboard bytes have no EOF')
                    status=peer.poll()
                    if status is not None:
                        # Preserve the actual reply before teardown closes the
                        # foreign pipe and changes the original owner's state.
                        reply,_=peer.communicate(timeout=1)
                        writer=dict(stream_task.states[source.pid])
                        try:writer['actual_pipe']=os.readlink(f'/proc/{source.pid}/fd/{writer["pipe_fd"]}')
                        except OSError as error:writer['observation_error']=str(error)
                        (root/'clipboard-early-reply.json').write_text(json.dumps(dict(
                            phase=phase,ordinal=ordinal,peer_exit=status,stdout=reply,
                            writer=writer,view=view(),frame=log().count('FRAME ')),indent=2)+'\n')
                    assert status is None,'native paste must not reply before EOF'
                    assert state(cursor=0,selected=0) and view()['revision']==revision,'no partial model edit before EOF'
                    observed.append(dict(frame=log().count('FRAME '),revision=view()['revision'],**stream_task.held(source)))
                return observed
            try:
                key(49,ctrl=True);wait(lambda:state(docs=1,cursor=0),'fresh original partial-stream document')
                for mode in ('complete','truncated','caret','preedit','preedit-roundtrip','review','tab'):
                    if mode.startswith('preedit') and not _candidate_accessible_ime:continue
                    clipboard_seed();source=stream_task.offer(stream_bytes);clipboard_serial_key(seed,0);stream_task.arm(source)
                    phase='clipboard-paste' if mode=='complete' else 'clipboard-paste-cancelled' if mode in ('review','tab') else 'clipboard-paste-refused'
                    cut=len(log());revision=view()['revision'];peer=clipboard_pending(phase,stream_value if mode=='complete' else seed)
                    wait(lambda:'.receive(' in log()[cut:],'native partial-stream transfer actually requested')
                    first=stream_task.message(source,1);assert first['written']==3
                    wait(lambda:stream_task.drained(source),'original owner consumed the incomplete UTF-8 prefix')
                    partial=stream_frames(source,peer,revision,'partial UTF-8');complete=[]
                    if mode!='truncated':
                        stream_task.send(source,b'N');all_bytes=stream_task.message(source,2);assert all_bytes['written']==len(stream_bytes)
                        wait(lambda:stream_task.drained(source),'original owner consumed all bytes while EOF remains held')
                        complete=stream_frames(source,peer,revision,'complete')
                    if mode=='caret':
                        key(106);wait(lambda:state(cursor=1),'original caret invalidates all-bytes/no-EOF paste')
                    elif mode.startswith('preedit'):
                        ime_preedit(0,7)
                        if mode=='preedit-roundtrip':ime_preedit(2,0,geometry=False)
                    elif mode=='review':
                        key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'review retires the all-bytes/no-EOF request')
                        key(1);wait(lambda:state(review=0),'original review Cancel before real EOF')
                    elif mode=='tab':
                        active=view()['active'];key(49,ctrl=True);wait(lambda:state(docs=2,cursor=0),'temporary tab retires partial-stream target')
                        key(15,ctrl=True);wait(lambda:state(active=active),'original tab restored before real EOF')
                    stream_task.send(source,b'E');closed=stream_task.message(source,3)
                    assert not closed['error'] and closed['written']==(3 if mode=='truncated' else len(stream_bytes))
                    clipboard_finish(peer);ax_phase('expect',stream_value if mode=='complete' else seed)
                    if mode=='complete':
                        key(44,ctrl=True);ax_phase('expect',seed);key(21,ctrl=True);ax_phase('expect',stream_value)
                        capture('foreign partial UTF-8 and complete bytes apply once only after actual EOF')
                    elif mode=='preedit':
                        assert state(ime=7);ime_preedit(2,0,geometry=False)
                    elif mode=='tab':
                        key(15,ctrl=True);key(17,ctrl=True);wait(lambda:state(docs=1),'temporary stream tab closes')
                    stream_result['cases'].append(dict(mode=mode,accepted=mode=='complete',partial_frames=partial,
                        complete_without_eof_frames=complete,closed=closed,payload_hex=stream_bytes.hex()))
                capture('late actual EOF cannot revive original caret IME review or tab requests')
                key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'partial-stream original document close review')
                click('Discard');wait(lambda:state(docs=0,review=0),'original partial-stream document discarded')
                stream_result['result']='passed'
            finally:
                stream_task.close()
