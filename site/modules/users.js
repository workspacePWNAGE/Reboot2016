const express = require('express');
const { validColorIds } = require('./config');
const { readUsers, saveUsers } = require('./database');
const { isBanned, checkBan } = require('./bans');

const router = express.Router();

router.get('/api/get-user-data', (req, res) => {
    const id = req.query.id;

    if (!id || !/^\d+$/.test(id)) {
        return res.status(400).json({ error: "Bad or missing id; id must be a number." });
    }

    const users = readUsers();
    const user = users.find(u => u.id === parseInt(id, 10));

    if (!user) {
        return res.status(404).json({ error: "User couldn't be found!" });
    }

    res.json({
        id: user.id,
        username: user.username,
        avatar: user.avatar,
        banned: isBanned(user)
    });
});

router.post('/api/update-bio', (req, res) => {
    if (!req.user) {
        return res.status(401).json({ error: "You must be logged in to update your bio!" });
    }

    let { bio } = req.body;

    if (typeof bio !== 'string') {
        return res.status(400).json({ error: "Bad bio format." });
    }

    bio = bio.trim();

    if (bio.length > 200) {
        return res.status(400).json({ error: "Bio cannot go past 200 characters!" });
    }

    const check = profanityFilter.check(bio);
    if (check.profane) {
        return res.status(400).json({ error: "Your bio contains inappropriate text..." });
    }

    const users = readUsers();
    const userIndex = users.findIndex(u => u.id === req.user.id);

    if (userIndex === -1) {
        return res.status(404).json({ error: "User not found." });
    }

    users[userIndex].bio = bio;
    saveUsers(users);

    res.json({ message: "Bio updated successfully!" });
});

router.post('/api/update-avatar', (req, res) => {
    const { token, bodycolors } = req.body;

    if (!token) {
        return res.status(401).json({ error: "Missing session token!" });
    }

    if (!bodycolors) {
        return res.status(400).json({ error: "Missing bodycolors data!" });
    }

    const users = readUsers();
    const userIndex = users.findIndex(u => u.token === token);

    if (userIndex === -1) {
        return res.status(401).json({ error: "Invalid or expired session token..." });
    }

    const ban = checkBan(users, userIndex);
    if (ban) return res.status(ban.status).json(ban.body);

    const validParts = ['head', 'torso', 'left_arm', 'right_arm', 'left_leg', 'right_leg'];
    let hasChanges = false;

    for (const part of validParts) {
        if (bodycolors[part] !== undefined) {
            const colorId = parseInt(bodycolors[part], 10);

            if (!validColorIds.has(colorId)) {
                return res.status(400).json({
                    error: `Invalid color ID '${colorId}' provided for '${part}'.`
                });
            }

            users[userIndex].avatar.bodycolors[part] = colorId;
            hasChanges = true;
        }
    }

    if (hasChanges) {
        saveUsers(users);
        return res.json({
            message: "Body colors updated successfully!",
            avatar: users[userIndex].avatar
        });
    } else {
        return res.status(400).json({ error: "No body parts provided to update..." });
    }
});

module.exports = router;
