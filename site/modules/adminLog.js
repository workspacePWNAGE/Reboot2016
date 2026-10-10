const fs = require('fs');
const path = require('path');
const { DB_DIR } = require('./config');

// One JSON object per line: database/admin-log.jsonl
const LOG_FILE = path.join(DB_DIR, 'admin-log.jsonl');

/** Records a staff action. Never throws (a logging problem must not break the command). */
function record(actor, command, output) {
    try {
        fs.mkdirSync(DB_DIR, { recursive: true });
        const entry = {
            time: new Date().toISOString(),
            actorId: actor.id,
            actor: actor.username,
            role: actor.role,
            command: String(command).slice(0, 500),
            result: String(output).split('\n')[0].slice(0, 300)
        };
        fs.appendFileSync(LOG_FILE, JSON.stringify(entry) + '\n');
    } catch (err) {
        console.error('Failed to write admin log:', err);
    }
}

/** The last `count` entries, oldest first. */
function recent(count) {
    try {
        const lines = fs.readFileSync(LOG_FILE, 'utf8').split('\n').filter(Boolean);
        return lines.slice(-count).map(line => {
            try { return JSON.parse(line); } catch (_) { return null; }
        }).filter(Boolean);
    } catch (_) {
        return [];
    }
}

module.exports = { record, recent };
