"""Two real native clients, original replicated state. No credentials on command lines."""
import ctypes
from ctypes import wintypes
import json, os, secrets, sqlite3, subprocess, time, urllib.request, socket
from pathlib import Path
ROOT=Path(__file__).resolve().parent
BIN=ROOT/'bin/NoOpt'
GAME_ADDRESS=os.getenv('LAN_TEST_GAME_ADDRESS','127.0.0.1')
CLIENT_ADDRESS=os.getenv('LAN_TEST_CLIENT_ADDRESS',GAME_ADDRESS)
GAME_PORT=int(os.getenv('LAN_TEST_GAME_PORT','53640'))
def api(method,path,payload=None,token=None):
    headers={'Content-Type':'application/json'}
    if token:headers['Authorization']='Bearer '+token
    data=json.dumps(payload).encode() if payload is not None else (b'' if method=='POST' else None)
    with urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8765'+path,data=data,headers=headers,method=method),timeout=10) as response:
        body=response.read();return json.loads(body) if body else None

def close(pid):
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def enum(hwnd,_):
        owner=wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid:ctypes.windll.user32.PostMessageW(hwnd,0x10,0,0)
        return True
    ctypes.windll.user32.EnumWindows(enum,0)
def capture(pid,path):
    if os.environ.get('XBOX_TEST_SKIP_DESKTOP_CAPTURE')=='1':
        return
    from PIL import ImageGrab
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def enum(hwnd,_):
        owner=wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid and ctypes.windll.user32.IsWindowVisible(hwnd):
            rect=wintypes.RECT();ctypes.windll.user32.GetWindowRect(hwnd,ctypes.byref(rect))
            ImageGrab.grab(bbox=(rect.left,rect.top,rect.right,rect.bottom)).save(path)
        return True
    ctypes.windll.user32.EnumWindows(enum,0)
