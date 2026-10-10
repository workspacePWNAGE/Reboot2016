const express = require('express');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const { ROOT, ADMIN_KEYS } = require('./config');
const { readUsers, saveUsers } = require('./database');
const { banUser, unbanUser } = require('./bans');

const router = express.Router();
const upload = multer({ storage: multer.memoryStorage() });

function keysValid({ key1, key2, key3 }) {
    return key1 === ADMIN_KEYS.key1 && key2 === ADMIN_KEYS.key2 && key3 === ADMIN_KEYS.key3;
}

router.get('/admin/terminal', (req, res) => {
    res.sendFile(path.join(ROOT, 'admin.html'));
});

router.post('/api/admin/terminal', (req, res) => {
    const { command } = req.body;

    if (!keysValid(req.body)) {
        return res.status(403).json({ error: "Bad keys!" });
    }

    if (!command) {
        return res.status(400).json({ error: "No command given." });
    }

    const args = command.trim().split(/\s+/);
    const cmd = args[0].toLowerCase();

    const users = readUsers();

    switch (cmd) {
        case 'help':
            return res.json({
                output: `Available Commands:
- ban <userid> <time hours/inf> <reason> : Bans a user.
- unban <userid> : Lifts a user's ban.
- upload : Instructs how to use the upload endpoint.
- get-userid <username> : Returns the ID of a given username.
- help : Prints this menu.`
            });

        case 'get-userid': {
            const targetUser = args[1];
            if (!targetUser) return res.json({ output: "Error: Missing username. Usage: get-userid <username>" });

            const foundUser = users.find(u => u.username.toLowerCase() === targetUser.toLowerCase());
            if (foundUser) {
                return res.json({ output: `User ${foundUser.username} has ID: ${foundUser.id}` });
            } else {
                return res.json({ output: `Error: User '${targetUser}' not found.` });
            }
        }

        case 'ban': {
            const targetId = parseInt(args[1], 10);
            const timeParam = args[2];
            const reason = args.slice(3).join(' ');

            if (!targetId || !timeParam || !reason) {
                return res.json({ output: "Error: Invalid syntax. Usage: ban <userid> <time hours/inf> <reason>" });
            }

            const userIndex = users.findIndex(u => u.id === targetId);
            if (userIndex === -1) return res.json({ output: `Error: User ID ${targetId} not found.` });

            let hours = null;   // null = permanent
            if (timeParam.toLowerCase() !== 'inf') {
                hours = parseFloat(timeParam);
                if (!Number.isFinite(hours) || hours <= 0) return res.json({ output: "Error: Time must be a positive number of hours or 'inf'." });
            }

            banUser(users, userIndex, hours, reason);
            saveUsers(users);

            const durationText = hours === null ? "permanently" : `${timeParam} hours`;
            return res.json({ output: `Success: User ${users[userIndex].username} (ID: ${targetId}) has been banned ${durationText} for: ${reason}` });
        }

        case 'unban': {
            const targetId = parseInt(args[1], 10);

            if (!targetId) {
                return res.json({ output: "Error: Invalid syntax. Usage: unban <userid>" });
            }

            const userIndex = users.findIndex(u => u.id === targetId);
            if (userIndex === -1) return res.json({ output: `Error: User ID ${targetId} not found.` });

            const wasBanned = unbanUser(users, userIndex);
            saveUsers(users);

            if (!wasBanned) return res.json({ output: `User ${users[userIndex].username} (ID: ${targetId}) is not banned.` });
            return res.json({ output: `Success: User ${users[userIndex].username} (ID: ${targetId}) has been unbanned.` });
        }

        case 'upload':
            return res.json({
                output: "Upload sys ready! To upload an asset, make a multipart/form-data POST request to /api/admin/upload with 'type' (texture, sound, place), 'id', and the 'file' attached, along with your 3 admin keys."
            });

        default:
            return res.json({ output: `Unknown command: '${cmd}'. Type 'help' for a list of commands.` });
    }
});

router.post('/api/admin/upload', upload.single('file'), (req, res) => {
    const { type } = req.body;
    const file = req.file;

    if (!keysValid(req.body)) {
        return res.status(403).json({ error: "You didn't input the correct keys." });
    }

    if (!file || !type) {
        return res.status(400).json({ error: "Missing needed fields: 'file' or 'type'" });
    }

    let targetDir = '';
    if (type === 'texture') targetDir = './data/assets/textures';
    else if (type === 'sound') targetDir = './data/assets/sounds';
    else if (type === 'place') targetDir = './data/places';
    else return res.status(400).json({ error: "Invalid type. Must be 'texture', 'sound', or 'place'." });

    const dirPath = path.resolve(ROOT, targetDir);
    if (!fs.existsSync(dirPath)) fs.mkdirSync(dirPath, { recursive: true });

    const existingFiles = fs.readdirSync(dirPath);
    let maxId = 0;

    for (const f of existingFiles) {
        const parsedName = path.parse(f).name;
        const num = parseInt(parsedName, 10);
        if (!isNaN(num) && num > maxId) {
            maxId = num;
        }
    }

    const newId = maxId + 1;

    const ext = path.extname(file.originalname);
    const finalPath = path.join(dirPath, `${newId}${ext}`);

    fs.writeFileSync(finalPath, file.buffer);

    res.json({ message: `Successfully uploaded ${type}! Assigned new Asset ID: ${newId}` });
});

module.exports = router;
