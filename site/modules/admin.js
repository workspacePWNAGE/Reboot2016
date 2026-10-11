const express = require('express');
const fs = require('fs');
const path = require('path');
const multer = require('multer');
const { ROOT } = require('./config');
const { requireRole } = require('./session');
const { roleAtLeast } = require('./roles');
const { runCommand } = require('./adminCommands');
const adminLog = require('./adminLog');

const router = express.Router();
const upload = multer({ storage: multer.memoryStorage() });

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

router.post('/api/admin/terminal', requireRole('moderator'), (req, res) => {
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

    res.json({ output: runCommand(req.user, command) });
});

router.post('/api/admin/upload', requireRole('administrator'), upload.single('file'), (req, res) => {
    const { type } = req.body;
    const file = req.file;

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

    adminLog.record(req.user, `upload ${type}`, `Success: uploaded ${type} as asset ${newId}`);

    res.json({ message: `Successfully uploaded ${type}! Assigned new Asset ID: ${newId}` });
});

module.exports = router;