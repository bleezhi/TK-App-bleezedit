#!/usr/bin/env python3
"""Local-only SIP registrar + PCMU peer for TK App integration tests.
No third-party modules; never connects to 15.235.62.162.
Not a production SIP server.
"""
import hashlib, json, math, os, re, selectors, socket, struct, sys, threading, time, uuid

sip = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sip.bind(('127.0.0.1', 0))
sip.setblocking(False)
# Bind RTP to all local interfaces. PJSIP may advertise the Windows NIC address
# rather than loopback in SDP; a loopback-only source socket breaks media routing.
rtp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rtp.bind(('0.0.0.0', 0))
rtp.setblocking(False)
sip_port = sip.getsockname()[1]
rtp_port = rtp.getsockname()[1]
nonce = 'tk-local-test-nonce'
realm = 'tk-local-test'
registered_addr = None
registered_contact = None
dialogs = {}
reject_next_dnd = False
lock = threading.Lock()

def emit(**obj):
    with lock:
        print(json.dumps(obj), flush=True)

def parse(data):
    text = data.decode('utf-8', 'replace')
    head, _, body = text.partition('\r\n\r\n')
    lines = head.split('\r\n')
    headers = {}
    for line in lines[1:]:
        if ':' in line:
            k, v = line.split(':', 1)
            headers[k.lower()] = v.strip()
    return lines[0], headers, body

def response(headers, addr, status, reason, body='', extra=()):
    to = headers.get('to', '')
    if ';tag=' not in to and status >= 180:
        to += ';tag=tk-peer'
    lines = [f'SIP/2.0 {status} {reason}', 'Via: '+headers.get('via',''),
             'From: '+headers.get('from',''), 'To: '+to,
             'Call-ID: '+headers.get('call-id',''), 'CSeq: '+headers.get('cseq',''),
             'Server: TK-App-local-tests']
    lines.extend(extra)
    if body: lines.append('Content-Type: application/sdp')
    lines.append('Content-Length: '+str(len(body.encode())))
    sip.sendto(('\r\n'.join(lines)+'\r\n\r\n'+body).encode(), addr)

def auth_valid(method, headers):
    text = headers.get('authorization', '')
    values = dict(re.findall(r'(\w+)=(?:"([^"]*)"|([^,\s]+))', text)) if False else {}
    for match in re.finditer(r'(\w+)=(?:"([^"]*)"|([^,\s]+))', text):
        values[match.group(1)] = match.group(2) if match.group(2) is not None else match.group(3)
    if values.get('username') != '101' or values.get('nonce') != nonce: return False
    md5 = lambda s: hashlib.md5(s.encode()).hexdigest()
    ha1 = md5('101:'+realm+':test-secret')
    ha2 = md5(method+':'+values.get('uri',''))
    if values.get('qop'):
        expected = md5(':'.join([ha1,nonce,values.get('nc',''),values.get('cnonce',''),values['qop'],ha2]))
    else: expected = md5(ha1+':'+nonce+':'+ha2)
    return expected == values.get('response')

def sdp():
    return (f'v=0\r\no=tk 1 1 IN IP4 127.0.0.1\r\ns=TK local test\r\nc=IN IP4 127.0.0.1\r\nt=0 0\r\n'
            f'm=audio {rtp_port} RTP/AVP 0 101\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:101 telephone-event/8000\r\na=fmtp:101 0-15\r\na=sendrecv\r\n')

def remote_rtp(body):
    m = re.search(r'm=audio (\d+)', body)
    c = re.search(r'c=IN IP4 ([0-9.]+)', body)
    return (c.group(1), int(m.group(1))) if m and c else None

def pcmu(sample):
    bias, clip = 0x84, 32635
    sign = 0x80 if sample < 0 else 0
    if sample < 0: sample = -sample
    sample = min(sample, clip) + bias
    exp = 7
    mask = 0x4000
    while exp > 0 and not sample & mask:
        exp -= 1; mask >>= 1
    mantissa = (sample >> (exp+3)) & 0x0f
    return ~(sign | (exp << 4) | mantissa) & 0xff

def media_loop():
    sequence, timestamp = 0, 0
    while True:
        time.sleep(.02)
        try:
            packet, source = rtp.recvfrom(2048)
            if len(packet) >= 16 and packet[1] & 0x7f == 101:
                emit(event='dtmf', digit=packet[12])
        except BlockingIOError: pass
        tone = bytes(pcmu(int(5000*math.sin(2*math.pi*440*(timestamp+i)/8000))) for i in range(160))
        packet = struct.pack('!BBHII', 0x80, 0, sequence & 0xffff, timestamp & 0xffffffff, 0x544b4150)+tone
        for dialog in list(dialogs.values()):
            if dialog.get('rtp') and dialog.get('active'):
                try: rtp.sendto(packet, dialog['rtp'])
                except OSError: pass
        sequence += 1; timestamp += 160

threading.Thread(target=media_loop, daemon=True).start()
emit(event='ready', sip_port=sip_port, rtp_port=rtp_port)
selector = selectors.DefaultSelector()
selector.register(sip, selectors.EVENT_READ, 'sip')
# Windows select() cannot watch stdin pipes. Use a reader thread on both OSes.
import queue
commands = queue.Queue()
def stdin_reader():
    for line in sys.stdin:
        commands.put(line)
    commands.put('')
