"""Two actual clients using the supplied external auth routes and native one-use admission; same PC only."""
import ctypes
import contextlib
from ctypes import wintypes
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

root=Path(__file__).resolve().parent
out=Path(os.getenv('ONLINE_TEST_EVIDENCE', root/'online-admission-evidence'))
out.mkdir(parents=True,exist_ok=True)
port=53940
names=['OnlineA_'+secrets.token_hex(3),'OnlineB_'+secrets.token_hex(3)]
passwords=[secrets.token_urlsafe(24),secrets.token_urlsafe(24)]
base=os.environ.copy()
base.update(XBOX_ACCOUNT_TEST_USERNAME=names[0],XBOX_ACCOUNT_TEST_PASSWORD=passwords[0],XBOX_ACCOUNT_DISABLE_CONTROLLER='1')
fixture=subprocess.Popen(['node',str(root/'original-login/FixtureServer.js'),r'C:\Users\314jax\Downloads\site'],env=base,
                         stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
clients=[]
server=None
temporary_resource=tempfile.TemporaryDirectory()
def close(pid):
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def enum(hwnd,_):
        owner=wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid:ctypes.windll.user32.PostMessageW(hwnd,0x10,0,0)
        return True
    ctypes.windll.user32.EnumWindows(enum,0)
try:
    origin=json.loads(fixture.stdout.readline())['origin']
    with contextlib.nullcontext(temporary_resource.name) as temporary:
        stop=Path(temporary)/'stop';status=Path(temporary)/'status.json'
        server_env=base.copy()
        server_env.update(XBOX_EXTERNAL_ADMISSION='1',XBOX_R16_URL=origin,LAN_SERVER_BIND='127.0.0.1',LAN_SERVER_PORT=str(port),
                          LAN_STATUS_PATH=str(status),LAN_STOP_PATH=str(stop),XBOX_ACCOUNT_URL='http://127.0.0.1:8765')
        server=subprocess.Popen([str(root/'bin/NoOpt/XboxLANServer.exe')],env=server_env,cwd=root,creationflags=subprocess.CREATE_NO_WINDOW)
        deadline=time.monotonic()+40
        while time.monotonic()<deadline:
            assert server.poll() is None,'External-authority server exited'
            if status.exists() and json.loads(status.read_text())['state']=='running':break
            time.sleep(.25)
        else:raise AssertionError('Server not ready')
        for i in range(2):
            env=base.copy();env.update(XBOX_R16_URL=origin,XBOX_ORIGINAL_LOGIN_TEST='1',XBOX_ORIGINAL_LOGIN_TEST_JOIN='1',
                XBOX_ACCOUNT_TEST_USERNAME=names[i],XBOX_ACCOUNT_TEST_PASSWORD=passwords[i],LAN_SERVER_ADDRESS='127.0.0.1',
                LAN_SERVER_PORT=str(port),LAN_CATALOG_URL='http://127.0.0.1:%d'%(port+1),LAN_TEST_LOG='OnlineClient%d.log'%i)
            if i:env['XBOX_ORIGINAL_LOGIN_TEST_REGISTER']='1'
            else:env.pop('XBOX_ORIGINAL_LOGIN_TEST_REGISTER',None)
            log=root/('bin/NoOpt/OnlineClient%d.log'%i)
            if log.exists():log.unlink()
            clients.append(subprocess.Popen([str(root/'bin/NoOpt/XboxClientWindows.exe'),'--originalLogin','--originalLoginSmoke','--localGameplay','--lanGameplay'],env=env,cwd=root))
        deadline=time.monotonic()+100
        while time.monotonic()<deadline:
            assert all(p.poll() is None for p in clients),'Native client exited'
            logs=[(root/('bin/NoOpt/OnlineClient%d.log'%i)).read_text(errors='replace') if (root/('bin/NoOpt/OnlineClient%d.log'%i)).exists() else '' for i in range(2)]
            if all('[OnlineAdmissionTest] PASS:' in log for log in logs):break
            time.sleep(.25)
        else:raise AssertionError('Original login and external-authority join timed out')
        players=json.loads(json.loads(status.read_text())['players_json'])
        assert {(p['username'],p['user_id']) for p in players}=={(names[0],7),(names[1],8)},players
        for i,log in enumerate(logs):
            assert '[Connection] Authenticated character received' in log
            assert passwords[i] not in log
            (out/('client%d.log'%i)).write_text(log)
        invalid=urllib.request.Request('http://127.0.0.1:%d/v1/lan/ticket'%(port+1),data=b'',method='POST',headers={'Authorization':'Bearer '+'0'*64})
        try:urllib.request.urlopen(invalid,timeout=15)
        except urllib.error.HTTPError as error:assert error.code==401
        else:raise AssertionError('Invalid external token accepted')
        for client in clients:close(client.pid);assert client.wait(timeout=25)==0
        time.sleep(1)
        players=json.loads(json.loads(status.read_text())['players_json']);assert players==[]
        stop.write_text('stop');assert server.wait(timeout=25)==0
        server_log=(root/'bin/NoOpt/LANServer.log').read_text(errors='replace')
        for i in range(2):assert '[LAN] Authenticated user: %s (%d)'%(names[i],7+i) in server_log
        assert '0'*64 not in server_log
        (out/'server.log').write_text(server_log)
        print('PASS: two native original-login clients; real supplied register/login/token validation; external IDs 7/8; native one-use proof admission; replicated authenticated characters; invalid token rejected; both disconnects and server shutdown. SAME-PC ONLY; no public-network verification.')
finally:
    for client in clients:
        if client.poll() is None:close(client.pid);client.wait(timeout=25)
    if server and server.poll() is None:
        stop.write_text('stop');server.wait(timeout=25)
    fixture.communicate('stop',timeout=10)
    temporary_resource.cleanup()
