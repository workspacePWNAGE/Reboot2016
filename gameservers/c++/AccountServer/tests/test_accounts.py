import hashlib
import json
import sqlite3
import time
from concurrent.futures import ThreadPoolExecutor

import pytest
from argon2 import PasswordHasher
from fastapi.testclient import TestClient

from account_server.app import create_app

PASSWORD = 'Test-only passphrase 839!'  # Test fixture, never a deployed credential.


@pytest.fixture
def service(tmp_path):
    path = tmp_path / 'accounts.sqlite3'
    app = create_app(path, rate_limit=False)
    with TestClient(app, client=('127.0.0.1', 54321)) as client:
        yield client, path


def account(client, username='Alice'):
    registered = client.post('/v1/auth/register', json={'username': username, 'password': PASSWORD})
    assert registered.status_code == 201, registered.text
    login = client.post('/v1/auth/login', json={'username': username.lower(), 'password': PASSWORD})
    assert login.status_code == 200, login.text
    token = login.json()['access_token']
    return registered.json()['user_id'], {'Authorization': f'Bearer {token}'}, token


def test_register_login_profile_and_hashes(service):
    client, path = service
    uid, headers, token = account(client)
    profile = client.get('/v1/me', headers=headers)
    assert profile.status_code == 200
    assert profile.json()['user_id'] == uid
    assert profile.json()['username'] == 'Alice'
    assert profile.json()['inventory'] == []
    assert profile.json()['appearance']['body_colors']['head'] == 226
    with sqlite3.connect(path) as c:
        encoded = c.execute('SELECT password_hash FROM users').fetchone()[0]
        stored_token = c.execute('SELECT token_hash FROM sessions').fetchone()[0]
    assert encoded.startswith('$argon2id$')
    assert PasswordHasher().verify(encoded, PASSWORD)
    assert stored_token == hashlib.sha256(token.encode()).hexdigest()
    assert token not in path.read_bytes().decode('latin1')
    assert PASSWORD not in path.read_bytes().decode('latin1')


def test_authentication_and_isolation(service):
    client, _ = service
    uid_a, headers_a, _ = account(client)
    uid_b, headers_b, _ = account(client, 'Bob')
    assert uid_a != uid_b
    assert client.get('/v1/me').status_code == 401
    assert client.get('/v1/me', headers={'Authorization': 'Bearer ' + 'x' * 43}).status_code == 401
    for username in ['Alice', 'MissingUser']:
        r = client.post('/v1/auth/login', json={'username': username, 'password': 'Incorrect password 42!'})
        assert r.status_code == 401
        assert r.json()['error']['message'] == 'Invalid username or password'
    assert client.post('/v1/auth/register', json={'username': 'aLiCe', 'password': PASSWORD}).status_code == 409
    assert client.patch('/v1/me', headers=headers_a, json={'profile_version': 1, 'user_id': uid_b}).status_code == 422
    changed = client.patch('/v1/me', headers=headers_a, json={'profile_version': 1, 'display_name': 'Alice local'})
    assert changed.status_code == 200
    assert client.get('/v1/me', headers=headers_b).json()['display_name'] == 'Bob'


def test_profile_persistence_and_concurrency(service):
    client, path = service
    uid, headers, _ = account(client)
    updated = client.patch('/v1/me', headers=headers, json={'profile_version': 1, 'avatar_name': 'Test avatar',
                  'appearance': {'body_colors': {'head': 24, 'torso': 23}},
                  'settings': {'master_volume': 0.4, 'invert_camera_y': True}})
    assert updated.status_code == 200, updated.text
    assert updated.json()['profile_version'] == 2
    with TestClient(create_app(path, rate_limit=False)) as restarted:
        stored = restarted.get('/v1/me', headers=headers).json()
        assert stored['user_id'] == uid
        assert stored['avatar_name'] == 'Test avatar'
        assert stored['appearance']['body_colors']['head'] == 24
        assert stored['settings']['master_volume'] == 0.4
    with ThreadPoolExecutor(2) as pool:
        results = list(pool.map(lambda name: client.patch('/v1/me', headers=headers,
                          json={'profile_version': 2, 'display_name': name}).status_code, ['First', 'Second']))
    assert sorted(results) == [200, 409]


