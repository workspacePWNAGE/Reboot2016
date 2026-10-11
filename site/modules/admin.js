const express = require('express');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const { ROOT } = require('./config');
const { requireRole } = require('./session');
const { roleAtLeast } = require('./roles');
const { runCommand } = require('./adminCommands');
const adminLog = require('./adminLog');
const { readItems, saveItems, getNextItemId } = require('./items');
const { readUsers, saveUsers } = require('./database');

const router = express.Router();
const upload = multer({ storage: multer.memoryStorage() });

const RENDERER_URL = (process.env.AVATAR_RENDERER_URL || 'http://0.0.0.0:4417').replace(/\/+$/, '');
const RENDERER_API_KEY = process.env.AVATAR_RENDERER_API_KEY || '';

const TEXTURE_DIR = path.resolve(ROOT, 'data', 'assets', 'textures');
const SOUND_DIR = path.resolve(ROOT, 'data', 'assets', 'sounds');
const PLACE_DIR = path.resolve(ROOT, 'data', 'places');
const RENDERER_TEXTURE_DIR = process.env.RENDERER_TEXTURE_DIR
    ? path.resolve(process.env.RENDERER_TEXTURE_DIR)
    : path.resolve(ROOT, 'content', 'textures');

const TEXTURE_TYPES = ['texture', 'shirt', 'pants', 'face'];

function ensureDir(dirPath) {
    if (!fs.existsSync(dirPath)) fs.mkdirSync(dirPath, { recursive: true });
}

function getMaxId(dirs) {
    let maxId = 0;
    for (const dirPath of dirs) {
        if (!fs.existsSync(dirPath)) continue;
        for (const f of fs.readdirSync(dirPath)) {
            const num = parseInt(path.parse(f).name, 10);
            if (!isNaN(num) && num > maxId) maxId = num;
        }
    }
    return maxId;
}

async function renderAndSaveItemThumbnail(itemId, type, textureFileName) {
    try {
        const params = new URLSearchParams();
        params.set('size', '256');
        params.set('quality', 'hq');
        ['head', 'torso', 'larm', 'rarm', 'lleg', 'rleg'].forEach(p => params.set(p, '7F7F7F'));
        if (type === 'shirt') params.set('shirt', textureFileName);
        if (type === 'pants') params.set('pants', textureFileName);
        if (type === 'face') params.set('face', textureFileName);

        const headers = {};
        if (RENDERER_API_KEY) headers['X-Api-Key'] = RENDERER_API_KEY;

        const response = await fetch(`${RENDERER_URL}/render`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded', ...headers },
            body: params.toString(),
            signal: AbortSignal.timeout(30000)
        });

        if (response.ok) {
            const imageBase64 = (await response.text()).trim();
            const thumbDir = path.resolve(ROOT, './data/thumbnails/items');
            ensureDir(thumbDir);
            fs.writeFileSync(path.join(thumbDir, `${itemId}.png`), Buffer.from(imageBase64, 'base64'));
        }
    } catch (err) {
        console.error('Failed to render item thumbnail:', err.message);
    }
}

router.get('/admin/terminal', (req, res) => {
    if (!req.user || !roleAtLeast(req.user, 'moderator')) return res.sendStatus(404);
    res.sendFile(path.join(ROOT, 'admin.html'));
});

router.get('/admin/terminal.js', (req, res) => {
    if (!req.user || !roleAtLeast(req.user, 'moderator')) return res.sendStatus(404);
    res.type('application/javascript').sendFile(path.join(__dirname, 'admin-terminal.js'));
});

router.get('/renderTest', (req, res) => {
    if (!req.user || !roleAtLeast(req.user, 'administrator')) return res.sendStatus(404);
    res.sendFile(path.join(ROOT, 'renderTest.html'));
});

router.post('/api/admin/terminal', requireRole('moderator'), async (req, res) => {
    if (!req.is('application/json')) {
        return res.status(415).json({ error: "Content-Type must be application/json." });
    }

    const { command } = req.body;

    if (typeof command !== 'string' || !command.trim()) {
        return res.status(400).json({ error: "No command given." });
    }
    if (command.length > 500) {
        return res.status(400).json({ error: "Command is too long." });
    }

    try {
        const output = await runCommand(req.user, command);
        res.json({ output });
    } catch (err) {
        res.status(500).json({ error: err.message || "Command execution failed." });
    }
});

