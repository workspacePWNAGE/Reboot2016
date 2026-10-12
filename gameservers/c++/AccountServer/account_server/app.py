import hashlib
import json
import os
import secrets
import sqlite3
import threading
import time
from collections import OrderedDict, deque
from pathlib import Path

from argon2 import PasswordHasher
from argon2.exceptions import VerificationError
from fastapi import Depends, FastAPI, HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse, FileResponse
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from starlette.exceptions import HTTPException as StarletteHTTPException

from .database import Database
from .models import Appearance, Credentials, ProfilePatch, Settings, GameLaunch, GamePreference, OutfitCreate, OutfitRename, OutfitEquip


class BodyLimit:
    """Bound request bytes before JSON parsing; no credentials enter logs/errors."""
    def __init__(self, app, limit=16384):
        self.app, self.limit = app, limit

    async def __call__(self, scope, receive, send):
        if scope['type'] != 'http':
            return await self.app(scope, receive, send)
        body = bytearray()
        while True:
            message = await receive()
            if message['type'] == 'http.disconnect':
                return
            body.extend(message.get('body', b''))
            if len(body) > self.limit:
                response = JSONResponse({'error': {'code': 'body_too_large', 'message': 'Request exceeds 16 KiB'}}, 413)
                return await response(scope, receive, send)
            if not message.get('more_body', False):
                break
        consumed = False

        async def replay():
            nonlocal consumed
            if consumed:
                return await receive()
            consumed = True
            return {'type': 'http.request', 'body': bytes(body), 'more_body': False}

        async def safe_send(message):
            if message['type'] == 'http.response.start':
                message['headers'] += [(b'cache-control', b'no-store'), (b'x-content-type-options', b'nosniff')]
            await send(message)
        await self.app(scope, replay, safe_send)


class RateLimit:
    """One-process bounded sliding window. Never trust forwarded client IPs."""
    def __init__(self):
        self.entries = OrderedDict()
        self.lock = threading.Lock()

    def check(self, keys):
        now = time.monotonic()
        with self.lock:
            for key, maximum in keys:
                queue = self.entries.setdefault(key, deque())
                while queue and queue[0] <= now - 60:
                    queue.popleft()
                self.entries.move_to_end(key)
                if len(queue) >= maximum:
                    raise HTTPException(429, 'Too many authentication attempts', headers={'Retry-After': '60'})
            for key, _ in keys:
                self.entries[key].append(now)
            while len(self.entries) > 4096:
                self.entries.popitem(last=False)


