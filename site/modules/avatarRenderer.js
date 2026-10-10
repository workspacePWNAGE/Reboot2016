'use strict';

const fs = require('fs');
const path = require('path');
const { ROOT, colors } = require('./config');

const RENDERER_DIR = path.join(ROOT, 'renderer');
const SHADER_DIR = path.join(RENDERER_DIR, 'shaders');
const PAGE_JS = path.join(RENDERER_DIR, 'page.js');
const THUMB_DIR = path.join(ROOT, 'data', 'thumbnails', 'players');

const LAUNCH_ARGS = [
    '--use-gl=angle',
    '--use-angle=swiftshader',
    '--enable-unsafe-swiftshader',
    '--ignore-gpu-blocklist',
    '--no-sandbox'
];

const DEFAULT_OPTIONS = {
    width: 420,
    height: 420,
    supersample: 2,
    yaw: 20,
    pitch: 8,
    fov: 35,
    defines: { PIN_SKINNED: 1, PIN_HQ: 1 }
};

function loadShader(name, seen = new Set()) {
    const src = fs.readFileSync(path.join(SHADER_DIR, name), 'utf8');
    return src.replace(/^[ \t]*#include\s+"([^"]+)"[ \t]*$/gm, (_, inc) => {
        if (seen.has(inc)) return '';
        seen.add(inc);
        return loadShader(inc, seen);
    });
}

function loadAllShaders() {
    return {
        vert: loadShader('default.vert'),
        frag: loadShader('default.frag'),
        plastic: loadShader('plastic.frag')
    };
}

const colorById = new Map(colors.map(c => [Number(c.id), c]));
const FALLBACK_RGB = [163, 162, 165];

function toRgb(c) {
    if (c === undefined || c === null) return FALLBACK_RGB;
    if (Array.isArray(c)) return c.slice(0, 3).map(Number);
    if (typeof c === 'number') return [(c >> 16) & 255, (c >> 8) & 255, c & 255];
    if (typeof c === 'string') {
        const m = c.trim().replace(/^#|^0x/i, '');
        if (/^[0-9a-f]{6}$/i.test(m)) return [0, 2, 4].map(i => parseInt(m.slice(i, i + 2), 16));
        if (/^[0-9a-f]{3}$/i.test(m)) return [0, 1, 2].map(i => parseInt(m[i] + m[i], 16));
        const parts = c.match(/\d+(\.\d+)?/g);
        if (parts && parts.length >= 3) return toRgb(parts.slice(0, 3).map(Number));
        return FALLBACK_RGB;
    }
    if (typeof c === 'object') {
        for (const key of ['hex', 'color', 'rgb', 'value', 'Color', 'code']) {
            if (c[key] !== undefined) return toRgb(c[key]);
        }
        const r = c.r ?? c.R, g = c.g ?? c.G, b = c.b ?? c.B;
        if (r !== undefined && g !== undefined && b !== undefined) {
            const v = [r, g, b].map(Number);
            const fractional = v.some(x => x % 1 !== 0);
            return fractional && v.every(x => x <= 1) ? v.map(x => Math.round(x * 255)) : v;
        }
    }
    return FALLBACK_RGB;
}

function colorForId(id) {
    return toRgb(colorById.get(Number(id)));
}

function resolvePartColors(avatar = {}) {
    const bc = avatar.bodycolors || {};
    return {
        head: colorForId(bc.head),
        torso: colorForId(bc.torso),
        leftArm: colorForId(bc.left_arm),
        rightArm: colorForId(bc.right_arm),
        leftLeg: colorForId(bc.left_leg),
        rightLeg: colorForId(bc.right_leg)
    };
}

let browser = null;
let page = null;
let starting = null;
let queue = Promise.resolve();

function loadPlaywright() {
    try { return require('playwright'); } catch (_) { /* fall through */ }
    try { return require('playwright-core'); } catch (_) { /* fall through */ }
    throw new Error("Playwright isn't installed. Run: npm i playwright && npx playwright install chromium");
}

async function ensurePage() {
    if (page && browser && browser.isConnected() && !page.isClosed()) return page;
    if (starting) return starting;

    starting = (async () => {
        if (browser) { try { await browser.close(); } catch (_) {} }
        const { chromium } = loadPlaywright();
        browser = await chromium.launch({
            args: LAUNCH_ARGS,
            executablePath: process.env.CHROMIUM_PATH || undefined
        });
        page = await browser.newPage();
        await page.setContent('<!doctype html><html><body style="margin:0;background:transparent"></body></html>');
        await page.addScriptTag({ content: fs.readFileSync(PAGE_JS, 'utf8') });
        await page.evaluate((sources) => window.AvatarGL.init(sources), loadAllShaders());
        return page;
    })();

    try { return await starting; }
    finally { starting = null; }
}

function enqueue(task) {
    const run = queue.then(task);
    queue = run.catch(() => {});
    return run;
}

function renderAvatar(avatar, options = {}) {
    return enqueue(async () => {
        const p = await ensurePage();
        const opts = { ...DEFAULT_OPTIONS, ...options, colors: resolvePartColors(avatar) };
        try {
            const b64 = await p.evaluate((o) => window.AvatarGL.render(o), opts);
            return Buffer.from(b64, 'base64');
        } catch (err) {
            if (!p.isClosed() && browser && browser.isConnected()) throw err;
            page = null;
            throw err;
        }
    });
}

async function renderPlayerThumbnail(user, options = {}) {
    const png = await renderAvatar(user.avatar, options);
    fs.mkdirSync(THUMB_DIR, { recursive: true });
    const file = path.join(THUMB_DIR, `${user.id}.png`);
    fs.writeFileSync(file, png);
    return file;
}

function compileTest(defines) {
    return enqueue(async () => {
        const p = await ensurePage();
        return p.evaluate((d) => window.AvatarGL.compileTest(d), defines);
    });
}

async function close() {
    const b = browser;
    browser = null;
    page = null;
    if (b) { try { await b.close(); } catch (_) {} }
}

module.exports = { renderAvatar, renderPlayerThumbnail, compileTest, close, resolvePartColors, toRgb };