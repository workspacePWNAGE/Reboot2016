PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);
CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT CHECK(user_id BETWEEN 1 AND 2147483647),
    username TEXT NOT NULL,
    username_key TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    created_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS profiles (
    user_id INTEGER PRIMARY KEY REFERENCES users(user_id) ON DELETE CASCADE,
    schema_version INTEGER NOT NULL DEFAULT 1,
    profile_version INTEGER NOT NULL DEFAULT 1,
    display_name TEXT NOT NULL,
    avatar_name TEXT NOT NULL,
    appearance_json TEXT NOT NULL,
    settings_json TEXT NOT NULL,
    updated_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS sessions (
    token_hash TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS sessions_expiry ON sessions(expires_at);
CREATE TABLE IF NOT EXISTS inventory (
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    asset_id INTEGER NOT NULL CHECK(asset_id > 0),
    kind TEXT NOT NULL CHECK(kind IN ('body_package','shirt','pants','tshirt','accessory')),
    acquired_at INTEGER NOT NULL,
    PRIMARY KEY(user_id, asset_id)
);
CREATE TABLE IF NOT EXISTS game_history (
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    library_id TEXT NOT NULL,
    game_id INTEGER NOT NULL,
    title TEXT NOT NULL,
    last_played INTEGER NOT NULL,
    play_count INTEGER NOT NULL CHECK(play_count > 0),
    PRIMARY KEY(user_id, library_id, game_id)
);
CREATE TABLE IF NOT EXISTS game_launch_receipts (
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    launch_id TEXT NOT NULL,
    recorded_at INTEGER NOT NULL,
    PRIMARY KEY(user_id, launch_id)
);
CREATE TABLE IF NOT EXISTS game_votes (
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    library_id TEXT NOT NULL,
    game_id INTEGER NOT NULL,
    vote INTEGER NOT NULL CHECK(vote IN (-1,1)),
    PRIMARY KEY(user_id,library_id,game_id)
);
CREATE TABLE IF NOT EXISTS game_favourites (
    user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
    library_id TEXT NOT NULL,
    game_id INTEGER NOT NULL,
    PRIMARY KEY(user_id,library_id,game_id)
);
