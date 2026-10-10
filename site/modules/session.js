const express = require('express');
const { SESSION_COOKIE, SESSION_MAX_AGE_MS } = require('./config');
const { readUsers } = require('./database');
const { checkBan } = require('./bans');
const { claimDailyBux } = require('./rewards');
const { getRole, roleAtLeast } = require('./roles');

const TOKEN_PATTERN = /^[0-9a-f]{64}$/;

function cookieOptions(req) {
    return {
        httpOnly: true,
        sameSite: 'lax',
        secure: !!req.secure,
        path: '/'
    };
}

function setSessionCookie(req, res, token) {
    res.cookie(SESSION_COOKIE, token, { ...cookieOptions(req), maxAge: SESSION_MAX_AGE_MS });
}

function clearSessionCookie(req, res) {
    res.clearCookie(SESSION_COOKIE, cookieOptions(req));
}

function bannedMayVisit(path) {
    return path === '/moderated' || path.startsWith('/api/') || path.startsWith('/admin');
}

function middleware(req, res, next) {
    req.user = null;
    req.ban = null;
    res.locals.user = null;
    res.locals.ban = null;

    const token = req.cookies && req.cookies[SESSION_COOKIE];
    if (token === undefined) return next();

    if (typeof token === 'string' && TOKEN_PATTERN.test(token)) {
        const users = readUsers();
        const index = users.findIndex(u => u.token === token);

        if (index !== -1) {
            const u = users[index];
            const ban = checkBan(users, index);

            if (!ban) {
                const isPageVisit = req.method === 'GET' && !req.path.startsWith('/api/');
                res.locals.dailyBux = isPageVisit ? claimDailyBux(users, index) : 0;

                req.user = { id: u.id, username: u.username, bux: u.bux ?? 0, role: getRole(u) };
                res.locals.user = req.user;
                return next();
            }

            req.ban = {
                username: u.username,
                reason: ban.body.reason,
                permanent: ban.body.expiresAt === null,
                expiresAt: ban.body.expiresAt,
                expiresText: ban.body.expiresAt === null ? null : new Date(ban.body.expiresAt).toUTCString()
            };
            res.locals.ban = req.ban;

            if (req.method === 'GET' && !bannedMayVisit(req.path)) return res.redirect('/moderated');
            return next();
        }
    }

    clearSessionCookie(req, res);
    next();
}

function requireLogin(req, res, next) {
    if (req.user) return next();
    if (req.method === 'GET' && req.accepts(['html', 'json']) === 'html') return res.redirect('/login');
    res.status(401).json({ error: "You need to be logged in." });
}

function requireRole(minRole) {
    return (req, res, next) => {
        if (!req.user) return res.status(401).json({ error: "You need to be logged in." });
        if (!roleAtLeast(req.user, minRole)) return res.status(403).json({ error: "You don't have permission to do that." });
        next();
    };
}

const router = express.Router();

router.get('/moderated', (req, res) => {
    if (!req.ban) return res.redirect('/');
    res.render('moderated', { ban: req.ban });
});

router.post('/logout', (req, res) => {
    clearSessionCookie(req, res);
    res.redirect('/');
});

router.get('/logout', (req, res) => {
    clearSessionCookie(req, res);
    res.redirect('/');
});

module.exports = { middleware, router, requireLogin, requireRole, setSessionCookie, clearSessionCookie };