def test_expiry_and_logout(service):
    client, path = service
    _, headers, _ = account(client)
    assert client.post('/v1/auth/logout', headers=headers).status_code == 204
    assert client.get('/v1/me', headers=headers).status_code == 401
    login = client.post('/v1/auth/login', json={'username': 'Alice', 'password': PASSWORD}).json()
    with sqlite3.connect(path) as c:
        c.execute('UPDATE sessions SET expires_at=?', (int(time.time()) - 1,))
    assert client.get('/v1/me', headers={'Authorization': 'Bearer ' + login['access_token']}).status_code == 401


@pytest.mark.parametrize('payload', [
    {'username': '../bad', 'password': PASSWORD},
    {'username': 'Ab', 'password': PASSWORD},
    {'username': 'Valid', 'password': 'short'},
    {'username': 'Valid', 'password': 'x' * 129},
    {'username': 'Valid', 'password': PASSWORD, 'user_id': 1},
    {'username': True, 'password': PASSWORD},
])
def test_invalid_credentials_never_echo_password(service, payload):
    client, _ = service
    response = client.post('/v1/auth/register', json=payload)
    assert response.status_code == 422
    assert payload['password'] not in response.text


def test_profile_validation_and_inventory_authority(service):
    client, path = service
    uid, headers, _ = account(client)
    for invalid in [{'inventory': [{'asset_id': 42}]}, {'username': 'Renamed'}, {'display_name': '\nControl'},
                    {'settings': {'master_volume': 2}}, {'appearance': {'body_colors': {'head': 4}}},
                    {'appearance': {'accessories': [True]}}, {'display_name': None}]:
        assert client.patch('/v1/me', headers=headers, json={'profile_version': 1, **invalid}).status_code == 422
    assert client.patch('/v1/me', headers=headers, json={'profile_version': 1,
                        'appearance': {'body_package': 42}}).status_code == 403
    # Inventory is server-managed; no public grant endpoint exists.
    with sqlite3.connect(path) as c:
        c.execute('INSERT INTO inventory VALUES (?,?,?,?)', (uid, 42, 'body_package', int(time.time())))
    allowed = client.patch('/v1/me', headers=headers, json={'profile_version': 1,
                           'appearance': {'body_package': 42, 'equipped_assets': [42]}})
    assert allowed.status_code == 200
    assert client.patch('/v1/me', headers=headers, json={'profile_version': 2,
                        'appearance': {'clothing': {'shirt': 42}}}).status_code == 403


def test_body_limit_and_content_type(service):
    client, _ = service
    assert client.post('/v1/auth/register', content='x' * 17000, headers={'Content-Type': 'application/json'}).status_code == 413
    assert client.post('/v1/auth/register', content=json.dumps({'username': 'Alice', 'password': PASSWORD}),
                       headers={'Content-Type': 'text/plain'}).status_code == 415
    assert client.post('/v1/auth/register', content='{bad', headers={'Content-Type': 'application/json'}).status_code == 422


def test_rate_limit(tmp_path):
    with TestClient(create_app(tmp_path / 'rate.sqlite3')) as client:
        responses = [client.post('/v1/auth/login', json={'username': 'Nobody', 'password': PASSWORD}) for _ in range(21)]
        assert [r.status_code for r in responses[:20]] == [401] * 20
        assert responses[20].status_code == 429
        assert responses[20].headers['Retry-After'] == '60'


def test_password_salts_rehash_and_session_cap(service):
    client, path = service
    uid, headers, token = account(client)
    account(client, 'Bob')
    with sqlite3.connect(path) as c:
        hashes = [r[0] for r in c.execute('SELECT password_hash FROM users')]
        assert hashes[0] != hashes[1]  # Independent random salts for same password.
        weak = PasswordHasher(memory_cost=8192, time_cost=1, parallelism=1).hash(PASSWORD)
        c.execute('UPDATE users SET password_hash=? WHERE user_id=?', (weak, uid))
    for _ in range(11):
        assert client.post('/v1/auth/login', json={'username': 'Alice', 'password': PASSWORD}).status_code == 200
    with sqlite3.connect(path) as c:
        assert c.execute('SELECT count(*) FROM sessions WHERE user_id=?', (uid,)).fetchone()[0] == 10
        upgraded = c.execute('SELECT password_hash FROM users WHERE user_id=?', (uid,)).fetchone()[0]
    assert not PasswordHasher(time_cost=3, memory_cost=65536, parallelism=4).check_needs_rehash(upgraded)
    assert client.get('/v1/me', headers=headers).status_code == 401  # Oldest token revoked.