router.post('/api/admin/upload', requireRole('administrator'), upload.single('file'), async (req, res) => {
    const { type, name, description, price } = req.body;
    const file = req.file;

    if (!file || !type) {
        return res.status(400).json({ error: "Missing needed fields: 'file' or 'type'" });
    }

    let primaryDir;
    let idDirs;
    if (type === 'sound') {
        primaryDir = SOUND_DIR;
        idDirs = [SOUND_DIR];
    } else if (type === 'place') {
        primaryDir = PLACE_DIR;
        idDirs = [PLACE_DIR];
    } else if (TEXTURE_TYPES.includes(type)) {
        primaryDir = TEXTURE_DIR;
        idDirs = [TEXTURE_DIR, RENDERER_TEXTURE_DIR];
    } else {
        return res.status(400).json({ error: "Invalid type." });
    }

    const newId = getMaxId(idDirs) + 1;
    const ext = path.extname(file.originalname);
    const fileName = `${newId}${ext}`;

    try {
        ensureDir(primaryDir);
        fs.writeFileSync(path.join(primaryDir, fileName), file.buffer);
    } catch (e) {
        console.error(`Failed to write file to ${primaryDir}:`, e.message);
        return res.status(500).json({ error: "Failed to save the uploaded file." });
    }

    if (TEXTURE_TYPES.includes(type)) {
        if (fs.existsSync(RENDERER_TEXTURE_DIR)) {
            try {
                fs.writeFileSync(path.join(RENDERER_TEXTURE_DIR, fileName), file.buffer);
            } catch (e) {
                console.error(`Failed to copy texture to renderer dir ${RENDERER_TEXTURE_DIR}:`, e.message);
            }
        } else {
            console.warn(`Renderer texture dir not found: ${RENDERER_TEXTURE_DIR} (set RENDERER_TEXTURE_DIR). Thumbnail may fail.`);
        }
    }

    let catalogMsg = '';
    if (['shirt', 'pants', 'face'].includes(type)) {
        const items = readItems();
        const itemId = getNextItemId(items);
        const itemName = name && typeof name === 'string' && name.trim() ? name.trim() : `New ${type} ${newId}`;
        const itemDesc = description && typeof description === 'string' && description.trim() ? description.trim() : `Uploaded ${type} item.`;
        const itemPrice = price !== undefined && !isNaN(parseInt(price, 10)) ? parseInt(price, 10) : 0;

        items.push({
            id: itemId,
            type: type,
            name: itemName,
            description: itemDesc,
            texture: fileName,
            renderId: 0,
            price: itemPrice,
            offsale: false
        });
        saveItems(items);

        await renderAndSaveItemThumbnail(itemId, type, fileName);
        catalogMsg = ` (Catalog Item ID: ${itemId})`;
    }

    adminLog.record(req.user, `upload ${type}`, `Success: uploaded ${type} as asset ${newId}${catalogMsg}`);

    res.json({ message: `Successfully uploaded ${type}! Assigned Asset ID: ${newId}${catalogMsg}` });
});

router.post('/api/admin/items/edit', requireRole('administrator'), (req, res) => {
    const { id, name, description, price, offsale, renderId } = req.body;
    if (!id) return res.status(400).json({ error: "Missing item ID." });

    const items = readItems();
    const index = items.findIndex(i => Number(i.id) === Number(id));
    if (index === -1) return res.status(404).json({ error: "Item not found." });

    if (name !== undefined) items[index].name = String(name).trim();
    if (description !== undefined) items[index].description = String(description).trim();
    if (price !== undefined) items[index].price = Math.max(0, parseInt(price, 10) || 0);
    if (offsale !== undefined) items[index].offsale = Boolean(offsale);
    if (renderId !== undefined) items[index].renderId = parseInt(renderId, 10) || 0;

    saveItems(items);
    res.json({ message: "Item updated successfully.", item: items[index] });
});

router.post('/api/admin/items/delete', requireRole('administrator'), (req, res) => {
    const { id } = req.body;
    if (!id) return res.status(400).json({ error: "Missing item ID." });

    const items = readItems();
    const index = items.findIndex(i => Number(i.id) === Number(id));
    if (index === -1) return res.status(404).json({ error: "Item not found." });

    const item = items[index];
    if (item.texture) {
        const textureName = path.basename(String(item.texture));
        for (const dirPath of [TEXTURE_DIR, RENDERER_TEXTURE_DIR]) {
            const texPath = path.join(dirPath, textureName);
            if (fs.existsSync(texPath)) {
                try { fs.unlinkSync(texPath); } catch (_) {}
            }
        }
    }
    const thumbPath = path.resolve(ROOT, `./data/thumbnails/items/${item.id}.png`);
    if (fs.existsSync(thumbPath)) {
        try { fs.unlinkSync(thumbPath); } catch (_) {}
    }

    items.splice(index, 1);
    saveItems(items);
    res.json({ message: "Item deleted successfully along with its texture and thumbnail." });
});

router.post('/api/admin/inventory/give', requireRole('administrator'), (req, res) => {
    const { userId, itemId } = req.body;
    if (!userId || !itemId) return res.status(400).json({ error: "Missing userId or itemId." });

    const users = readUsers();
    const userIndex = users.findIndex(u => Number(u.id) === Number(userId));
    if (userIndex === -1) return res.status(404).json({ error: "User not found." });

    const items = readItems();
    const item = items.find(i => Number(i.id) === Number(itemId));
    if (!item) return res.status(404).json({ error: "Item not found." });

    if (!users[userIndex].inventory) users[userIndex].inventory = [];
    if (!users[userIndex].inventory.includes(Number(itemId))) {
        users[userIndex].inventory.push(Number(itemId));
        saveUsers(users);
    }

    res.json({ message: `Successfully gave item ${itemId} to user ${userId}.` });
});

router.post('/api/admin/inventory/take', requireRole('administrator'), (req, res) => {
    const { userId, itemId } = req.body;
    if (!userId || !itemId) return res.status(400).json({ error: "Missing userId or itemId." });

    const users = readUsers();
    const userIndex = users.findIndex(u => Number(u.id) === Number(userId));
    if (userIndex === -1) return res.status(404).json({ error: "User not found." });

    if (!users[userIndex].inventory) users[userIndex].inventory = [];
    const invIndex = users[userIndex].inventory.indexOf(Number(itemId));
    if (invIndex !== -1) {
        users[userIndex].inventory.splice(invIndex, 1);
        saveUsers(users);
    }

    res.json({ message: `Successfully removed item ${itemId} from user ${userId}.` });
});

module.exports = router;
