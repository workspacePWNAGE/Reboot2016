const path = require('path');

const ROOT = path.join(__dirname, '..');

const colors = require(path.join(ROOT, 'colors.json'));

const ADMIN_KEYS = {
    key1: "dc060ab9a1c58e4edc0319342436db60b63b0da573d9ea8451efb240d67235b0",
    key2: "3c67fbb082aa8ea51b8cf0530db8b7dfa0ed52e1e4277d79c7c2be8b3c1addb3",
    key3: "e321e7f8dae23daa456624b57ca6a79fc5fa9af9097fec5bf5aee330bfc519b5"
};

module.exports = {
    ROOT,
    PORT: 4451,
    ADMIN_KEYS,
    TURNSTILE_KEY: "0x4AAAAAAFU_6yg_jnlruCG7kYthdkBxFIE",
    colors,
    validColorIds: new Set(colors.map(color => color.id)),
    VALID_BODY_PARTS: ['head', 'torso', 'leftArm', 'rightArm', 'leftLeg', 'rightLeg'],
    DB_DIR: path.join(ROOT, 'database'),
    USERS_FILE: path.join(ROOT, 'database', 'users.json'),
    CONTENT_DIR: path.join(ROOT, 'content'),
    VIEWS_DIR: path.join(ROOT, 'views'),
    SESSION_COOKIE: 'r16-token',
    SESSION_MAX_AGE_MS: 30 * 24 * 60 * 60 * 1000,
    DAILY_BUX: 5,
};