threading.Thread(target=stdin_reader, daemon=True).start()
stdin_key = type('StdinKey', (), {'data': 'stdin'})()

while True:
    for key, _ in selector.select(.05) + [(stdin_key, None)]:
        if key.data == 'stdin':
            try: line = commands.get_nowait()
            except queue.Empty: continue
            if not line: sys.exit(0)
            try: command = json.loads(line)
            except ValueError: continue
            if command.get('cmd') == 'reject_next_dnd':
                reject_next_dnd = True; emit(event='will_reject_dnd')
            if command.get('cmd') == 'incoming' and registered_addr:
                cid = 'incoming-'+uuid.uuid4().hex
                branch = 'z9hG4bK-'+uuid.uuid4().hex
                contact = registered_contact or f'sip:101@{registered_addr[0]}:{registered_addr[1]}'
                headers = {'via':f'SIP/2.0/UDP 127.0.0.1:{sip_port};branch={branch};rport',
                           'from':'"Local peer" <sip:caller@127.0.0.1>;tag=tk-incoming',
                           'to':f'<{contact}>','call-id':cid,'cseq':'1 INVITE'}
                body = sdp()
                text = f'INVITE {contact} SIP/2.0\r\n'+'\r\n'.join(k.title()+': '+v for k,v in headers.items())
                text += f'\r\nMax-Forwards: 70\r\nContact: <sip:caller@127.0.0.1:{sip_port}>\r\nContent-Type: application/sdp\r\nContent-Length: {len(body)}\r\n\r\n'+body
                dialogs[cid] = {'incoming':True,'headers':headers,'addr':registered_addr,'number':'incoming','active':False}
                sip.sendto(text.encode(), registered_addr)
                emit(event='incoming_sent')
            continue
        data, addr = sip.recvfrom(65535)
        first, h, body = parse(data)
        # Windows/PJSIP may emit an empty UDP datagram during endpoint teardown;
        # it is not a SIP request and must not terminate the test peer.
        if not first.strip():
            continue
        if first.startswith('SIP/2.0'):
            status = int(first.split()[1]); cid=h.get('call-id','')
            if cid in dialogs and dialogs[cid].get('incoming'):
                emit(event='incoming_response', code=status)
                if status == 200 and h.get('cseq','').endswith('INVITE'):
                    dialog=dialogs[cid]; dialog['active']=True; dialog['rtp']=remote_rtp(body)
                    target=re.search(r'<([^>]+)>',h.get('contact',''))
                    target=target.group(1) if target else registered_contact
                    dialog['target']=target
                    lines=[f'ACK {target} SIP/2.0',f'Via: SIP/2.0/UDP 127.0.0.1:{sip_port};branch=z9hG4bK-{uuid.uuid4().hex}',
                           'From: '+h['from'],'To: '+h['to'],'Call-ID: '+cid,'CSeq: 1 ACK','Max-Forwards: 70','Content-Length: 0']
                    sip.sendto(('\r\n'.join(lines)+'\r\n\r\n').encode(),addr)
                elif status >= 300:
                    # ACK a non-2xx response using the original transaction branch.
                    original=dialogs[cid]['headers']; target=registered_contact
                    lines=[f'ACK {target} SIP/2.0','Via: '+original['via'],'From: '+original['from'],'To: '+h['to'],'Call-ID: '+cid,'CSeq: 1 ACK','Content-Length: 0']
                    sip.sendto(('\r\n'.join(lines)+'\r\n\r\n').encode(),addr)
                    dialogs.pop(cid,None)
            continue
        method = first.split()[0]
        if method == 'REGISTER':
            if not auth_valid(method,h):
                response(h,addr,401,'Unauthorized',extra=[f'WWW-Authenticate: Digest realm="{realm}", nonce="{nonce}", algorithm=MD5, qop="auth"'])
                emit(event='auth_challenge')
            else:
                registered_addr=addr
                c=re.search(r'<([^>]+)>',h.get('contact',''))
                registered_contact=c.group(1) if c else None
                exp=h.get('expires','300')
                response(h,addr,200,'OK',extra=['Contact: '+h.get('contact','')+';expires='+exp,'Expires: '+exp])
                emit(event='registered', digest_valid=True)
        elif method == 'INVITE':
            number=first.split()[1].split(':',1)[1].split('@',1)[0]
            emit(event='invite',number=number)
            if number == '13' and reject_next_dnd:
                reject_next_dnd=False; response(h,addr,486,'Busy Here'); continue
            dialogs[h['call-id']]={'number':number,'addr':addr,'headers':h,'rtp':remote_rtp(body),'active':True,'incoming':False}
            response(h,addr,180,'Ringing')
            response(h,addr,200,'OK',sdp(),['Contact: <sip:'+number+f'@127.0.0.1:{sip_port}>'])
        elif method == 'ACK': emit(event='ack')
        elif method == 'BYE':
            response(h,addr,200,'OK'); dialog=dialogs.pop(h.get('call-id',''),{})
            emit(event='bye',number=dialog.get('number','unknown'))
        elif method == 'CANCEL':
            response(h,addr,200,'OK'); dialog=dialogs.pop(h.get('call-id',''),None)
            if dialog: response(dialog['headers'],dialog['addr'],487,'Request Terminated')
        elif method == 'OPTIONS': response(h,addr,200,'OK')
        else: response(h,addr,200,'OK')