@pytest.mark.parametrize('versions', [[], [2], [1, 2]])
def test_unknown_schema_is_rejected_without_changes(tmp_path, versions):
    path = tmp_path / 'future.sqlite3'
    with sqlite3.connect(path) as c:
        c.execute('CREATE TABLE schema_version(version INTEGER PRIMARY KEY)')
        c.executemany('INSERT INTO schema_version VALUES (?)', [(v,) for v in versions])
        c.execute('CREATE TABLE future_data(value TEXT)')
        c.execute("INSERT INTO future_data VALUES ('preserve me')")
    with pytest.raises(RuntimeError, match='Unsupported database schema'):
        create_app(path)
    with sqlite3.connect(path) as c:
        assert [row[0] for row in c.execute('SELECT version FROM schema_version ORDER BY version')] == versions
        assert c.execute('SELECT value FROM future_data').fetchone()[0] == 'preserve me'
        assert c.execute("SELECT name FROM sqlite_master WHERE name='users'").fetchone() is None


def test_existing_unversioned_database_is_not_reused(tmp_path):
    path = tmp_path / 'other.sqlite3'
    with sqlite3.connect(path) as c:
        c.execute('CREATE TABLE other_application(value TEXT)')
    with pytest.raises(RuntimeError, match='Unversioned database'):
        create_app(path)
    with sqlite3.connect(path) as c:
        assert c.execute("SELECT name FROM sqlite_master WHERE name='schema_version'").fetchone() is None


def test_bundled_avatar_assets_are_read_only_and_confined(tmp_path):
    root = tmp_path / 'assets'; root.mkdir()
    (root / '123').write_bytes(b'local mesh bytes')
    with TestClient(create_app(tmp_path / 'db.sqlite3', asset_root=root)) as client:
        assert client.get('/asset/?id=123').content == b'local mesh bytes'
        assert client.get('/asset/?id=456').status_code == 404
        for value in ('../db.sqlite3', '0', '0123', '-1', '9007199254740992'):
            assert client.get('/asset/', params={'id': value}).status_code == 400
        assert client.post('/asset/?id=123', content=b'overwrite').status_code == 405
    assert (root / '123').read_bytes() == b'local mesh bytes'


def test_bundled_avatar_asset_size_limit(tmp_path):
    root = tmp_path / 'assets'; root.mkdir()
    with (root / '123').open('wb') as f: f.truncate(16 * 1024 * 1024 + 1)
    with TestClient(create_app(tmp_path / 'db.sqlite3', asset_root=root)) as client:
        assert client.get('/asset/?id=123').status_code == 413

def test_lan_ticket_single_use_and_authoritative_profile(service):
    client, path = service
    uid, headers, token = account(client)
    proof = client.post('/v1/lan/ticket', headers=headers).json()['ticket']
    assert proof != token
    joined = client.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+proof})
    assert joined.status_code == 200
    assert joined.json()['user_id'] == uid
    assert joined.json()['username'] == 'Alice'
    assert 'access_token' not in joined.json()
    assert client.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+proof}).status_code == 401


def test_lan_ticket_revoked_session_and_bad_proof(service):
    client, path = service
    _, headers, _ = account(client)
    proof = client.post('/v1/lan/ticket', headers=headers).json()['ticket']
    assert client.post('/v1/auth/logout', headers=headers).status_code == 204
    assert client.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+proof}).status_code == 401
    assert client.post('/v1/lan/ticket', headers=headers).status_code == 401
    assert client.post('/v1/lan/redeem').status_code == 401
    assert client.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+'x'*43}).status_code == 401

def test_lan_ticket_expiry_and_remote_redemption(service, monkeypatch):
    client, path = service
    _, headers, _ = account(client)
    proof = client.post('/v1/lan/ticket', headers=headers).json()['ticket']
    import account_server.app as module
    original = module.time.monotonic
    monkeypatch.setattr(module.time, 'monotonic', lambda: original()+31)
    assert client.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+proof}).status_code == 401
    monkeypatch.undo()
    app = create_app(path, rate_limit=False)
    with TestClient(app, client=('192.168.1.22', 54321)) as remote:
        _, remote_headers, _ = account(remote, 'RemoteAlice')
        proof = remote.post('/v1/lan/ticket', headers=remote_headers).json()['ticket']
        assert remote.post('/v1/lan/redeem', headers={'Authorization': 'Bearer '+proof}).status_code == 403