accounts=[];clients=[];server_process=None
trigger=BIN/"LANClientA.leave"
if trigger.exists():trigger.unlink()
try:
    try:api('GET','/health')
    except urllib.error.URLError:
        subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT.parent/'AccountServer/StartBackground.ps1')],check=True)
    # Refuse to interfere with any existing server on the test port.
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as check:
        check.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
        check.bind((GAME_ADDRESS,GAME_PORT))
    server_env=os.environ.copy();server_env.update(LAN_SERVER_BIND=GAME_ADDRESS,LAN_SERVER_PORT=str(GAME_PORT),XBOX_ACCOUNT_URL='http://127.0.0.1:8765')
    server_process=subprocess.Popen([str(BIN/'XboxLANServer.exe')],cwd=ROOT,env=server_env,creationflags=subprocess.CREATE_NO_WINDOW)
    time.sleep(1)
    assert server_process.poll() is None,'Dedicated server startup failed'

    for role,color in [('A',21),('B',28)]:
        name='LAN'+role+'_'+secrets.token_hex(5);password=secrets.token_urlsafe(32)
        uid=api('POST','/v1/auth/register',{'username':name,'password':password})['user_id']
        token=api('POST','/v1/auth/login',{'username':name,'password':password})['access_token']
        with sqlite3.connect(ROOT.parent/'AccountServer/data/accounts.sqlite3') as c:
            for asset,kind in [(900100001,'shirt'),(900100002,'body_package'),(900100003,'accessory')]:
                c.execute('INSERT INTO inventory VALUES (?,?,?,?)',(uid,asset,kind,int(time.time())))
        profile=api('GET','/v1/me',token=token)
        appearance=profile['appearance'];appearance['body_colors']['torso']=color
        appearance['body_package']=900100002;appearance['clothing']['shirt']=900100001;appearance['accessories']=[900100003]
        api('PATCH','/v1/me',{'profile_version':profile['profile_version'],'display_name':'LAN Explorer '+role,'avatar_name':'LAN Avatar '+role,'appearance':appearance},token)
        api('POST','/v1/auth/logout',token=token)
        accounts.append((role,name,password,uid,color))
    # A valid account proof with a forged numeric claim must fail before admission.
    role,name,password,uid,color=accounts[0]
    env=os.environ.copy();env.update(LAN_SERVER_ADDRESS=CLIENT_ADDRESS,LAN_SERVER_PORT=str(GAME_PORT),XBOX_ACCOUNT_URL='http://127.0.0.1:8765',LAN_TEST_LOG='LANRejected.log',XBOX_ACCOUNT_TEST_USERNAME=name,XBOX_ACCOUNT_TEST_PASSWORD=password)
    rejected=subprocess.Popen([str(BIN/'XboxClientWindows.exe'),'--accountAvatarSmokeTest','--localGameplay','--lanGameplay','--lanRejectTest'],cwd=ROOT,env=env)
    clients.append(rejected)
    deadline=time.monotonic()+35
    while time.monotonic()<deadline:
        text=(BIN/'LANServer.log').read_text(errors='replace')
        if 'Claimed UserId does not match authenticated account' in text:break
        time.sleep(.2)
    assert 'Claimed UserId does not match authenticated account' in text,'Forged identity was not rejected'
    assert '[LAN] Player joined:' not in text,'Unauthenticated Player was admitted'
    close(rejected.pid);assert rejected.wait(timeout=20)==0
    clients.clear()
    for role,name,password,uid,color in accounts:
        env=os.environ.copy();env.update(LAN_SERVER_ADDRESS=CLIENT_ADDRESS,LAN_SERVER_PORT=str(GAME_PORT),XBOX_ACCOUNT_URL='http://127.0.0.1:8765',LAN_TEST_LOG='LANClient'+role+'.log',XBOX_ACCOUNT_TEST_USERNAME=name,XBOX_ACCOUNT_TEST_PASSWORD=password)
        if role=='A':env['LAN_TEST_LEAVE_TRIGGER']='LANClientA.leave'
        if role=='B':env['LAN_TEST_MOVE_X']='0.5'
        clients.append(subprocess.Popen([str(BIN/'XboxClientWindows.exe'),'--accountAvatarSmokeTest','--localGameplay','--lanGameplay'],cwd=ROOT,env=env))
    deadline=time.monotonic()+110;captured=False;ready_at=None;positioned=set()
    while time.monotonic()<deadline:
        for i,process in enumerate(clients):
            if process.pid not in positioned:
                @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
                def position(hwnd,_):
                    owner=wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
                    class_name=ctypes.create_unicode_buffer(128);ctypes.windll.user32.GetClassNameW(hwnd,class_name,128)
                    if owner.value==process.pid and class_name.value=='XboxClientWindows':
                        ctypes.windll.user32.MoveWindow(hwnd,i*950,30,940,780,True);positioned.add(process.pid)
                    return True
                ctypes.windll.user32.EnumWindows(position,0)
        logs=[(BIN/('LANClient'+role+'.log')).read_text(errors='replace') if (BIN/('LANClient'+role+'.log')).exists() else '' for role,*_ in accounts]
        if all(accounts[i][1] in log and '[LAN] Remote player ready:' in log for i,log in enumerate(logs)) and ready_at is None:ready_at=time.monotonic()
        if ready_at and time.monotonic()-ready_at>3 and not captured:
            for i,process in enumerate(clients):capture(process.pid,ROOT/('lan-client-'+accounts[i][0]+'.png'))
            captured=True
        if accounts[1][1] in logs[1] and 'returned to retained shell' in logs[1]:trigger.write_text('leave')
        if all(accounts[i][1] in log and 'returned to retained shell' in log for i,log in enumerate(logs)):break
        if any(p.poll() is not None for p in clients):raise AssertionError('Native client exited before completing LAN workflow')
        time.sleep(.25)
    for i,(role,name,password,uid,color) in enumerate(accounts):
        log=logs[i];other=accounts[1-i]
        (ROOT/('lan-client-'+role+'-test.log')).write_text(log)
        assert f'[Player] AUTHENTICATED PLAYER PASS: {name} UserId {uid}' in log
        assert f'[LAN] Remote player ready: {other[1]} ({other[3]}) torso={other[4]}' in log, 'Remote identity/body colors absent'
        assert f'display=LAN Explorer {other[0]} avatar=LAN Avatar {other[0]} shirt=true package=true hat=true' in log, 'Remote avatar/name values absent'
        assert f'[LAN] Player movement replicated: {other[1]}' in log, 'Remote horizontal movement absent'
        assert f'[LAN] Walking joints animated: {name}' in log, 'Local walking joints are not animating'
        assert f'[LAN] Walking joints animated: {other[1]}' in log, 'Remote walking joints are not animating'
        assert '[Avatar] AVATAR COMPONENTS PASS' in log
        assert 'Local animation: Jumping true' in log, 'Local original jump control failed'
        assert f'[LAN] Player jump motion replicated: {other[1]}' in log, 'Remote jumping state absent'
        assert 'returned to retained shell' in log, 'Original Xbox Leave did not return to menu'
    assert f'[LAN] Remote player removed: {accounts[1][1]}' in logs[0], 'B disconnect did not remove remote Player from A'
    server=(BIN/'LANServer.log').read_text(errors='replace')
    for role,name,password,uid,color in accounts:
        assert f'[LAN] Authenticated user: {name} ({uid})' in server
        assert f'[LAN] Player left: {name}' in server
    assert '[LAN] Server player count: 0' in server, 'Server retained a disconnected Player'
    for p in clients:close(p.pid);assert p.wait(timeout=20)==0
    with sqlite3.connect(ROOT.parent/'AccountServer/data/accounts.sqlite3') as c:
        for _,_,_,uid,_ in accounts:assert c.execute('SELECT COUNT(*) FROM sessions WHERE user_id=?',(uid,)).fetchone()[0]==0
    (BIN/'LANServer.stop').write_text('stop')
    assert server_process.wait(timeout=15)==0
    assert '[LAN] Server shutdown cleanly' in (BIN/'LANServer.log').read_text()
    (ROOT/'lan-server-test.log').write_text((BIN/'LANServer.log').read_text())
    print('PASS: local and remote walking joints; jump motion; staggered B removal from A; server player count zero; clean server shutdown; two real clients; server admission; reciprocal names/UserIds/avatars/horizontal movement; both original Leave/menu; server membership cleanup; clean client exit/token revocation')
finally:
    if trigger.exists():trigger.unlink()
    for p in clients:
        if p.poll() is None:
            close(p.pid)
            p.wait(timeout=20)

    if server_process and server_process.poll() is None:
        (BIN/"LANServer.stop").write_text("stop")
        server_process.wait(timeout=15)
