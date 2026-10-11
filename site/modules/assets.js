const express = require('express');
const fs = require('fs');
const path = require('path');
const { ROOT, CONTENT_DIR } = require('./config');

const router = express.Router();

router.use(express.static(CONTENT_DIR));
router.use('/siteAssets', express.static(CONTENT_DIR));

function serveAssetById(req, res, relativeDir, allowedExtensions = null) {
    const id = req.query.id;
    if (!id || !/^\d+$/.test(id)) {
        return res.status(400).json({ error: "Bad or missing id; id must be a number!" });
    }

    const dirPath = path.resolve(ROOT, relativeDir);
    if (!fs.existsSync(dirPath)) {
        return res.status(404).json({ error: "Asset directory not found!" });
    }

    fs.readdir(dirPath, (err, files) => {
        if (err) return res.status(500).json({ error: "Internal server error..." });

        const matchedFile = files.find(file => {
            const parsed = path.parse(file);
            const isNameMatch = parsed.name === id;
            const isExtMatch = !allowedExtensions || allowedExtensions.includes(parsed.ext.toLowerCase());
            return isNameMatch && isExtMatch;
        });

        if (matchedFile) {
            res.sendFile(path.join(dirPath, matchedFile));
        } else {
            res.status(404).json({ error: "Asset not found!" });
        }
    });
}

router.get('/api/get-texture', (req, res) => serveAssetById(req, res, './data/assets/textures'));
router.get('/api/get-sound', (req, res) => serveAssetById(req, res, './data/assets/sounds'));
router.get('/api/get-place', (req, res) => serveAssetById(req, res, './data/places', ['.rbxl', '.rbxlx']));
router.get('/api/get-thumbnail/place', (req, res) => serveAssetById(req, res, './data/thumbnails/places'));
router.get('/api/get-thumbnail/player', (req, res) => serveAssetById(req, res, './data/thumbnails/players'));
router.get('/api/get-thumbnail/item', (req, res) => serveAssetById(req, res, './data/thumbnails/items'));

module.exports = router;