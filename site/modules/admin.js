const express = require('express');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const { ROOT, ITEM_WEBHOOK } = require('./config');
const { requireRole } = require('./session');
const { roleAtLeast } = require('./roles');
const { runCommand } = require('./adminCommands');
const adminLog = require('./adminLog');
const { readItems, saveItems, getNextItemId } = require('./items');
const { readUsers, saveUsers } = require('./database');
const { TEXTURE_DIR, MESH_DIR, SOUND_DIR, PLACE_DIR, HAT_DIR, RENDERER_TEXTURE_DIR } = require('./assetPaths');

const router = express.Router();
const upload = multer({ storage: multer.memoryStorage() });

const RENDERER_URL = (process.env.AVATAR_RENDERER_URL || 'http://0.0.0.0:4417').replace(/\/+$/, '');
const RENDERER_API_KEY = process.env.AVATAR_RENDERER_API_KEY || '';

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

function parseOffsale(value) {
    if (value === false) return false;
    return !['false', '0', 'off', 'no'].includes(String(value).trim().toLowerCase());
}

function truncate(text, max) {
    const s = String(text ?? '');
    return s.length > max ? s.slice(0, max - 1) + '…' : s;
}

async function checkHatWithRenderer(xml) {
    const headers = { 'Content-Type': 'text/xml' };
    if (RENDERER_API_KEY) headers['X-Api-Key'] = RENDERER_API_KEY;

    let response;
    try {
        response = await fetch(`${RENDERER_URL}/hats/check`, {
            method: 'POST',
            headers,
            body: xml,
            signal: AbortSignal.timeout(15000)
        });
    } catch (err) {
        return { ok: false, problems: [`Could not reach the renderer to check the hat (${err.message}).`] };
    }

    const data = await response.json().catch(() => null);
    if (!data) return { ok: false, problems: [`The renderer answered HTTP ${response.status} without JSON.`] };
    if (data.ok) return data;
    return { ok: false, problems: data.problems || [data.error || `Renderer failed with HTTP ${response.status}.`] };
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
        if (type === 'hat') params.set('hat', textureFileName);

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

async function sendItemOnsaleWebhook(item) {
    if (!ITEM_WEBHOOK) {
        console.warn('[webhook] ITEM_WEBHOOK is not set in config.js, skipping onsale announcement.');
        return;
    }

    try {
        const resp = await fetch(ITEM_WEBHOOK, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                username: 'Catalog',
                allowed_mentions: { parse: [] },
                embeds: [{
                    title: 'New Item Onsale!',
                    color: 0x57F287,
                    fields: [
                        { name: 'Name', value: truncate(item.name, 256) || '(unnamed)', inline: false },
                        { name: 'Type', value: truncate(item.type, 64) || 'unknown', inline: true },
                        { name: 'Price', value: `${item.price} Bux`, inline: true },
                        { name: 'ID', value: String(item.id), inline: true }
                    ],
                    timestamp: new Date().toISOString()
                }]
            }),
            signal: AbortSignal.timeout(5000)
        });

        if (resp.ok) {
            console.log(`[webhook] Announced item ${item.id} (${item.name}) as onsale.`);
        } else {
            console.error('[webhook] Item webhook failed:', resp.status, await resp.text());
        }
    } catch (err) {
        console.error('[webhook] Failed to send item webhook:', err.message);
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
    const { type, name, description, price, offsale } = req.body;
    const file = req.file;

    if (!file || !type) {
        return res.status(400).json({ error: "Missing needed fields: 'file' or 'type'" });
    }

    if (type === 'mesh') {
        const meshExt = path.extname(file.originalname).toLowerCase();
        if (meshExt === '.obj') {
            const text = file.buffer.toString('utf8');
            if (!/^\s*v\s/m.test(text) || !/^\s*f\s/m.test(text)) {
                return res.status(400).json({ error: "That doesn't look like an .obj mesh (no 'v' vertex lines or 'f' faces)." });
            }
        } else if (meshExt === '.mesh') {
            if (file.buffer.slice(0, 8).toString('latin1') !== 'version ') {
                return res.status(400).json({ error: "That doesn't look like a Roblox .mesh file (missing 'version' header)." });
            }
        } else {
            return res.status(400).json({ error: "Meshes must be .obj files (or Roblox .mesh)." });
        }
    }

    let hatName = '';
    if (type === 'hat') {
        const check = await checkHatWithRenderer(file.buffer.toString('utf8'));
        if (!check.ok) {
            return res.status(400).json({ error: `Hat rejected:\n - ${check.problems.join('\n - ')}` });
        }
        hatName = check.name || '';
    }

    let primaryDir;
    let idDirs;
    if (type === 'sound') {
        primaryDir = SOUND_DIR;
        idDirs = [SOUND_DIR];
    } else if (type === 'place') {
        primaryDir = PLACE_DIR;
        idDirs = [PLACE_DIR];
    } else if (type === 'hat') {
        primaryDir = HAT_DIR;
        idDirs = [HAT_DIR];
    } else if (type === 'mesh') {
        primaryDir = MESH_DIR;
        idDirs = [MESH_DIR];
    } else if (TEXTURE_TYPES.includes(type)) {
        primaryDir = TEXTURE_DIR;
        idDirs = [TEXTURE_DIR, RENDERER_TEXTURE_DIR];
    } else {
        return res.status(400).json({ error: "Invalid type." });
    }

    const newId = getMaxId(idDirs) + 1;
    const ext = type === 'hat' ? '.rbxm' : path.extname(file.originalname);
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

    if (['shirt', 'pants', 'face', 'hat'].includes(type)) {
        const items = readItems();
        const itemId = getNextItemId(items);
        const itemName = name && typeof name === 'string' && name.trim() ? name.trim() : (hatName || `New ${type} ${newId}`);
        const itemDesc = description && typeof description === 'string' && description.trim() ? description.trim() : `Uploaded ${type} item.`;
        const itemPrice = price !== undefined && !isNaN(parseInt(price, 10)) ? Math.max(0, parseInt(price, 10)) : 0;

        const isOffsale = offsale === undefined ? true : parseOffsale(offsale);

        const newItem = {
            id: itemId,
            type: type,
            name: itemName,
            description: itemDesc,
            texture: type === 'hat' ? '' : fileName,
            ...(type === 'hat' ? { model: fileName } : {}),
            renderId: 0,
            price: itemPrice,
            offsale: isOffsale
        };

        items.push(newItem);
        saveItems(items);

        await renderAndSaveItemThumbnail(itemId, type, fileName);
        catalogMsg = ` (Catalog Item ID: ${itemId})`;

        if (!isOffsale) await sendItemOnsaleWebhook(newItem);
    }

    adminLog.record(req.user, `upload ${type}`, `Success: uploaded ${type} as asset ${newId}${catalogMsg}`);

    let hint = '';
    if (type === 'mesh') hint = ` Use /api/get-mesh?id=${newId} as the MeshId in a hat model.`;
    else if (type === 'texture') hint = ` Use /api/get-texture?id=${newId} as the TextureId in a hat model.`;

    res.json({ message: `Successfully uploaded ${type}! Assigned Asset ID: ${newId}${catalogMsg}${hint}` });
});

