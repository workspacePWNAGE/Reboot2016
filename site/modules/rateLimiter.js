const rateLimit = require('express-rate-limit');
const { readUsers } = require('./database');

function hasValidToken(req) {
    const token = req.body.token;
    if (!token) return false;

    const users = readUsers();
    return users.some(u => u.token === token);
}

const authLimiter = rateLimit({
    windowMs: 15 * 60 * 1000,
    max: 5,
    message: { error: "Too many requests! Try again in 15 minutes." },
    standardHeaders: true,
    legacyHeaders: false,
    skip: (req, res) => hasValidToken(req)
});

module.exports = { authLimiter, hasValidToken };