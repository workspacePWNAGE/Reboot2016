const fs = require('fs');
const { DB_DIR, USERS_FILE } = require('./config');

if (!fs.existsSync(DB_DIR)) fs.mkdirSync(DB_DIR);
if (!fs.existsSync(USERS_FILE)) fs.writeFileSync(USERS_FILE, JSON.stringify([]));

function readUsers() {
    try {
        const data = fs.readFileSync(USERS_FILE, 'utf8');
        if (!data.trim()) return [];
        return JSON.parse(data);
    } catch (err) {
        console.error("Error reading users, returning empty array:", err);
        return [];
    }
}

function saveUsers(users) {
    fs.writeFileSync(USERS_FILE, JSON.stringify(users, null, 4));
}

module.exports = { readUsers, saveUsers };