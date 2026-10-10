const express = require('express');
const bcrypt = require('bcrypt');
const crypto = require('crypto');
const { readUsers, saveUsers } = require('./database');
const { authLimiter } = require('./rateLimiter');
const { checkBan } = require('./bans');
const { isReservedUsername } = require('./usernames');
const { setSessionCookie } = require('./session');

const router = express.Router();

router.post('/api/register-user', authLimiter, async (req, res) => {
    const { username, password } = req.body;

    if (!username || !password) {
        return res.status(400).json({ error: "Username and password are required." });
    }

    const usernameRegex = /^[a-zA-Z0-9_]{3,20}$/;
    if (!usernameRegex.test(username)) {
        return res.status(400).json({ error: "Username needs to be 3-20 characters long and can only contain letters, numbers, and underscores." });
    }

    if (isReservedUsername(username)) {
        return res.status(400).json({ error: "That username is reserved." });
    }

    const users = readUsers();

    if (users.find(u => u.username.toLowerCase() === username.toLowerCase())) {
        return res.status(409).json({ error: "That username is already taken!" });
    }

    const hashedPassword = await bcrypt.hash(password, 10);
    const newId = users.length > 0 ? Math.max(...users.map(u => u.id)) + 1 : 1;

    const newUser = {
        id: newId,
        username: username,
        password: hashedPassword,
        banned: false,
        role: 'user',
        avatar: {
            hat1: 0,
            hat2: 0,
            hat3: 0,
            shirt: 0,
            pants: 0,
            bodycolors: { head: 1, torso: 1, left_arm: 1, right_arm: 1, left_leg: 1, right_leg: 1 },
            bodymeshes: { head: 0, torso: 0, left_arm: 0, right_arm: 0, left_leg: 0, right_leg: 0 }
        }
    };

    users.push(newUser);
    saveUsers(users);

    res.status(201).json({ message: "User registered!", userId: newId });
});

router.post('/api/login-user', authLimiter, async (req, res) => {
    const { username, password, token } = req.body;
    const users = readUsers();

    if (token) {
        const userIndex = users.findIndex(u => u.token === token);
        if (userIndex !== -1) {
            const ban = checkBan(users, userIndex);
            if (ban) return res.status(ban.status).json(ban.body);

            const { password: _, ...userData } = users[userIndex];
            return res.json({
                message: "Login successful via token!",
                cookieToken: token,
                user: userData
            });
        }
        return res.status(401).json({ error: "Invalid session token." });
    }

    if (!username || !password) {
        return res.status(400).json({ error: "Username and password are required." });
    }

    const userIndex = users.findIndex(u => u.username.toLowerCase() === username.toLowerCase());

    if (userIndex === -1) {
        return res.status(401).json({ error: "Bad username or password!" });
    }

    const user = users[userIndex];
    const passwordMatch = await bcrypt.compare(password, user.password);

    if (!passwordMatch) {
        return res.status(401).json({ error: "Bad username or password." });
    }

    const ban = checkBan(users, userIndex);
    if (ban) {
        if (!users[userIndex].token) {
            users[userIndex].token = crypto.randomBytes(32).toString('hex');
            saveUsers(users);
        }
        setSessionCookie(req, res, users[userIndex].token);
        return res.status(ban.status).json(ban.body);
    }

    const sessionToken = crypto.randomBytes(32).toString('hex');

    users[userIndex].token = sessionToken;
    saveUsers(users);

    setSessionCookie(req, res, sessionToken);

    const { password: _, ...userData } = users[userIndex];

    res.json({
        message: "Login successful!",
        cookieToken: sessionToken,
        user: userData
    });
});

module.exports = router;