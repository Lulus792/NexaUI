        # Preserve the original task above, then exercise real compositor-close
        # ownership on this same process and native window, including Save.
        observations=[]
        def geometry():
            event('Q')
            rows=re.findall(r'^EDITOR_WINDOW id=(\d+) x=(-?\d+) y=(-?\d+) width=(\d+) height=(\d+) max=(\d+)$',log('server'),re.M)
            assert rows,log('server')[-1500:]
            return dict(zip(('id','x','y','width','height','max'),map(int,rows[-1])))
        def snapshot(label):
            value=geometry();observations.append(dict(label=label,**value));return value
        def latest_requests():return list(map(int,re.findall(r'^NATIVE_CLOSE taken=(\d+)$',log(),re.M)))
        def close_request(number):
            event('C');wait(lambda:len(latest_requests())==number,'native request consumed '+str(number))
            wait(lambda:state(review=document,closing=1),'retained original document close review')
        def current_click(name):
            controls=log().rsplit('FRAME ',2)[-2]
            rows=re.findall(r'^CONTROL \d+ ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) '+re.escape(name)+r'$',controls,re.M)
            assert rows,(name,controls[-2000:])
            x,y,w,h=map(float,rows[-1]);native=geometry()
            event(f'M {native["x"]+x+w/2} {native["y"]+y+h/2}')
            event('B 272 1');event('B 272 0')
        key(49,ctrl=True);wait(lambda:state(docs=1,cursor=0),'new retained close document')
        document=view()['active'];typing('retained');wait(lambda:state(cursor=8,dirty=1),'unsaved retained document')
        original_window=snapshot('before-close')
        close_request(1);capture('native unsaved close review')
        close_request(2)
        requests=latest_requests();assert requests[1]>requests[0] and state(docs=1,review=document,cursor=8)
        releases=log().count('EDITOR_ESCAPE_RELEASE\n')
        key(1);wait(lambda:state(review=0,closing=0,docs=1,active=document,cursor=8) and 'NATIVE_CLOSE cancelled='+str(requests[-1])+'\n' in log(),'latest native close cancelled')
        def active_wait():
            text=log()
            if text.count('EDITOR_ESCAPE_RELEASE\n')!=releases+1:return 0
            tail=text.rsplit('EDITOR_ESCAPE_RELEASE\n',1)[-1]
            begins=re.findall(r'^EDITOR_WAIT begin=(\d+) timeout=4294967295$',tail,re.M)
            if not begins or 'EDITOR_WAIT end='+begins[-1]+'\n' in tail:return 0
            fields=Path(f'/proc/{app.pid}/stat').read_text().rsplit(')',1)[1].split()
            return int(begins[-1]) if fields[0]=='S' else 0
        wait(active_wait,'last Escape release processed and original owner sleeping in native wait')
        idle_wait=active_wait();assert idle_wait
        before=log().count('FRAME ');stat=Path(f'/proc/{app.pid}/stat')
        def ticks():
            fields=stat.read_text().rsplit(')',1)[1].split();return int(fields[11])+int(fields[12])
        initial=ticks();time.sleep(1.2);delta=ticks()-initial
        assert delta<=2 and log().count('FRAME ')==before,(delta,before,log().count('FRAME '))
        # Native polling may return for protocol housekeeping without a UI event.
        # Retain the original no-redraw/CPU budget and require a sleeping owner
        # again; record both wait IDs instead of forbidding legitimate wakeups.
        idle_wait_end=active_wait();assert idle_wait_end,'owner must be sleeping after the idle sample'
        assert stat.read_text().rsplit(')',1)[1].split()[0]=='S','retained native event wait must sleep'
        typing('x');wait(lambda:state(cursor=9,review=0,active=document),'editing after native cancellation')
        native=snapshot('after-cancel');assert native==original_window
        event(f'M {native["x"]+200} {native["y"]+18}');event('B 272 1')
        wait(lambda:'EDITOR_GESTURE edge=0 result=0\n' in log(),'accepted retained-window caption move')
        event(f'M {native["x"]+240} {native["y"]+48}')
        wait(lambda:geometry()['x']==native['x']+40 and geometry()['y']==native['y']+30,'actual native position changed')
        event('B 272 0');moved=snapshot('after-move')
        assert moved['id']==original_window['id'] and moved['width']==native['width'] and moved['height']==native['height']
        event(f'M {moved["x"]+moved["width"]-2} {moved["y"]+moved["height"]-2}');event('B 272 1')
        # NX_WINDOW_RESIZE_BOTTOM_RIGHT is public enum5; its Wayland wire edge is10.
        wait(lambda:'EDITOR_GESTURE edge=5 result=0\n' in log(),'accepted retained-window resize')
        event(f'M {moved["x"]+moved["width"]+58} {moved["y"]+moved["height"]+38}')
        wait(lambda:geometry()['width']==moved['width']+60 and geometry()['height']==moved['height']+40,'actual retained-window resize')
        event('B 272 0');resized=snapshot('after-resize');assert resized['id']==original_window['id']
        wait(lambda:state(width=resized['width'],height=resized['height'],active=document,cursor=9),'original layout after resize')
        typing('z');wait(lambda:state(cursor=10,active=document),'native editing after move and resize')
        current_click('Zoom or restore')
        wait(lambda:geometry()['max']==1 and geometry()['width']==1920 and geometry()['height']==1080 and state(width=1920,height=1080),'committed native zoom extent after cancellation')
        maximum=snapshot('maximized');assert maximum['id']==original_window['id'] and maximum['width']>resized['width']
        current_click('Zoom or restore');wait(lambda:geometry()['max']==0 and geometry()['width']==resized['width'] and geometry()['height']==resized['height'],'restored retained-window extent')
        restored=snapshot('restored');assert restored==resized
        wait(lambda:state(width=restored['width'],height=restored['height'],active=document,cursor=10,review=0),'retained document and focus after zoom')
        capture('retained editor after Cancel move resize and zoom')
        if _candidate_clipboard_tools:clipboard_close_state=clipboard_close_begin()
        close_request(3)
        if _candidate_clipboard_tools:clipboard_close_settled(clipboard_close_state)
        current_click('Save');wait(lambda:state(mode=2,closing=1),'native close preserves original Save picker')
        capture('native close original Save destination')
        typing('native.txt');wait(lambda:state(draft=10),'original native destination filename')
        key(28);assert app.wait(timeout=8)==0;children.remove(app)
        if _candidate_clipboard_tools:clipboard_close_end(clipboard_close_state)
        requests=latest_requests();assert len(requests)==3 and requests==sorted(set(requests))
        assert 'NATIVE_CLOSE accepted='+str(requests[-1])+'\n' in log()
        assert (folder/'native.txt').read_bytes()==b'retainedxz'
        assert (folder/'draft.txt').read_bytes()==b'changed' and (folder/'seed.txt').read_bytes()==b'external'
        (root/'close-result.json').write_text(json.dumps(dict(result='passed',requests=requests,cancelled=requests[1],accepted=requests[2],retained_window=original_window['id'],geometry=observations,idle_wait_start=idle_wait,idle_wait_end=idle_wait_end,idle_cpu_ticks=delta,idle_frames=0,idle_seconds=1.2,saved='retainedxz',scope='Actual private compositor close, original editor models/review/save and real native input/geometry; no physical/human/normal-owner adoption inferred.'),indent=2)+'\n')
