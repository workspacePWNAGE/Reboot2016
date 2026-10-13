const express = require('express');
const bcrypt = require('bcrypt');
const crypto = require('crypto');
const { readUsers, saveUsers } = require('./database');
const { authLimiter } = require('./rateLimiter');
const { checkBan } = require('./bans');
const { isReservedUsername } = require('./usernames');
const { setSessionCookie } = require('./session');
const { TURNSTILE_KEY } = require('./config');
const profanityFilter = require('./profanityFilter');

const router = express.Router();

router.post('/api/register-user', authLimiter, async (req, res) => {
    const { username, password, token } = req.body;

    const isValidCaptcha = await verifyTurnstile(token, req.ip);
    if (!isValidCaptcha) {
        return res.status(400).json({ error: "Please complete the captcha!" });
    }

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

    const check = profanityFilter.check(username);
    if (check.profane) {
        return res.status(400).json({ error: "Your username contains inappropriate text..." });
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

router.post('/api/login-user', async (req, res) => {
    const { username, password, token: captchaToken } = req.body;

    if (typeof captchaToken === 'string' && /^[0-9a-f]{64}$/.test(captchaToken)
        && !username && !password) {
        const users = readUsers();
        const index = users.findIndex(user => user.token === captchaToken);
        if (index === -1) return res.status(401).json({ error: "Invalid or expired session token." });
        const ban = checkBan(users, index);
        if (ban) return res.status(ban.status).json(ban.body);
        const { password: ignoredPassword, ...userData } = users[index];
        return res.json({ message: "Login successful!", cookieToken: captchaToken, user: userData });
    }


    const isValidCaptcha = !captchaToken || await verifyTurnstile(captchaToken, req.ip);
    if (!isValidCaptcha) {
        return res.status(400).json({ error: "Please complete the captcha!" });
    }

    if (typeof username !== "string" || typeof password !== "string"
        || !/^[a-zA-Z0-9_]{3,20}$/.test(username)
        || Buffer.byteLength(password, "utf8") < 8
        || Buffer.byteLength(password, "utf8") > 72) {
        return res.status(400).json({ error: "Valid username and password are required." });
    }

    let users = readUsers();

    let userIndex = users.findIndex(
        u => u.username.toLowerCase() === username.toLowerCase()
    );

    if (userIndex === -1) {
        return res.status(401).json({ error: "Bad username or password." });
    }

    const user = users[userIndex];
    const passwordMatch = await bcrypt.compare(password, user.password);

    if (!passwordMatch) {
        return res.status(401).json({ error: "Bad username or password." });
    }

    users = readUsers();
    userIndex = users.findIndex(current => current.id === user.id && current.password === user.password);
    if (userIndex === -1) return res.status(401).json({ error: "Bad username or password." });

    const ban = checkBan(users, userIndex);

    if (ban) {
        return res.status(ban.status).json(ban.body);
    }

    const sessionToken = crypto.randomBytes(32).toString('hex');

    users[userIndex].token = sessionToken;
    saveUsers(users);

    setSessionCookie(req, res, sessionToken);

    const { password: _, ...userData } = users[userIndex];

    return res.json({
        message: "Login successful!",
        cookieToken: sessionToken,
        user: userData
    });
});

async function verifyTurnstile(token, ip) {
    if (!token) return false;

    try {
        const res = await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                secret: TURNSTILE_KEY,
                response: token,
                remoteip: ip
            })
        });
        const data = await res.json();
        return data.success;
    } catch (err) {
        console.error('Turnstile verification error:', err);
        return false;
    }
}

module.exports = router;