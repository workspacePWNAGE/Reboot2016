const express = require('express');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const { ROOT, ADMIN_KEYS } = require('./config');
const { readUsers, saveUsers } = require('./database');

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

            let banExpiration = null;
            if (timeParam.toLowerCase() !== 'inf') {
                const hours = parseFloat(timeParam);
                if (isNaN(hours)) return res.json({ output: "Error: Time must be a number of hours or 'inf'." });
                banExpiration = Date.now() + (hours * 60 * 60 * 1000);
            }

            users[userIndex].banned = true;
            users[userIndex].banReason = reason;
            users[userIndex].banExpiration = banExpiration;
            saveUsers(users);

            const durationText = banExpiration ? `${timeParam} hours` : "permanently";
            return res.json({ output: `Success: User ${users[userIndex].username} (ID: ${targetId}) has been banned ${durationText} for: ${reason}` });
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