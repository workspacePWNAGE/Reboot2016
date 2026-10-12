const { readUsers, saveUsers } = require('./database');
const { isBanned, banUser, unbanUser } = require('./bans');
const { resetUsername } = require('./usernames');
const { ROLES, getRole, rankOf, roleLabel, withArticle, parseRole, canModerate } = require('./roles');
const adminLog = require('./adminLog');
const { readItems, saveItems } = require('./items');
const fs = require('fs');
const path = require('path');
const { ROOT, colors } = require('./config');
const { TEXTURE_DIR, RENDERER_TEXTURE_DIR, HAT_DIR } = require('./assetPaths');

const MAX_BUX_GRANT = 1000000;
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
    return '7F7F7F';
}

async function callRenderer(params) {
    const headers = {};
    if (RENDERER_API_KEY) headers['X-Api-Key'] = RENDERER_API_KEY;
    const response = await fetch(`${RENDERER_URL}/render`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded', ...headers },
        body: params.toString(),
        signal: AbortSignal.timeout(30000)
    });
    if (!response.ok) {
        const text = await response.text();
        throw new Error(text || `Renderer failed with HTTP ${response.status}`);
    }
    return (await response.text()).trim();
}

async function rerenderUserThumbnail(user) {
    const avatar = user.avatar || {};
    const bodycolors = avatar.bodycolors || {};
    const params = new URLSearchParams();
    params.set('size', '512');
    params.set('quality', 'hq');

    for (const [part, rendererPart] of Object.values(PARTS)) {
        const colorId = bodycolors[part];
        const color = findColor(colorId);
        params.set(rendererPart, colorHex(color));
    }

    const items = readItems();
    const equipped = avatar.items || {};
    if (equipped.face) {
        const faceItem = items.find(i => i.id === Number(equipped.face) && i.type === 'face');
        if (faceItem && faceItem.texture) params.set('face', faceItem.texture);
    }
    if (equipped.shirt) {
        const shirtItem = items.find(i => i.id === Number(equipped.shirt) && i.type === 'shirt');
        if (shirtItem && shirtItem.texture) params.set('shirt', shirtItem.texture);
    }
    if (equipped.pants) {
        const pantsItem = items.find(i => i.id === Number(equipped.pants) && i.type === 'pants');
        if (pantsItem && pantsItem.texture) params.set('pants', pantsItem.texture);
    }
    // hats: avatar.items.hat can be one item id or a list of them
    for (const hatId of [].concat(equipped.hat || [])) {
        const hatItem = items.find(i => i.id === Number(hatId) && i.type === 'hat');
        if (hatItem && hatItem.model) params.append('hat', hatItem.model);
    }

    const image = await callRenderer(params);
    const thumbDir = path.resolve(ROOT, './data/thumbnails/players');
    if (!fs.existsSync(thumbDir)) fs.mkdirSync(thumbDir, { recursive: true });
    fs.writeFileSync(path.join(thumbDir, `${user.id}.png`), Buffer.from(image, 'base64'));
}

async function rerenderItemThumbnail(item) {
    const params = new URLSearchParams();
    params.set('size', '256');
    params.set('quality', 'hq');
    ['head', 'torso', 'larm', 'rarm', 'lleg', 'rleg'].forEach(p => params.set(p, '7F7F7F'));
    if (item.type === 'shirt') params.set('shirt', item.texture);
    if (item.type === 'pants') params.set('pants', item.texture);
    if (item.type === 'face') params.set('face', item.texture);
    if (item.type === 'hat') params.set('hat', item.model);

    const image = await callRenderer(params);
    const thumbDir = path.resolve(ROOT, './data/thumbnails/items');
    if (!fs.existsSync(thumbDir)) fs.mkdirSync(thumbDir, { recursive: true });
    fs.writeFileSync(path.join(thumbDir, `${item.id}.png`), Buffer.from(image, 'base64'));
}

function findIndexById(users, raw) {
    if (!/^\d+$/.test(raw || '')) return -1;
    const id = parseInt(raw, 10);
    return users.findIndex(u => u.id === id);
}

function banText(user) {
    if (!isBanned(user)) return 'no';
    const until = user.banExpiration === null || user.banExpiration === undefined
        ? 'permanently'
        : `until ${new Date(user.banExpiration).toUTCString()}`;
    return `yes, ${until} (${user.banReason})`;
}

