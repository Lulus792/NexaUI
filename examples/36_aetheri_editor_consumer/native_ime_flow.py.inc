        # Actual text-input-v3 callbacks and the original document/save model.
        # Provider snapshots are processed barriers, not an older enable line.
        ime_checks=[]
        def ime_contexts(text=None):
            return re.findall(r'^EDITOR_IME_CONTEXT target=(\d+) epoch=(\d+) bytes=(\d+) cursor=(\d+) anchor=(\d+) revision=(\d+) document_cursor=(\d+)$',log() if text is None else text,re.M)
        def ime_ready():
            rows=ime_contexts()
            return bool(rows) and state(revision=rows[-1][5],cursor=rows[-1][6])
        def ime_snapshot(value,cursor,anchor=None,after=0):
            raw=value.encode() if isinstance(value,str) else value
            expected=(cursor,cursor if anchor is None else anchor,raw.hex())
            def matches():
                rows=re.findall(r'^IME state enabled=(\d+) serial=(\d+) cursor=(-?\d+) anchor=(-?\d+) hex=([0-9a-f]*)$',log('server')[after:],re.M)
                return bool(rows) and rows[-1][0]=='1' and (int(rows[-1][2]),int(rows[-1][3]),rows[-1][4])==expected
            wait(matches,'processed Editor IME context '+repr((len(raw),cursor,anchor)))
            wait(ime_ready,'original rendered document matches the published IME model revision')
            ime_checks.append(dict(context_bytes=len(raw),cursor=cursor,anchor=cursor if anchor is None else anchor))
        def ime_expect(value,cursor,anchor=None,after=0):
            wait(lambda:state(cursor=cursor,mode=0,review=0),'original Editor IME cursor '+str(cursor))
            ime_snapshot(value,cursor,anchor,after)
        def ime_area():
            rows=re.findall(r'^IME commit enabled=1 serial=\d+ area=(-?\d+),(-?\d+),(\d+),(\d+)$',log('server'),re.M)
            assert rows,log('server')[-1500:]
            area=tuple(map(int,rows[-1]));assert area[2]>0 and area[3]>0
            assert 0<=area[0]<float(view()['width']) and 0<=area[1]<float(view()['height']),area
            return area
        def ime_preedit(command,length,geometry=True):
            cut=len(log());old_area=ime_area();event('I '+str(command))
            wait(lambda:re.search(r'^EDITOR_PREEDIT bytes='+str(length)+r' ',log()[cut:],re.M) is not None,'native Editor preedit '+str(length))
            wait(lambda:state(ime=length),'original renderer publishes native preedit '+str(length))
            if geometry:wait(lambda:ime_area()!=old_area,'processed native preedit candidate rectangle '+str(length))
        def ime_seed():
            wait(ime_ready,'original model/render settled before replacing the seed')
            previous_revision=int(view()['revision']);app_cut=len(log());cut=len(log('server'));key(30,ctrl=True);key(14);typing('abcde')
            def replaced():
                rows=ime_contexts(log()[app_cut:])
                return bool(rows) and rows[-1][2:5]==('5','5','5') and int(rows[-1][5])>previous_revision
            wait(replaced,'fresh seed text revision published after replacement, not only a modifier event')
            ime_expect('abcde',5,after=cut)
        def ime_style(name):
            # Called after the original selected-document style capture.
            ime_snapshot('changed',7,0);before=view()['revision'];area=ime_area()
            ime_preedit(0,7);assert state(cursor=7,selected=1,begin=0,end=7) and view()['revision']==before
            ime_snapshot('changed',7,0);current=ime_area();assert current[0]!=area[0] and current[1]==area[1]
            capture(name+' native composition at original scale/density')
            event('I 1');ime_expect('é',2);key(44,ctrl=True);ime_expect('changed',7)
            wait(lambda:state(dirty=0),'native style Undo restores acknowledged original file state')
        key(49,ctrl=True);wait(lambda:state(docs=1,cursor=0),'new original IME document')
        assert 'EDITOR_IME available=1\n' in log()
        typing('abcde');ime_expect('abcde',5);initial=view()['revision'];area=ime_area()
        ime_preedit(0,7);ime_snapshot('abcde',5);assert state(cursor=5) and view()['revision']==initial
        current=ime_area();assert current[0]>area[0] and current[1]==area[1]
        capture('native preedit keeps original document and history uncommitted')
        event('I 1');ime_expect('abcdeé',7);key(44,ctrl=True);ime_expect('abcde',5);key(21,ctrl=True);ime_expect('abcdeé',7)
        capture('native Unicode commit through original document and Undo Redo')
        ime_seed();provider_cut=len(log('server'));event('I 28');ime_expect('abcdex',6,after=provider_cut)
        wait(lambda:state(ime=3),'new composition survives acknowledgment of its simultaneous commit')
        capture('native commit and continuing preedit share one done callback')
        event('I 1');ime_expect('abcdexé',8);key(44,ctrl=True);ime_expect('abcdex',6);key(44,ctrl=True);ime_expect('abcde',5)
        ime_seed();initial=view()['revision'];area=ime_area();ime_preedit(29,7,geometry=False)
        assert state(ime_hidden=1,cursor=5) and view()['revision']==initial and ime_area()==area
        capture('native preedit respects the hidden caret')
        ime_preedit(2,0,geometry=False);ime_expect('abcde',5)
        for mode,value in ((16,'abcdxé'),(18,'abcd'),(19,'abcd'),(24,'abcdeé')):
            ime_seed();event('I '+str(mode));ime_expect(value,len(value.encode()))
            key(44,ctrl=True);ime_expect('abcde',5);key(21,ctrl=True);ime_expect(value,len(value.encode()))
            ime_checks.append(dict(transaction=mode,one_undo=True))
        ime_seed();event('I 20');ime_expect('abcdexy',7)
        key(44,ctrl=True);ime_expect('abcdex',6);key(44,ctrl=True);ime_expect('abcde',5)
        key(21,ctrl=True);ime_expect('abcdex',6);key(21,ctrl=True);ime_expect('abcdexy',7)
        ime_checks.append('two coqueued native commits have separate original undo entries')
        ime_seed();key(30,ctrl=True);ime_snapshot('abcde',5,0);event('I 21');ime_expect('',0)
        key(44,ctrl=True);ime_expect('abcde',5)
        ime_seed();cut=len(log());provider_cut=len(log('server'));event('I 17')
        wait(lambda:'EDITOR_IME_BATCH outcome=3 result=1\n' in log()[cut:],'filtered native commit rejects its deletion too')
        ime_expect('abcde',5,after=provider_cut);ime_checks.append('filtered delete+commit leaves the complete original document intact')
        key(30,ctrl=True);key(14);ime_expect('',0);event('I 26');ime_expect('aX́b',5)
        key(102);key(106);ime_expect('aX́b',1);event('I 25');ime_expect('aq́b',4)
        key(44,ctrl=True);ime_expect('aX́b',1);key(21,ctrl=True);ime_expect('aq́b',4)
        capture('native atomic byte replacement avoids intermediate grapheme caret')
        key(44,ctrl=True);ime_expect('aX́b',1);cut=len(log());event('I 27')
        wait(lambda:'EDITOR_IME_BATCH outcome=3 result=1\n' in log()[cut:],'invalid second coqueued scalar range is rejected')
        ime_expect('aq́b',4);key(44,ctrl=True);ime_expect('aX́b',1)
        ime_seed();prefix_area=ime_area();event('Q')
        window=re.findall(r'^EDITOR_WINDOW id=\d+ x=(-?\d+) y=(-?\d+) width=\d+ height=\d+ max=\d+$',log('server'),re.M)[-1]
        window_x,window_y=map(int,window);assert view()['density']=='1.00'
        capture('original document prefix before long native preedit');prefix_before=images[-3]
        ime_preedit(3,1000);before=view()['revision'];ime_snapshot('abcde',5);ime_area()
        capture('long native preedit scrolls within the original text viewport')
        prefix_after=images[-3]
        def prefix_pixels(entry):
            raw=gzip.decompress((captures/entry['file']).read_bytes());_,dimensions,_,pixels=raw.split(b'\n',3);width,height=map(int,dimensions.split())
            x0,x1=window_x,window_x+prefix_area[0];y0=window_y+prefix_area[1];y1=y0+prefix_area[3]
            assert 0<=x0<x1<=width and 0<=y0<y1<=height
            return b''.join(pixels[(y*width+x0)*3:(y*width+x1)*3] for y in range(y0,y1))
        before_pixels=prefix_pixels(prefix_before);after_pixels=prefix_pixels(prefix_after)
        assert len(set(before_pixels))>10 and before_pixels==after_pixels,'long preedit overdraws the original document prefix'
        ime_checks.append(dict(original_prefix_rgb_sha256=hashlib.sha256(before_pixels).hexdigest(),prefix_pixels_unchanged=True))
        ime_preedit(2,0);assert view()['revision']==before;ime_expect('abcde',5)
        ime_preedit(0,7);provider_cut=len(log('server'));previous_epoch=int(ime_contexts()[-1][1]);releases=log().count('EDITOR_ESCAPE_RELEASE\n')
        event('I 7')
        wait(lambda:log().count('EDITOR_ESCAPE_RELEASE\n')==releases+1,'queued native Escape release processed')
        wait(lambda:int(ime_contexts()[-1][1])>previous_epoch,'new original input context after queued Escape')
        ime_expect('abcde',5,after=provider_cut)
        ime_checks.append('native Escape invalidates the queued old commit')
        ime_preedit(0,7);event('I 8');ime_expect('abcde\t',6)
        ime_checks.append('original Tab indentation survives while queued old IME commit is rejected')
        ime_seed();ime_preedit(0,7);focused=view()['focus'];key(64)
        wait(lambda:view()['focus']!=focused,'F6 leaves document composition')
        wait(lambda:log('server').rfind('IME commit enabled=0 ')>log('server').rfind('IME commit enabled=1 '),'document IME disabled at toolbar')
        key(64);wait(lambda:view()['focus']==focused,'F6 restores original document focus');ime_expect('abcde',5)
        event('I 1');ime_expect('abcdeé',7)
        key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'original close review after native editing')
        key(1);wait(lambda:state(review=0,docs=1,cursor=7),'review Cancel retains native-edited document');ime_snapshot('abcdeé',7)
        key(17,ctrl=True);wait(lambda:int(view()['review'])!=0,'discard original IME test document')
        click('Discard');wait(lambda:state(docs=0,review=0),'IME test document retired by original owner')
        large=('é'*3000+'abc').encode();(folder/'ime-large.txt').write_bytes(large)
        key(24,ctrl=True);wait(lambda:state(mode=1),'original Open for bounded IME context')
        click('ime-large.txt');key(28);wait(lambda:state(mode=0,docs=1),'original large document opened')
        key(107,ctrl=True);wait(lambda:state(cursor=len(large)),'original large document EOF')
        ime_snapshot(large[2004:],3999);assert len(large)>4000
        event('I 1');large+='é'.encode();wait(lambda:state(cursor=len(large)),'IME insertion after bounded surrounding window')
        ime_snapshot(large[2006:],3999)
        key(30,ctrl=True);wait(lambda:state(selected=1,begin=0,end=len(large)),'selection exceeds native surrounding budget')
        ime_snapshot('',0);event('I 21');ime_expect('',0)
        key(44,ctrl=True);wait(lambda:state(cursor=len(large)),'one Undo restores complete large original model');ime_snapshot(large[2006:],3999)
        key(31,ctrl=True);wait(lambda:state(io=0,dirty=0),'original save worker acknowledges native large-document edit')
        assert (folder/'ime-large.txt').read_bytes()==large
        key(17,ctrl=True);wait(lambda:state(docs=0,review=0),'clean large document closes normally')
        ime_checks.append(dict(large_document_bytes=len(large),maximum_surrounding_bytes=4000,saved_sha256=hashlib.sha256(large).hexdigest()))
        (root/'ime-result.json').write_text(json.dumps(dict(result='passed',checks=ime_checks,scope='Controlled native text-input-v3 to the original document/history/save model. Physical/installed IME, editable accessibility and normal Aetheri adoption remain open.'),indent=2)+'\n')
