from fastapi.testclient import TestClient
from account_server.app import create_app
from test_accounts import account, service

LIBRARY = 'a'*32
PATH = f'/v1/games/{LIBRARY}/900000100/preferences'

def test_votes_favourites_account_isolation_and_persistence(service):
    client, path = service
    _, alice, _ = account(client)
    _, bob, _ = account(client, 'PreferenceBob')
    assert client.get(PATH).status_code == 401
    assert client.get(PATH, headers=alice).json() == dict(positive=0, negative=0, vote=None, favourite=False)
    for _ in range(3):
        assert client.put(PATH, headers=alice, json={'vote': 1}).json()['positive'] == 1
    result = client.put(PATH, headers=bob, json={'vote': -1}).json()
    assert (result['positive'], result['negative'], result['vote']) == (1, 1, -1)
    assert client.put(PATH, headers=alice, json={'vote': -1}).json()['negative'] == 2
    result = client.put(PATH, headers=alice, json={'vote': None}).json()
    assert (result['positive'], result['negative'], result['vote']) == (0, 1, None)
    assert client.put(PATH, headers=alice, json={'favourite': True}).json()['favourite']
    assert not client.get(PATH, headers=bob).json()['favourite']
    assert client.get('/v1/me/favourites', headers=alice, params={'library_id': LIBRARY}).json()['game_ids'] == [900000100]
    assert client.get('/v1/me/favourites', headers=alice, params={'library_id': 'b'*32}).json()['game_ids'] == []
    with TestClient(create_app(path, rate_limit=False)) as restarted:
        assert restarted.get(PATH, headers=alice).json() == dict(positive=0, negative=1, vote=None, favourite=True)
    assert not client.put(PATH, headers=alice, json={'favourite': False}).json()['favourite']
    client.post('/v1/auth/logout', headers=alice)
    assert client.put(PATH, headers=alice, json={'vote': 1}).status_code == 401

def test_preferences_validation(service):
    client, _ = service
    _, headers, _ = account(client)
    for body in ({'vote': 1, 'user_id': 2}, {'vote': 0}, {'vote': '1'}, {'favourite': 'true'},
                 {'vote': 1, 'favourite': True}, {}, {'favourite': None}):
        assert client.put(PATH, headers=headers, json=body).status_code == 422
    assert client.get(PATH.replace(LIBRARY, 'invalid'), headers=headers).status_code == 422
    assert client.get(PATH.replace('900000100', '-1'), headers=headers).status_code == 422