const COMMAND_LIST = [
    {
        name: 'help', role: 'moderator',
        usage: 'help', desc: 'Prints the commands you can use.',
        run: async ({ actor }) => {
            const mine = COMMAND_LIST.filter(c => rankOf(actor) >= ROLES[c.role]);
            return `Available Commands (${roleLabel(getRole(actor))}):\n` +
                mine.map(c => `- ${c.usage} : ${c.desc}`).join('\n');
        }
    },
    {
        name: 'whoami', role: 'moderator',
        usage: 'whoami', desc: 'Shows who you are logged in as.',
        run: async ({ actor }) => `Logged in as ${actor.username} (ID: ${actor.id}) - ${roleLabel(getRole(actor))}`
    },
    {
        name: 'get-userid', role: 'moderator',
        usage: 'get-userid <username>', desc: 'Returns the ID of a given username.',
        run: async ({ args, users }) => {
            const name = args[0];
            if (!name) return 'Error: Missing username. Usage: get-userid <username>';

            const found = users.find(u => u.username.toLowerCase() === name.toLowerCase());
            return found ? `User ${found.username} has ID: ${found.id}` : `Error: User '${name}' not found.`;
        }
    },
    {
        name: 'userinfo', role: 'moderator',
        usage: 'userinfo <userid>', desc: "Shows a user's name, role, Bux and ban status.",
        run: async ({ args, users }) => {
            const index = findIndexById(users, args[0]);
            if (!args[0]) return 'Error: Invalid syntax. Usage: userinfo <userid>';
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const u = users[index];
            return `User: ${u.username} (ID: ${u.id})\nRole: ${roleLabel(getRole(u))}\nBux: ${u.bux ?? 0}\nBanned: ${banText(u)}`;
        }
    },
    {
        name: 'ban', role: 'moderator', audit: true,
        usage: 'ban <userid> <time hours/inf> <reason>', desc: 'Bans a user (and resets their username).',
        run: async ({ actor, args, users }) => {
            const timeParam = args[1];
            const reason = args.slice(2).join(' ');

            if (!/^\d+$/.test(args[0] || '') || !timeParam || !reason) {
                return 'Error: Invalid syntax. Usage: ban <userid> <time hours/inf> <reason>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't ban ${users[index].username}.`;

            let hours = null;
            if (timeParam.toLowerCase() !== 'inf') {
                hours = parseFloat(timeParam);
                if (!Number.isFinite(hours) || hours <= 0) return "Error: Time must be a positive number of hours or 'inf'.";
            }

            banUser(users, index, hours, reason);
            const bannedName = users[index].username;
            const reset = resetUsername(users, index);
            saveUsers(users);

            const durationText = hours === null ? 'permanently' : `${timeParam} hours`;
            let output = `Success: User ${bannedName} (ID: ${users[index].id}) has been banned ${durationText} for: ${reason}`;
            if (reset.changed) output += `\nTheir username was reset to ${reset.newName}.`;
            else if (reset.conflict) output += `\nWarning: couldn't reset their username, another account already uses ${reset.newName}.`;
            return output;
        }
    },
    {
        name: 'unban', role: 'moderator', audit: true,
        usage: 'unban <userid>', desc: "Lifts a user's ban.",
        run: async ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '')) return 'Error: Invalid syntax. Usage: unban <userid>';

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't unban ${users[index].username}.`;

            const wasBanned = unbanUser(users, index);
            saveUsers(users);

            const u = users[index];
            return wasBanned
                ? `Success: User ${u.username} (ID: ${u.id}) has been unbanned.`
                : `User ${u.username} (ID: ${u.id}) is not banned.`;
        }
    },
    {
        name: 'reset-username', role: 'moderator', audit: true,
        usage: 'reset-username <userid>', desc: "Resets a user's username to player_<userid>.",
        run: async ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '')) return 'Error: Invalid syntax. Usage: reset-username <userid>';

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't reset the username of ${users[index].username}.`;

            const reset = resetUsername(users, index);

            if (reset.conflict) return `Error: Another account already uses the username ${reset.newName}.`;
            if (!reset.changed) return `User ID ${users[index].id} already has the username ${reset.newName}.`;

            saveUsers(users);
            return `Success: Username of ID ${users[index].id} reset from ${reset.oldName} to ${reset.newName}.`;
        }
    },
    {
        name: 'give-bux', role: 'administrator', audit: true,
        usage: 'give-bux <userid> <amount>', desc: 'Gives a user Bux.',
        run: async ({ args, users }) => {
            if (!/^\d+$/.test(args[0] || '') || !/^\d+$/.test(args[1] || '')) {
                return 'Error: Invalid syntax. Usage: give-bux <userid> <amount>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const amount = parseInt(args[1], 10);
            if (amount < 1 || amount > MAX_BUX_GRANT) return `Error: Amount must be between 1 and ${MAX_BUX_GRANT}.`;

            const u = users[index];
            u.bux = (u.bux ?? 0) + amount;
            saveUsers(users);

            return `Success: Gave ${amount} Bux to ${u.username} (ID: ${u.id}). New balance: ${u.bux}.`;
        }
    },
    {
        name: 'set-role', role: 'administrator', audit: true,
        usage: 'set-role <userid> <user/moderator/administrator>', desc: "Sets a user's role.",
        run: async ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '') || !args[1]) {
                return 'Error: Invalid syntax. Usage: set-role <userid> <user/moderator/administrator>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const role = parseRole(args[1]);
            if (!role) return `Error: Unknown role '${args[1]}'. Roles: user, moderator, administrator.`;

            const u = users[index];
            if (u.id === actor.id) return "Error: You can't change your own role.";
            if (getRole(u) === role) return `${u.username} (ID: ${u.id}) is already ${withArticle(role)}.`;

            const before = getRole(u);
            u.role = role;
            saveUsers(users);

            return `Success: ${u.username} (ID: ${u.id}) changed from ${roleLabel(before)} to ${roleLabel(role)}.`;
        }
    },
    {
        name: 'item-edit', role: 'administrator', audit: true,
        usage: 'item-edit <itemid> <name|description|price|offsale|renderid> <value>', desc: 'Edits an item property.',
        run: async ({ args }) => {
            const itemId = args[0];
            const field = args[1] ? args[1].toLowerCase() : '';
            const value = args.slice(2).join(' ');

            if (!/^\d+$/.test(itemId || '') || !['name', 'description', 'price', 'offsale', 'renderid'].includes(field) || value === '') {
                return 'Error: Invalid syntax. Usage: item-edit <itemid> <name|description|price|offsale|renderid> <value>';
            }

            const items = readItems();
            const index = items.findIndex(i => Number(i.id) === Number(itemId));
            if (index === -1) return `Error: Item ID ${itemId} not found.`;

            if (field === 'name') items[index].name = value;
            else if (field === 'description') items[index].description = value;
            else if (field === 'price') {
                const p = parseInt(value, 10);
                if (isNaN(p) || p < 0) return 'Error: Price must be a non-negative integer.';
                items[index].price = p;
            } else if (field === 'offsale') {
                const low = value.toLowerCase();
                if (low !== 'true' && low !== 'false') return 'Error: Offsale value must be true or false.';
                items[index].offsale = low === 'true';
            } else if (field === 'renderid') {
                const r = parseInt(value, 10);
                if (isNaN(r)) return 'Error: RenderId must be an integer.';
                items[index].renderId = r;
            }

            saveItems(items);
            return `Success: Updated item ${itemId} (${field}).`;
        }
    },
    {
        name: 'item-delete', role: 'administrator', audit: true,
        usage: 'item-delete <itemid>', desc: 'Deletes an item and its associated files.',
        run: async ({ args }) => {
            const itemId = args[0];
            if (!/^\d+$/.test(itemId || '')) return 'Error: Invalid syntax. Usage: item-delete <itemid>';

            let items = readItems();
            const index = items.findIndex(i => Number(i.id) === Number(itemId));
            if (index === -1) return `Error: Item ID ${itemId} not found.`;

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
            return `Success: Deleted item ID ${itemId} along with its files and thumbnail.`;
        }
    },
    {
        name: 'give-item', role: 'administrator', audit: true,
        usage: 'give-item <userid> <itemid>', desc: 'Grants an item to a user inventory.',
        run: async ({ args, users }) => {
            const userId = args[0];
            const itemId = args[1];
            if (!/^\d+$/.test(userId || '') || !/^\d+$/.test(itemId || '')) {
                return 'Error: Invalid syntax. Usage: give-item <userid> <itemid>';
            }

            const uIndex = findIndexById(users, userId);
            if (uIndex === -1) return `Error: User ID ${userId} not found.`;

            const items = readItems();
            const item = items.find(i => Number(i.id) === Number(itemId));
            if (!item) return `Error: Item ID ${itemId} not found.`;

            if (!users[uIndex].inventory) users[uIndex].inventory = [];
            if (users[uIndex].inventory.includes(Number(itemId))) {
                return `User ${users[uIndex].username} already owns item ID ${itemId}.`;
            }

            users[uIndex].inventory.push(Number(itemId));
            saveUsers(users);
            return `Success: Gave item ID ${itemId} to ${users[uIndex].username}.`;
        }
    },
    {
        name: 'take-item', role: 'administrator', audit: true,
        usage: 'take-item <userid> <itemid>', desc: 'Removes an item from a user inventory.',
        run: async ({ args, users }) => {
            const userId = args[0];
            const itemId = args[1];
            if (!/^\d+$/.test(userId || '') || !/^\d+$/.test(itemId || '')) {
                return 'Error: Invalid syntax. Usage: take-item <userid> <itemid>';
            }

            const uIndex = findIndexById(users, userId);
            if (uIndex === -1) return `Error: User ID ${userId} not found.`;

            if (!users[uIndex].inventory) users[uIndex].inventory = [];
            const invIndex = users[uIndex].inventory.indexOf(Number(itemId));
            if (invIndex === -1) {
                return `User ${users[uIndex].username} does not own item ID ${itemId}.`;
            }

            users[uIndex].inventory.splice(invIndex, 1);
            saveUsers(users);
            return `Success: Removed item ID ${itemId} from ${users[uIndex].username}.`;
        }
    },
    {
        name: 'rerender-user', role: 'administrator', audit: true,
        usage: 'rerender-user <userid>', desc: "Force re-renders a user's avatar thumbnail.",
        run: async ({ args, users }) => {
            const userId = args[0];
            if (!/^\d+$/.test(userId || '')) return 'Error: Invalid syntax. Usage: rerender-user <userid>';
            const index = findIndexById(users, userId);
            if (index === -1) return `Error: User ID ${userId} not found.`;

            try {
                await rerenderUserThumbnail(users[index]);
                return `Success: Re-rendered thumbnail for user ${users[index].username} (ID: ${userId}).`;
            } catch (err) {
                return `Error: Failed to re-render user thumbnail: ${err.message}`;
            }
        }
    },
    {
        name: 'rerender-item', role: 'administrator', audit: true,
        usage: 'rerender-item <itemid>', desc: 'Force re-renders a catalog item thumbnail.',
        run: async ({ args }) => {
            const itemId = args[0];
            if (!/^\d+$/.test(itemId || '')) return 'Error: Invalid syntax. Usage: rerender-item <itemid>';
            const items = readItems();
            const item = items.find(i => Number(i.id) === Number(itemId));
            if (!item) return `Error: Item ID ${itemId} not found.`;

            try {
                await rerenderItemThumbnail(item);
                return `Success: Re-rendered thumbnail for item ID ${itemId} (${item.name}).`;
            } catch (err) {
                return `Error: Failed to re-render item thumbnail: ${err.message}`;
            }
        }
    },
    {
        name: 'rerender-all', role: 'administrator', audit: true,
        usage: 'rerender-all', desc: 'Force re-renders all user avatars and catalog items.',
        run: async ({ users }) => {
            let userCount = 0;
            let itemCount = 0;
            try {
                for (const u of users) {
                    await rerenderUserThumbnail(u);
                    userCount++;
                }
                const items = readItems();
                for (const i of items) {
                    if (['shirt', 'pants', 'face', 'hat'].includes(i.type)) {
                        await rerenderItemThumbnail(i);
                        itemCount++;
                    }
                }
                return `Success: Re-rendered ${userCount} user thumbnails and ${itemCount} item thumbnails.`;
            } catch (err) {
                return `Error during bulk re-render: ${err.message}`;
            }
        }
    },
    {
        name: 'audit', role: 'administrator',
        usage: 'audit [count]', desc: 'Shows the latest staff actions (default 10, max 50).',
        run: async ({ args }) => {
            const count = Math.min(Math.max(parseInt(args[0], 10) || 10, 1), 50);
            const entries = adminLog.recent(count);
            if (!entries.length) return 'No staff actions recorded yet.';
            return entries.map(e => `${e.time}  ${e.actor} (${e.actorId}): ${e.command}  ->  ${e.result}`).join('\n');
        }
    },
    {
        name: 'upload', role: 'administrator',
        usage: 'upload', desc: 'Instructs how to use the upload endpoint.',
        run: async () => "Upload sys ready! To upload an asset, make a multipart/form-data POST request to /api/admin/upload with 'type' and 'file'."
    }
];

const COMMANDS = new Map(COMMAND_LIST.map(c => [c.name, c]));

async function runCommand(actor, line) {
    const args = line.trim().split(/\s+/);
    const name = args.shift().toLowerCase();

    const cmd = COMMANDS.get(name);
    if (!cmd) return `Unknown command: '${name}'. Type 'help' for a list of commands.`;
    if (rankOf(actor) < ROLES[cmd.role]) return `You don't have permission to use '${name}'.`;

    const output = await cmd.run({ actor, args, users: readUsers() });
    if (cmd.audit) adminLog.record(actor, line, output);
    return output;
}

module.exports = { runCommand, COMMAND_LIST };