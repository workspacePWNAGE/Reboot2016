import json, os, socket, subprocess, tempfile, time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
EXE=ROOT/'bin/NoOpt/XboxLANServer.exe'
def wait_status(process,path,state='running',timeout=18):
    end=time.time()+timeout
    while time.time()<end:
        if path.exists():
            data=json.loads(path.read_text())
            if data['pid']==process.pid and data['state']==state:return data
        if process.poll() is not None:raise AssertionError('Unexpected server exit; inspect status/log')
        time.sleep(.1)
    raise AssertionError('Server status timed out')

with tempfile.TemporaryDirectory(prefix='Reboot16 server test ') as name:
    temp=Path(name)
    def start(**settings):
        status=temp/(str(time.time_ns())+'.json');stop=status.with_suffix('.stop')
        env=dict(os.environ,LAN_STATUS_PATH=str(status),LAN_STOP_PATH=str(stop),LAN_SERVER_BIND='127.0.0.1',LAN_SERVER_PORT='53740',LAN_SERVER_GAME_ID='900000100',LAN_SERVER_MAX_PLAYERS='2',XBOX_ACCOUNT_URL='http://127.0.0.1:8765')
        env.update(settings)
        proc=subprocess.Popen([str(EXE)],cwd=ROOT,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
        return proc,status,stop
    def stop(proc,path):
        path.write_text('stop');assert proc.wait(timeout=25)==0
    config=json.loads((ROOT/'RevivalServer.config.json').read_text())
    for change,valid in [({},True),({'port':0},False),({'port':65535},False),({'max_players':129},False),({'port':'53640'},False),({'bind_address':'300.1.2.3'},False),({'server_name':'bad\nname'},False),({'account_password':'x'},False)]:
        path=temp/'config.json';path.write_text(json.dumps({**config,**change}))
        result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'RevivalServerManager.ps1'),'-ValidateOnly','-ConfigPath',str(path)],capture_output=True)
        assert (result.returncode==0)==valid,(change,result.stderr)
    for game_id in (900000100,900000107):
        proc,status,trigger=start(LAN_SERVER_GAME_ID=str(game_id))
        try:
            data=wait_status(proc,status);assert data['max_players']==2 and data['account_ready']
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as conflict:
                conflict.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
                try:conflict.bind(('127.0.0.1',53740))
                except OSError:pass
                else:raise AssertionError('Server not actually listening')
            stop(proc,trigger);assert json.loads(status.read_text())['state']=='stopped'
        finally:
            if proc.poll() is None:stop(proc,trigger)
    for settings in ({'LAN_SERVER_GAME_ID':'2147483647'},{'LAN_SERVER_PORT':'bad'},{'LAN_SERVER_MAX_PLAYERS':'-1'},{'XBOX_ACCOUNT_URL':'http://127.0.0.1:1'}):
        proc,status,trigger=start(**settings);assert proc.wait(timeout=18)!=0
        data=json.loads(status.read_text());assert data['state']=='failed' and data['error']
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as conflict:
        conflict.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1);conflict.bind(('127.0.0.1',53740))
        proc,status,trigger=start();assert proc.wait(timeout=18)!=0
        assert json.loads(status.read_text())['state']=='failed'
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as conflict:
        conflict.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1);conflict.bind(('127.0.0.1',53741));conflict.listen()
        proc,status,trigger=start();assert proc.wait(timeout=18)!=0
        assert json.loads(status.read_text())['state']=='failed'
    # Catalog bind failure must also release the already-opened UDP game socket.
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as released:
        released.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1);released.bind(('127.0.0.1',53740))
print('PASS: config defaults/rejection, two real places, actual UDP listening, clean startup/stop, invalid game/port/limit, account outage, UDP/TCP conflicts and failed-start socket cleanup.')