def create_app(database_path=None, session_seconds=86400, rate_limit=True, asset_root=None):
    db = Database(database_path or os.getenv('ACCOUNT_DB', Path(__file__).parents[1] / 'data/accounts.sqlite3'))
    hasher = PasswordHasher(time_cost=3, memory_cost=65536, parallelism=4)
    hash_slots = threading.BoundedSemaphore(2)
    dummy_hash = hasher.hash(secrets.token_urlsafe(32))
    limiter = RateLimit()
    bearer = HTTPBearer(auto_error=False)
    app = FastAPI(title='XboxClientWindows local accounts', version='1.0.0')
    app.add_middleware(BodyLimit)
    app.state.db = db
    outfit_limit = int(os.getenv('ACCOUNT_OUTFIT_LIMIT', '20'))
    if not 1 <= outfit_limit <= 100:
        raise ValueError('ACCOUNT_OUTFIT_LIMIT must be between 1 and 100')
    local_models = Path(__file__).parents[2] / 'XboxClientWindows/content/offline-avatar/assets'
    bundled = [(900100001, 'shirt', 'Local demo shirt'),
               (900100002, 'body_package', 'Local R6 body package'),
               (900100003, 'accessory', 'Local accessory'),
               (527365852, 'accessory', 'Gift13')]

    @app.exception_handler(RequestValidationError)
    async def invalid_request(request, exc):
        # FastAPI normally includes the rejected input; that can be a password.
        return JSONResponse({'error': {'code': 'validation_error', 'message': 'Invalid request fields',
                                     'fields': [list(e['loc']) for e in exc.errors()]}}, 422)

    @app.exception_handler(StarletteHTTPException)
    async def http_error(request, exc):
        return JSONResponse({'error': {'code': str(exc.status_code), 'message': exc.detail}},
                            exc.status_code, headers=exc.headers)

    def json_request(request: Request):
        if request.headers.get('content-type', '').split(';')[0].lower() != 'application/json':
            raise HTTPException(415, 'Use application/json')

    def auth_limit(request, username, operation):
        if rate_limit:
            ip = request.client.host if request.client else 'unknown'
            limiter.check([(f'ip:{ip}', 30), (f'{operation}:{ip}', 5 if operation == 'register' else 20),
                           (f'user:{username.lower()}', 20)])

    def identity(credentials: HTTPAuthorizationCredentials | None = Depends(bearer)):
        if not credentials or credentials.scheme.lower() != 'bearer' or not 40 <= len(credentials.credentials) <= 128:
            raise HTTPException(401, 'Invalid or expired session', headers={'WWW-Authenticate': 'Bearer'})
        token_hash = hashlib.sha256(credentials.credentials.encode()).hexdigest()
        with db.connect() as c:
            row = c.execute('SELECT user_id FROM sessions WHERE token_hash=? AND expires_at>?',
                            (token_hash, int(time.time()))).fetchone()
        if not row:
            raise HTTPException(401, 'Invalid or expired session', headers={'WWW-Authenticate': 'Bearer'})
        return row['user_id'], token_hash

    def profile(c, user_id):
        row = c.execute('SELECT u.user_id,u.username,p.* FROM profiles p JOIN users u USING(user_id) WHERE user_id=?',
                        (user_id,)).fetchone()
        inventory = [dict(r) for r in c.execute('SELECT asset_id,kind,acquired_at FROM inventory WHERE user_id=? ORDER BY asset_id', (user_id,))]
        return {'schema_version': row['schema_version'], 'profile_version': row['profile_version'],
                'user_id': user_id, 'username': row['username'], 'display_name': row['display_name'],
                'avatar_name': row['avatar_name'], 'appearance': json.loads(row['appearance_json']),
                'settings': json.loads(row['settings_json']), 'inventory': inventory, 'updated_at': row['updated_at'],
                'equipped_outfit_id': row['equipped_outfit_id'], 'default_outfit_id': row['default_outfit_id']}

    def check_appearance(c, user_id, appearance):
        owned = {r['asset_id']: r['kind'] for r in c.execute('SELECT asset_id,kind FROM inventory WHERE user_id=?', (user_id,))}
        selected = [(appearance.body_package, 'body_package')]
        selected += [(value, key) for key, value in appearance.clothing.model_dump().items()]
        selected += [(value, 'accessory') for value in appearance.accessories]
        selected += [(value, None) for value in appearance.equipped_assets]
        if any(asset is not None and (asset not in owned or (kind and owned[asset] != kind)) for asset, kind in selected):
            raise HTTPException(403, 'Equipped assets must be owned and match their slot')

    @app.post('/v1/me/avatar-catalog')
    def avatar_catalog(who=Depends(identity)):
        # Explicit claim of the server's free bundled demo items, not an arbitrary
        # client-supplied inventory grant. Old registration behavior is preserved.
        items = []
        with db.connect() as c:
            for asset_id, kind, name in bundled:
                path = local_models / f'{asset_id}.rbxm'
                if path.is_file() and not path.is_symlink() and path.stat().st_size <= 16*1024*1024:
                    c.execute('INSERT OR IGNORE INTO inventory VALUES (?,?,?,?)', (who[0], asset_id, kind, int(time.time())))
                    items.append({'asset_id': asset_id, 'kind': kind, 'name': name})
        return {'items': items, 'outfit_limit': outfit_limit,
                'body_colors': sorted(__import__('account_server.models', fromlist=['PALETTE']).PALETTE),
                'default_appearance': Appearance().model_dump()}

    def outfit(c, owner, outfit_id):
        row = c.execute('SELECT * FROM outfits WHERE user_id=? AND outfit_id=?', (owner, outfit_id)).fetchone()
        if not row:
            raise HTTPException(404, 'Outfit unavailable')
        return row

    @app.get('/v1/me/outfits')
    def list_outfits(who=Depends(identity)):
        with db.connect() as c:
            rows = [dict(r) for r in c.execute('SELECT outfit_id,name,appearance_json,created_at,updated_at FROM outfits WHERE user_id=? ORDER BY created_at,outfit_id', (who[0],))]
            for row in rows:
                row['appearance'] = json.loads(row.pop('appearance_json'))
            return {'outfits': rows, 'limit': outfit_limit}

    @app.post('/v1/me/outfits', dependencies=[Depends(json_request)], status_code=201)
    def save_outfit(payload: OutfitCreate, who=Depends(identity)):
        with db.connect() as c:
            c.execute('BEGIN IMMEDIATE')
            check_appearance(c, who[0], payload.appearance)
            if c.execute('SELECT COUNT(*) FROM outfits WHERE user_id=?', (who[0],)).fetchone()[0] >= outfit_limit:
                raise HTTPException(409, 'Saved outfit limit reached')
            stable_id = secrets.token_hex(16)
            now = int(time.time())
            c.execute('INSERT INTO outfits VALUES (?,?,?,?,?,?)', (stable_id, who[0], payload.name, payload.appearance.model_dump_json(), now, now))
            return {'outfit_id': stable_id, 'name': payload.name}

    @app.patch('/v1/me/outfits/{outfit_id}', dependencies=[Depends(json_request)])
    def rename_outfit(outfit_id: str, payload: OutfitRename, who=Depends(identity)):
        with db.connect() as c:
            outfit(c, who[0], outfit_id)
            c.execute('UPDATE outfits SET name=?,updated_at=? WHERE user_id=? AND outfit_id=?', (payload.name, int(time.time()), who[0], outfit_id))
        return {'outfit_id': outfit_id, 'name': payload.name}

    @app.delete('/v1/me/outfits/{outfit_id}', status_code=204)
    def delete_outfit(outfit_id: str, who=Depends(identity)):
        with db.connect() as c:
            c.execute('BEGIN IMMEDIATE')
            outfit(c, who[0], outfit_id)
            c.execute('DELETE FROM outfits WHERE user_id=? AND outfit_id=?', (who[0], outfit_id))
            c.execute('UPDATE profiles SET equipped_outfit_id=CASE WHEN equipped_outfit_id=? THEN NULL ELSE equipped_outfit_id END, default_outfit_id=CASE WHEN default_outfit_id=? THEN NULL ELSE default_outfit_id END, profile_version=profile_version+1,updated_at=? WHERE user_id=? AND (equipped_outfit_id=? OR default_outfit_id=?)', (outfit_id, outfit_id, int(time.time()), who[0], outfit_id, outfit_id))

    @app.post('/v1/me/outfits/{outfit_id}/equip', dependencies=[Depends(json_request)])
    def equip_outfit(outfit_id: str, payload: OutfitEquip, who=Depends(identity)):
        with db.connect() as c:
            c.execute('BEGIN IMMEDIATE')
            selected = outfit(c, who[0], outfit_id)
            old = profile(c, who[0])
            if payload.profile_version != old['profile_version']:
                raise HTTPException(409, 'Profile changed; retrieve it again before updating')
            check_appearance(c, who[0], Appearance.model_validate_json(selected['appearance_json']))
            c.execute('UPDATE profiles SET appearance_json=?,equipped_outfit_id=?,default_outfit_id=?,profile_version=profile_version+1,updated_at=? WHERE user_id=?', (selected['appearance_json'], outfit_id, outfit_id if payload.default_on_login else old['default_outfit_id'], int(time.time()), who[0]))
            return profile(c, who[0])

    @app.get('/health')
    def health():
        return {'status': 'ok', 'service': 'local-accounts', 'api_version': 1}

    @app.get('/v1/me/games')
    def recent_games(library_id: str, owner=Depends(identity)):
        import re
        if not re.fullmatch(r'[a-f0-9]{32}', library_id):
            raise HTTPException(422, 'Invalid library ID')
        with db.connect() as c:
            rows = [dict(r) for r in c.execute(
                'SELECT game_id,title,last_played,play_count FROM game_history '
                'WHERE user_id=? AND library_id=? ORDER BY last_played DESC,rowid DESC LIMIT 20',
                (owner[0], library_id))]
        return {'library_id': library_id, 'games': rows}

    @app.post('/v1/me/games', dependencies=[Depends(json_request)])
    def record_game(payload: GameLaunch, owner=Depends(identity)):
        now = int(time.time())
        with db.connect() as c:
            # Serial transaction makes retries idempotent and keeps account
            # ownership derived exclusively from the validated bearer session.
            c.execute('BEGIN IMMEDIATE')
            inserted = c.execute('INSERT OR IGNORE INTO game_launch_receipts VALUES (?,?,?)',
                                 (owner[0], payload.launch_id, now)).rowcount
            if inserted:
                row = c.execute('SELECT play_count FROM game_history WHERE user_id=? AND library_id=? AND game_id=?',
                                (owner[0], payload.library_id, payload.game_id)).fetchone()
                c.execute('INSERT OR REPLACE INTO game_history VALUES (?,?,?,?,?,?)',
                          (owner[0], payload.library_id, payload.game_id, payload.title, now,
                           row['play_count'] + 1 if row else 1))
                c.execute('DELETE FROM game_history WHERE user_id=? AND library_id=? AND game_id NOT IN '
                          '(SELECT game_id FROM game_history WHERE user_id=? AND library_id=? '
                          'ORDER BY last_played DESC,rowid DESC LIMIT 20)',
                          (owner[0], payload.library_id, owner[0], payload.library_id))
                c.execute('DELETE FROM game_launch_receipts WHERE user_id=? AND launch_id NOT IN '
                          '(SELECT launch_id FROM game_launch_receipts WHERE user_id=? ORDER BY recorded_at DESC,rowid DESC LIMIT 100)',
                          (owner[0], owner[0]))
        return {'recorded': bool(inserted)}

    @app.delete('/v1/me/games/{library_id}/{game_id}', status_code=204)
    def remove_game(library_id: str, game_id: int, owner=Depends(identity)):
        with db.connect() as c:
            c.execute('DELETE FROM game_history WHERE user_id=? AND library_id=? AND game_id=?',
                      (owner[0], library_id, game_id))

    def validate_game_key(library_id, game_id=None):
        import re
        if not re.fullmatch(r'[a-f0-9]{32}', library_id) or (game_id is not None and not 0 < game_id <= 2147483647):
            raise HTTPException(422, 'Invalid library/game ID')

    def game_social(c, user_id, library_id, game_id):
        totals = c.execute('SELECT SUM(vote=1) AS positive,SUM(vote=-1) AS negative FROM game_votes WHERE library_id=? AND game_id=?',
                           (library_id, game_id)).fetchone()
        own = c.execute('SELECT vote FROM game_votes WHERE user_id=? AND library_id=? AND game_id=?',
                        (user_id, library_id, game_id)).fetchone()
        favourite = c.execute('SELECT 1 FROM game_favourites WHERE user_id=? AND library_id=? AND game_id=?',
                             (user_id, library_id, game_id)).fetchone()
        return {'positive': totals['positive'] or 0, 'negative': totals['negative'] or 0,
                'vote': own['vote'] if own else None, 'favourite': bool(favourite)}

    @app.get('/v1/me/favourites')
    def favourites(library_id: str, owner=Depends(identity)):
        validate_game_key(library_id)
        with db.connect() as c:
            ids = [r['game_id'] for r in c.execute('SELECT game_id FROM game_favourites WHERE user_id=? AND library_id=? ORDER BY game_id',
                                                  (owner[0], library_id))]
        return {'game_ids': ids}

    @app.get('/v1/games/{library_id}/{game_id}/preferences')
    def get_game_social(library_id: str, game_id: int, owner=Depends(identity)):
        validate_game_key(library_id, game_id)
        with db.connect() as c:
            return game_social(c, owner[0], library_id, game_id)

    @app.put('/v1/games/{library_id}/{game_id}/preferences', dependencies=[Depends(json_request)])
    def put_game_social(library_id: str, game_id: int, payload: GamePreference, owner=Depends(identity)):
        validate_game_key(library_id, game_id)
        with db.connect() as c:
            c.execute('BEGIN IMMEDIATE')
            key = (owner[0], library_id, game_id)
            if 'vote' in payload.model_fields_set:
                if payload.vote is None:
                    c.execute('DELETE FROM game_votes WHERE user_id=? AND library_id=? AND game_id=?', key)
                else:
                    c.execute('INSERT OR REPLACE INTO game_votes VALUES (?,?,?,?)', (*key, payload.vote))
            else:
                if payload.favourite:
                    c.execute('INSERT OR IGNORE INTO game_favourites VALUES (?,?,?)', key)
                else:
                    c.execute('DELETE FROM game_favourites WHERE user_id=? AND library_id=? AND game_id=?', key)
            return game_social(c, owner[0], library_id, game_id)

    @app.post('/v1/auth/register', status_code=201, dependencies=[Depends(json_request)])
    def register(payload: Credentials, request: Request):
        auth_limit(request, payload.username, 'register')
        with hash_slots:
            password_hash = hasher.hash(payload.password.get_secret_value())
        now = int(time.time())
        try:
            with db.connect() as c:
                user_id = c.execute('INSERT INTO users(username,username_key,password_hash,created_at) VALUES (?,?,?,?)',
                                    (payload.username, payload.username.lower(), password_hash, now)).lastrowid
                c.execute('INSERT INTO profiles(user_id,display_name,avatar_name,appearance_json,settings_json,updated_at) VALUES (?,?,?,?,?,?)',
                          (user_id, payload.username, payload.username, Appearance().model_dump_json(), Settings().model_dump_json(), now))
        except sqlite3.IntegrityError:
            raise HTTPException(409, 'Username unavailable') from None
        return {'user_id': user_id, 'username': payload.username}

    @app.post('/v1/auth/login', dependencies=[Depends(json_request)])
    def login(payload: Credentials, request: Request):
        auth_limit(request, payload.username, 'login')
        with db.connect() as c:
            user = c.execute('SELECT * FROM users WHERE username_key=?', (payload.username.lower(),)).fetchone()
        try:
            with hash_slots:
                hasher.verify(user['password_hash'] if user else dummy_hash, payload.password.get_secret_value())
        except VerificationError:
            raise HTTPException(401, 'Invalid username or password') from None
        if not user:
            raise HTTPException(401, 'Invalid username or password')
        token = secrets.token_urlsafe(32)
        now = int(time.time())
        expires = now + session_seconds
        with db.connect() as c:
            current = profile(c, user['user_id'])
            if current['default_outfit_id']:
                selected = outfit(c, user['user_id'], current['default_outfit_id'])
                check_appearance(c, user['user_id'], Appearance.model_validate_json(selected['appearance_json']))
                if json.loads(selected['appearance_json']) != current['appearance'] or current['equipped_outfit_id'] != selected['outfit_id']:
                    c.execute('UPDATE profiles SET appearance_json=?,equipped_outfit_id=?,profile_version=profile_version+1,updated_at=? WHERE user_id=?', (selected['appearance_json'], selected['outfit_id'], now, user['user_id']))
            if hasher.check_needs_rehash(user['password_hash']):
                with hash_slots:
                    upgraded = hasher.hash(payload.password.get_secret_value())
                c.execute('UPDATE users SET password_hash=? WHERE user_id=?', (upgraded, user['user_id']))
            c.execute('DELETE FROM sessions WHERE expires_at<=?', (now,))
            c.execute('INSERT INTO sessions VALUES (?,?,?,?)', (hashlib.sha256(token.encode()).hexdigest(), user['user_id'], now, expires))
            c.execute('DELETE FROM sessions WHERE user_id=? AND token_hash NOT IN (SELECT token_hash FROM sessions WHERE user_id=? ORDER BY created_at DESC,rowid DESC LIMIT 10)',
                      (user['user_id'], user['user_id']))
        return {'user_id': user['user_id'], 'username': user['username'], 'access_token': token,
                'token_type': 'Bearer', 'expires_at': expires}

    @app.post('/v1/auth/logout', status_code=204)
    def logout(who=Depends(identity)):
        with db.connect() as c:
            c.execute('DELETE FROM sessions WHERE token_hash=?', (who[1],))

    @app.get('/v1/me')
    def get_profile(who=Depends(identity)):
        with db.connect() as c:
            return profile(c, who[0])

    # One-use join proofs: never give the game server an account bearer token.
    # Bounded in-memory tickets intentionally expire when the account service restarts.
    join_tickets = OrderedDict()
    ticket_lock = threading.Lock()

    @app.post('/v1/lan/ticket')
    def lan_ticket(who=Depends(identity)):
        now = time.monotonic()
        value = secrets.token_urlsafe(32)
        digest = hashlib.sha256(value.encode()).hexdigest()
        with ticket_lock:
            for key in list(join_tickets):
                if join_tickets[key][2] <= now:
                    del join_tickets[key]
            if len(join_tickets) >= 4096:
                raise HTTPException(503, 'Join ticket capacity reached')
            join_tickets[digest] = (who[0], who[1], now + 30)
        return {'ticket': value, 'expires_in': 30}

    @app.post('/v1/lan/redeem')
    def redeem_ticket(request: Request, credentials: HTTPAuthorizationCredentials | None = Depends(bearer)):
        # Dedicated game server runs alongside the account service. Remote clients
        # obtain tickets over the account service's HTTPS endpoint.
        if not request.client or request.client.host not in ('127.0.0.1', '::1'):
            raise HTTPException(403, 'Join proofs can only be redeemed locally')
        if not credentials or credentials.scheme.lower() != 'bearer' or not 40 <= len(credentials.credentials) <= 128:
            raise HTTPException(401, 'Invalid join proof')
        digest = hashlib.sha256(credentials.credentials.encode()).hexdigest()
        with ticket_lock:
            entry = join_tickets.pop(digest, None)
        if not entry or entry[2] <= time.monotonic():
            raise HTTPException(401, 'Expired or already consumed join proof')
        with db.connect() as c:
            if not c.execute('SELECT 1 FROM sessions WHERE token_hash=? AND user_id=? AND expires_at>?',
                             (entry[1], entry[0], int(time.time()))).fetchone():
                raise HTTPException(401, 'Account session revoked')
            return profile(c, entry[0])

    @app.patch('/v1/me', dependencies=[Depends(json_request)])
    def update_profile(payload: ProfilePatch, who=Depends(identity)):
        with db.connect() as c:
            c.execute('BEGIN IMMEDIATE')
            old = profile(c, who[0])
            if payload.profile_version != old['profile_version']:
                raise HTTPException(409, 'Profile changed; retrieve it again before updating')
            if payload.appearance:
                check_appearance(c, who[0], payload.appearance)
                c.execute('UPDATE profiles SET equipped_outfit_id=NULL,default_outfit_id=NULL WHERE user_id=?', (who[0],))
            for key, value in payload.model_dump(exclude_unset=True).items():
                if key != 'profile_version':
                    old[key] = value
            c.execute('UPDATE profiles SET display_name=?,avatar_name=?,appearance_json=?,settings_json=?,profile_version=profile_version+1,updated_at=? WHERE user_id=?',
                      (old['display_name'], old['avatar_name'], json.dumps(old['appearance']), json.dumps(old['settings']), int(time.time()), who[0]))
            return profile(c, who[0])

    # Read-only, public bundled avatar bytes, independent of profile ownership.
    # There is no remote proxy, upload, grant, or access to account database files.
    bundle = Path(asset_root or Path(__file__).parents[2] / 'XboxClientWindows/content/offline-avatar/cache').resolve()

    @app.get('/asset/')
    @app.get('/asset', include_in_schema=False)
    def bundled_avatar_asset(id: str):
        if not id.isascii() or not id.isdecimal() or len(id) > 16 or id.startswith('0') or int(id) > 9007199254740991:
            raise HTTPException(400, 'Invalid bundled asset ID')
        target = (bundle / id).resolve()
        if target.parent != bundle or not target.is_file():
            raise HTTPException(404, f'Bundled avatar asset {id} unavailable')
        if target.stat().st_size > 16 * 1024 * 1024:
            raise HTTPException(413, 'Bundled avatar asset exceeds 16 MiB')
        return FileResponse(target, media_type='application/octet-stream')

    return app
