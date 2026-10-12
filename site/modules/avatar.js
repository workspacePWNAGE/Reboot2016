const express = require('express');
const fs = require('fs');
const path = require('path');
const { colors, validColorIds, ROOT } = require('./config');
const { readUsers, saveUsers } = require('./database');
const { checkBan } = require('./bans');
const { readItems } = require('./items');

const router = express.Router();

const RENDERER_URL = (process.env.AVATAR_RENDERER_URL || 'http://0.0.0.0:4417').replace(/\/+$/, '');
const RENDERER_API_KEY = process.env.AVATAR_RENDERER_API_KEY || '';

const PARTS = {
    head: ['head', 'head'],
    torso: ['torso', 'torso'],
    leftArm: ['leftArm', 'larm'],
    rightArm: ['rightArm', 'rarm'],
    leftLeg: ['leftLeg', 'lleg'],
    rightLeg: ['rightLeg', 'rleg']
};

function findColor(id) {
    return colors.find(color => String(color.id) === String(id));
}

function colorHex(color) {
    if (!color) return '7F7F7F';

    let value = color.hex || color.hexColor || color.color || color.value;

    if (typeof value === 'number' && Number.isFinite(value)) {
        value = value.toString(16).padStart(6, '0');
    }

    if (typeof value === 'string') {
        value = value.trim();

        if (/^#[0-9a-f]{6}$/i.test(value)) return value.slice(1);
        if (/^[0-9a-f]{6}$/i.test(value)) return value;
        if (/^#[0-9a-f]{3}$/i.test(value)) {
            return value.slice(1).split('').map(c => c + c).join('');
        }
    }

    if (Array.isArray(color.rgb) && color.rgb.length >= 3) {
        return color.rgb.slice(0, 3).map(n =>
            Math.max(0, Math.min(255, Number(n) || 0))
                .toString(16).padStart(2, '0')
        ).join('');
    }

    if (typeof color.r === 'number' &&
        typeof color.g === 'number' &&
        typeof color.b === 'number') {
        return [color.r, color.g, color.b].map(n =>
            Math.max(0, Math.min(255, n))
                .toString(16).padStart(2, '0')
        ).join('');
    }

    return '7F7F7F';
}

const MAX_HATS = 3;

function hatList(value) {
    const ids = [].concat(value === undefined || value === null ? [] : value)
        .map(Number)
        .filter(id => Number.isInteger(id) && id > 0);
    return [...new Set(ids)].slice(0, MAX_HATS);
}

function getAvatar(user) {
    const avatar = user.avatar && typeof user.avatar === 'object'
        ? user.avatar
        : {};

    if (!avatar.bodycolors || typeof avatar.bodycolors !== 'object') {
        avatar.bodycolors = {};
    }
    if (!avatar.items) {
        avatar.items = { shirt: null, pants: null, face: null, hat: [] };
    }
    avatar.items.hat = hatList(avatar.items.hat);

    return avatar;
}

function ownedIds(user) {
    return new Set((Array.isArray(user.inventory) ? user.inventory : []).map(Number));
}

function ownedEquipped(avatar, user) {
    const owned = ownedIds(user);
    const result = {};
    for (const [type, id] of Object.entries(avatar.items || {})) {
        if (type === 'hat') {
            result.hat = hatList(id).filter(hatId => owned.has(hatId));
        } else {
            result[type] = id && owned.has(Number(id)) ? id : null;
        }
    }
    return result;
}

function requireLogin(req, res, next) {
    if (!req.user) {
        return res.status(401).json({ error: 'Please sign in to use your character.' });
    }

    next();
}

async function rendererRequest(path, options = {}) {
    const headers = { ...(options.headers || {}) };

    if (RENDERER_API_KEY) {
        headers['X-Api-Key'] = RENDERER_API_KEY;
    }

    const response = await fetch(`${RENDERER_URL}${path}`, {
        ...options,
        headers,
        signal: AbortSignal.timeout(60000)
    });

    if (!response.ok) {
        const text = await response.text();
        throw new Error(text || `Renderer returned HTTP ${response.status}`);
    }

    return response;
}

router.get('/api/character/data', requireLogin, (req, res) => {
    const users = readUsers();
    const user = users.find(u => u.id === req.user.id);

    if (!user) {
        return res.status(404).json({ error: 'User not found.' });
    }

    const avatar = getAvatar(user);

    res.json({
        avatar: { ...avatar, items: ownedEquipped(avatar, user) },
        colors: colors.map(color => ({
            id: color.id,
            name: color.name || color.Name || `Color ${color.id}`,
            hex: colorHex(color)
        }))
    });
});

router.get('/api/character/items', requireLogin, (req, res) => {
    const user = readUsers().find(u => u.id === req.user.id);
    if (!user) return res.status(404).json({ error: 'User not found.' });

    const owned = ownedIds(user);
    const type = req.query.type;
    const filtered = readItems().filter(i => owned.has(Number(i.id)) && (!type || i.type === type));
    res.json({ items: filtered });
});

router.post('/api/character/equip', requireLogin, (req, res) => {
    const { type, itemId } = req.body;
    if (!['shirt', 'pants', 'face', 'hat'].includes(type)) {
        return res.status(400).json({ error: 'Invalid item type.' });
    }

    const users = readUsers();
    const index = users.findIndex(u => u.id === req.user.id);
    if (index === -1) return res.status(404).json({ error: 'User not found.' });

    const ban = checkBan(users, index);
    if (ban) return res.status(ban.status).json(ban.body);

    const avatar = getAvatar(users[index]);

    if (type === 'hat') {
        const hats = hatList(avatar.items.hat);

        if (itemId === null || itemId === undefined || itemId === '') {
            avatar.items.hat = [];
        } else if (req.body.equip === false) {
            avatar.items.hat = hats.filter(id => id !== Number(itemId));
        } else {
            const item = readItems().find(i => i.id === Number(itemId) && i.type === 'hat');
            if (!item) return res.status(404).json({ error: 'Item not found.' });
            if (!ownedIds(users[index]).has(item.id)) {
                return res.status(403).json({ error: "You don't own that item." });
            }
            if (!hats.includes(item.id)) {
                if (hats.length >= MAX_HATS) {
                    return res.status(400).json({ error: `You can only wear ${MAX_HATS} hats at once. Take one off first.` });
                }
                hats.push(item.id);
            }
            avatar.items.hat = hats;
        }

        users[index].avatar = avatar;
        saveUsers(users);
        return res.json({ message: 'Hats updated.', avatar });
    }

    if (itemId === null || itemId === undefined || itemId === '') {
        avatar.items[type] = null;
    } else {
        const items = readItems();
        const item = items.find(i => i.id === Number(itemId) && i.type === type);
        if (!item) return res.status(404).json({ error: 'Item not found.' });
        if (!ownedIds(users[index]).has(item.id)) {
            return res.status(403).json({ error: "You don't own that item." });
        }
        avatar.items[type] = item.id;
    }

    users[index].avatar = avatar;
    saveUsers(users);

    res.json({ message: 'Item equipped successfully.', avatar });
});

router.post('/api/character/colors', requireLogin, (req, res) => {
    const bodycolors = req.body && req.body.bodycolors;

    if (!bodycolors || typeof bodycolors !== 'object' || Array.isArray(bodycolors)) {
        return res.status(400).json({ error: 'Invalid body colors.' });
    }

    const users = readUsers();
    const index = users.findIndex(u => u.id === req.user.id);

    if (index === -1) {
        return res.status(404).json({ error: 'User not found.' });
    }

    const ban = checkBan(users, index);
    if (ban) {
        return res.status(ban.status).json(ban.body);
    }

    const avatar = getAvatar(users[index]);
    const validParts = Object.keys(PARTS);
    const entries = Object.entries(bodycolors);

    if (!entries.length || entries.some(([part, id]) =>
        !validParts.includes(part) ||
        !validColorIds.has(Number(id)) ||
        !Number.isInteger(Number(id))
    )) {
        return res.status(400).json({ error: 'Invalid body part or color ID.' });
    }

    for (const [part, id] of entries) {
        avatar.bodycolors[part] = Number(id);
    }

    users[index].avatar = avatar;
    saveUsers(users);

    res.json({ message: 'Character saved.', avatar });
});

router.get('/api/character/textures', requireLogin, async (req, res) => {
    try {
        const response = await rendererRequest('/textures');
        const data = await response.json();

        res.json({
            files: Array.isArray(data.files) ? data.files : []
        });
    } catch (error) {
        console.error('Renderer texture request failed:', error.message);
        res.status(502).json({ error: 'Could not load renderer textures.' });
    }
});

router.post('/api/character/render', requireLogin, async (req, res) => {
    const users = readUsers();
    const user = users.find(u => u.id === req.user.id);

    if (!user) {
        return res.status(404).json({ error: 'User not found.' });
    }

    const avatar = getAvatar(user);
    const bodycolors = req.body && req.body.bodycolors && typeof req.body.bodycolors === 'object'
        ? req.body.bodycolors
        : avatar.bodycolors;

    const params = new URLSearchParams();

    const size = Number(req.body.size || 512);
    params.set('size', String(Math.max(16, Math.min(1024,
        Number.isFinite(size) ? Math.round(size) : 512
    ))));

    params.set('quality', req.body.quality === 'lq' ? 'lq' : 'hq');

    for (const [part, rendererPart] of Object.values(PARTS)) {
        const colorId = bodycolors[part] !== undefined ? bodycolors[part] : avatar.bodycolors[part];
        const color = findColor(colorId);
        params.set(rendererPart, colorHex(color));
    }

    const items = readItems();
    const equippedItems = ownedEquipped(avatar, user);

    if (equippedItems.face) {
        const faceItem = items.find(i => i.id === Number(equippedItems.face) && i.type === 'face');
        if (faceItem && faceItem.texture) params.set('face', faceItem.texture);
    }
    if (equippedItems.shirt) {
        const shirtItem = items.find(i => i.id === Number(equippedItems.shirt) && i.type === 'shirt');
        if (shirtItem && shirtItem.texture) params.set('shirt', shirtItem.texture);
    }
    if (equippedItems.pants) {
        const pantsItem = items.find(i => i.id === Number(equippedItems.pants) && i.type === 'pants');
        if (pantsItem && pantsItem.texture) params.set('pants', pantsItem.texture);
    }
    for (const hatId of hatList(equippedItems.hat)) {
        const hatItem = items.find(i => i.id === hatId && i.type === 'hat');
        if (hatItem && hatItem.model) params.append('hat', hatItem.model);
    }

    try {
        const response = await rendererRequest('/render', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: params.toString()
        });

        const image = (await response.text()).trim();

        if (!/^[A-Za-z0-9+/]+={0,2}$/.test(image)) {
            throw new Error('Renderer returned invalid image data.');
        }

        const thumbDir = path.resolve(ROOT, './data/thumbnails/players');
        fs.mkdirSync(thumbDir, { recursive: true });
        fs.writeFileSync(path.join(thumbDir, `${user.id}.png`), Buffer.from(image, 'base64'));

        res.json({ image });
    } catch (error) {
        console.error('Avatar render failed:', error.message);
        res.status(502).json({ error: 'Avatar rendering failed. Check the renderer service.' });
    }
});

router.get('/health', async (req, res) => {
    try {
        const response = await rendererRequest('/health');
        res.status(response.status).json(await response.json());
    } catch (_) {
        res.status(502).json({ status: 'unreachable' });
    }
});

module.exports = router;