router.post('/api/admin/items/edit', requireRole('administrator'), async (req, res) => {
    const { id, name, description, price, offsale, renderId } = req.body;
    if (!id) return res.status(400).json({ error: "Missing item ID." });

    const items = readItems();
    const index = items.findIndex(i => Number(i.id) === Number(id));
    if (index === -1) return res.status(404).json({ error: "Item not found." });

    const item = items[index];
    const wasOffsale = Boolean(item.offsale);

    if (name !== undefined) item.name = String(name).trim();
    if (description !== undefined) item.description = String(description).trim();
    if (price !== undefined) item.price = Math.max(0, parseInt(price, 10) || 0);
    if (offsale !== undefined) item.offsale = parseOffsale(offsale);
    if (renderId !== undefined) item.renderId = parseInt(renderId, 10) || 0;

    saveItems(items);
    
    if (wasOffsale && !item.offsale) {
        await sendItemOnsaleWebhook(item);
    }

    res.json({ message: "Item updated successfully.", item });
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
    if (item.model) {
        const modelName = path.basename(String(item.model));
        const modelPath = path.join(HAT_DIR, modelName);
        if (fs.existsSync(modelPath)) {
            try { fs.unlinkSync(modelPath); } catch (_) {}
        }
    }
    const thumbPath = path.resolve(ROOT, `./data/thumbnails/items/${item.id}.png`);
    if (fs.existsSync(thumbPath)) {
        try { fs.unlinkSync(thumbPath); } catch (_) {}
    }

    items.splice(index, 1);
    saveItems(items);
    res.json({ message: "Item deleted successfully along with its files and thumbnail." });
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