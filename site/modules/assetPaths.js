const path = require('path');
const { ROOT } = require('./config');

const TEXTURE_DIR = path.resolve(ROOT, 'data', 'assets', 'textures');
const MESH_DIR = path.resolve(ROOT, 'data', 'assets', 'meshes');
const SOUND_DIR = path.resolve(ROOT, 'data', 'assets', 'sounds');
const PLACE_DIR = path.resolve(ROOT, 'data', 'places');
const HAT_DIR = path.resolve(ROOT, 'data', 'models');

const RENDERER_TEXTURE_DIR = process.env.RENDERER_TEXTURE_DIR
    ? path.resolve(process.env.RENDERER_TEXTURE_DIR)
    : path.resolve(ROOT, 'content', 'textures');

module.exports = {
    TEXTURE_DIR,
    MESH_DIR,
    SOUND_DIR,
    PLACE_DIR,
    HAT_DIR,
    RENDERER_TEXTURE_DIR
};
