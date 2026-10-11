const fs = require('fs');
const path = require('path');
const { DB_DIR } = require('./config');

const ITEMS_FILE = path.join(DB_DIR, 'items.json');

if (!fs.existsSync(DB_DIR)) fs.mkdirSync(DB_DIR, { recursive: true });
if (!fs.existsSync(ITEMS_FILE)) fs.writeFileSync(ITEMS_FILE, JSON.stringify([], null, 4));

function readItems() {
    try {
        const data = fs.readFileSync(ITEMS_FILE, 'utf8');
        if (!data.trim()) return [];
        return JSON.parse(data);
    } catch (err) {
        return [];
    }
}

function saveItems(items) {
    fs.writeFileSync(ITEMS_FILE, JSON.stringify(items, null, 4));
}

function getNextItemId(items) {
    let maxId = 0;
    for (const item of items) {
        if (item.id > maxId) maxId = item.id;
    }
    return maxId + 1;
}

module.exports = { readItems, saveItems, getNextItemId };