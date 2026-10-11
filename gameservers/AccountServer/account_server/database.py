import sqlite3
from contextlib import contextmanager
from pathlib import Path


class Database:
    def __init__(self, path):
        self.path = Path(path).resolve()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as connection:
            tables = {row[0] for row in connection.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")}
            if 'schema_version' in tables:
                versions = [row[0] for row in connection.execute('SELECT version FROM schema_version ORDER BY version')]
                if versions not in ([1], [2]) or (versions == [2] and not {'users', 'profiles', 'outfits'} <= tables):
                    raise RuntimeError('Unsupported database schema version; database left unchanged')
            elif tables:
                raise RuntimeError('Unversioned database; choose a new file or perform an explicit migration')
            connection.executescript(Path(__file__).with_name('schema.sql').read_text())
            # Versioned, transactional extension: existing identities/passwords
            # and profile JSON are retained exactly as stored.
            connection.execute('BEGIN IMMEDIATE')
            columns = {r[1] for r in connection.execute('PRAGMA table_info(profiles)')}
            for column in ('equipped_outfit_id', 'default_outfit_id'):
                if column not in columns:
                    connection.execute(f'ALTER TABLE profiles ADD COLUMN {column} TEXT')
            connection.execute('''CREATE TABLE IF NOT EXISTS outfits (
                outfit_id TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
                name TEXT NOT NULL, appearance_json TEXT NOT NULL, created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL)''')
            connection.execute('CREATE INDEX IF NOT EXISTS outfits_owner ON outfits(user_id)')
            connection.execute('DELETE FROM schema_version')
            connection.execute('INSERT INTO schema_version VALUES (2)')

    @contextmanager
    def connect(self):
        connection = sqlite3.connect(self.path, timeout=10)
        connection.row_factory = sqlite3.Row
        connection.execute('PRAGMA foreign_keys=ON')
        try:
            with connection:
                yield connection
        finally:
            connection.close()
