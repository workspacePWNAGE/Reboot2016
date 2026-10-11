import sqlite3
from account_server.app import create_app
from fastapi.testclient import TestClient
from test_accounts import account, service

LIBRARY='a'*32
def launch(game=900000100, receipt=None, **extra):
    return dict(library_id=LIBRARY,game_id=game,title='MyTestPlace',
                launch_id=receipt or f'{game:032x}',**extra)

def test_history_requires_auth_and_identity_isolation(service):
    client,_=service
    _,alice,_=account(client)
    bob_id,bob,_=account(client,'HistoryBob')
    assert client.get('/v1/me/games',params={'library_id':LIBRARY}).status_code==401
    assert client.post('/v1/me/games',json=launch()).status_code==401
    assert client.post('/v1/me/games',headers=alice,json=launch(user_id=bob_id)).status_code==422
    assert client.post('/v1/me/games',headers=alice,json=launch()).status_code==200
    assert client.get('/v1/me/games',headers=alice,params={'library_id':LIBRARY}).json()['games'][0]['play_count']==1
    assert client.get('/v1/me/games',headers=bob,params={'library_id':LIBRARY}).json()['games']==[]
    assert client.delete(f'/v1/me/games/{LIBRARY}/900000100',headers=bob).status_code==204
    assert len(client.get('/v1/me/games',headers=alice,params={'library_id':LIBRARY}).json()['games'])==1

def test_history_count_idempotence_and_front_order(service):
    client,_=service
    _,headers,_=account(client)
    assert client.post('/v1/me/games',headers=headers,json=launch()).json()['recorded']
    assert not client.post('/v1/me/games',headers=headers,json=launch()).json()['recorded']
    client.post('/v1/me/games',headers=headers,json=launch(900000101))
    client.post('/v1/me/games',headers=headers,json=launch(receipt='b'*32))
    games=client.get('/v1/me/games',headers=headers,params={'library_id':LIBRARY}).json()['games']
    assert games[0]['game_id']==900000100
    assert games[0]['play_count']==2

def test_history_limits_scope_and_persistence(service):
    client,path=service
    _,headers,token=account(client)
    for i in range(25):
        assert client.post('/v1/me/games',headers=headers,json=launch(900000100+i)).status_code==200
    games=client.get('/v1/me/games',headers=headers,params={'library_id':LIBRARY}).json()['games']
    assert len(games)==20
    assert client.get('/v1/me/games',headers=headers,params={'library_id':'c'*32}).json()['games']==[]
    with TestClient(create_app(path,rate_limit=False)) as restarted:
        assert restarted.get('/v1/me/games',headers=headers,params={'library_id':LIBRARY}).json()['games']==games
    assert client.delete(f'/v1/me/games/{LIBRARY}/{games[0]["game_id"]}',headers=headers).status_code==204
    assert len(client.get('/v1/me/games',headers=headers,params={'library_id':LIBRARY}).json()['games'])==19
    client.post('/v1/auth/logout',headers=headers)
    assert client.post('/v1/me/games',headers=headers,json=launch()).status_code==401

def test_history_rejects_invalid_fields_and_paths(service):
    client,_=service
    _,headers,_=account(client)
    for field,value in [('library_id','../games'),('game_id',-1),('game_id','1'),('title','bad\nname'),('launch_id','invalid')]:
        payload=launch();payload[field]=value
        assert client.post('/v1/me/games',headers=headers,json=payload).status_code==422
    assert client.post('/v1/me/games',headers=headers,json=launch(path='D:/secret.rbxlx')).status_code==422
    assert client.get('/v1/me/games',headers=headers,params={'library_id':'../games'}).status_code==422
