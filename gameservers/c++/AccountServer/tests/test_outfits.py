import json, sqlite3
from pathlib import Path
from argon2 import PasswordHasher
from fastapi.testclient import TestClient
from account_server.app import create_app
from account_server.models import Appearance, Settings

PASSWORD='Local test passphrase only 3946'
def login(client,name):
    result=client.post('/v1/auth/login',json={'username':name,'password':PASSWORD})
    assert result.status_code==200,result.text
    return {'Authorization':'Bearer '+result.json()['access_token']}

def test_legacy_account_migration_outfits_and_restart(tmp_path):
    path=tmp_path/'legacy.sqlite3'
    with sqlite3.connect(path) as c:
        c.executescript((Path(__file__).parents[1]/'account_server/schema.sql').read_text())
        c.execute('INSERT INTO schema_version VALUES (1)')
        c.execute('INSERT INTO users VALUES (?,?,?,?,?)',(123,'OldUser','olduser',PasswordHasher().hash(PASSWORD),1))
        c.execute('INSERT INTO profiles VALUES (?,?,?,?,?,?,?,?)',(123,1,7,'Old Display','Old Avatar',Appearance().model_dump_json(),Settings().model_dump_json(),2))
    with TestClient(create_app(path,rate_limit=False)) as client:
        headers=login(client,'OldUser');profile=client.get('/v1/me',headers=headers).json()
        assert profile['user_id']==123 and profile['profile_version']==7
        catalog=client.post('/v1/me/avatar-catalog',headers=headers).json()
        assert {item['asset_id'] for item in catalog['items']}=={900100001,900100002,900100003,527365852}
        appearance=profile['appearance'];appearance['body_colors']['torso']=21
        appearance['clothing']['shirt']=900100001
        changed=client.patch('/v1/me',headers=headers,json={'profile_version':7,'display_name':'New Display','appearance':appearance})
        assert changed.status_code==200,changed.text
        outfit=client.post('/v1/me/outfits',headers=headers,json={'name':'My R6','appearance':appearance})
        assert outfit.status_code==201,outfit.text
        oid=outfit.json()['outfit_id'];assert len(oid)==32
        assert client.patch('/v1/me/outfits/'+oid,headers=headers,json={'name':'Renamed R6'}).status_code==200
        equipped=client.post('/v1/me/outfits/'+oid+'/equip',headers=headers,json={'profile_version':8,'default_on_login':True}).json()
        assert equipped['equipped_outfit_id']==oid and equipped['default_outfit_id']==oid
        client.post('/v1/auth/register',json={'username':'OtherUser','password':PASSWORD})
        other=login(client,'OtherUser')
        assert client.get('/v1/me/outfits',headers=other).json()['outfits']==[]
        for method,payload,url in [('PATCH',{'name':'Stolen'},''),('DELETE',None,''),('POST',{'profile_version':1},'/equip')]:
            assert client.request(method,'/v1/me/outfits/'+oid+url,headers=other,json=payload).status_code==404
        assert client.post('/v1/me/outfits',headers=headers,json={'name':'Bad','appearance':{'body_package':42}}).status_code==403
        assert client.post('/v1/me/outfits',headers=headers,json={'name':'Bad','appearance':{'clothing':{'shirt':'../file'}}}).status_code==422
        assert client.post('/v1/me/outfits',headers=headers,json={'name':'Bad','appearance':appearance,'user_id':123}).status_code==422
        client.post('/v1/auth/logout',headers=headers)
    with TestClient(create_app(path,rate_limit=False)) as restarted:
        headers=login(restarted,'OldUser');profile=restarted.get('/v1/me',headers=headers).json()
        assert profile['display_name']=='New Display' and profile['appearance']==appearance
        assert restarted.get('/v1/me/outfits',headers=headers).json()['outfits'][0]['name']=='Renamed R6'
        assert restarted.delete('/v1/me/outfits/'+oid,headers=headers).status_code==204
        profile=restarted.get('/v1/me',headers=headers).json()
        assert profile['default_outfit_id'] is None and profile['equipped_outfit_id'] is None
        assert profile['appearance']==appearance
    with sqlite3.connect(path) as c:
        assert c.execute('SELECT version FROM schema_version').fetchall()==[(2,)]

def test_outfit_limit_and_default_login(tmp_path,monkeypatch):
    monkeypatch.setenv('ACCOUNT_OUTFIT_LIMIT','1')
    with TestClient(create_app(tmp_path/'db.sqlite3',rate_limit=False)) as client:
        client.post('/v1/auth/register',json={'username':'Example','password':PASSWORD})
        headers=login(client,'Example');appearance=Appearance().model_dump()
        oid=client.post('/v1/me/outfits',headers=headers,json={'name':'Default','appearance':appearance}).json()['outfit_id']
        assert client.post('/v1/me/outfits',headers=headers,json={'name':'Extra','appearance':appearance}).status_code==409
        assert client.post('/v1/me/outfits/'+oid+'/equip',headers=headers,json={'profile_version':1,'default_on_login':True}).status_code==200
        # Simulate a stale profile snapshot from an earlier session; default login restores approved outfit.
        with client.app.state.db.connect() as c:
            c.execute("UPDATE profiles SET appearance_json=?",(json.dumps({'rig_type':'R6','body_colors':{'head':21,'torso':21,'left_arm':21,'right_arm':21,'left_leg':21,'right_leg':21},'body_package':None,'clothing':{'shirt':None,'pants':None,'tshirt':None},'accessories':[],'equipped_assets':[]}),))
        headers=login(client,'Example')
        assert client.get('/v1/me',headers=headers).json()['appearance']==appearance
