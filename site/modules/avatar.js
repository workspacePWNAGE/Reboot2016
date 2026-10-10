const express = require('express');
const router = express.Router();

router.get('/health', (req, res) => {
    res.json({
        status: 'ok',
        pending: 0,
        maxQueue: 10,
        completed: 0,
        failed: 0,
        maxSize: 1024
    });
});

router.get('/textures', (req, res) => {
    res.json({ files: [] });
});

router.post('/render', (req, res) => {
    const dummyBase64Png = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=";
    res.send(dummyBase64Png);
});

router.post('/jobs', (req, res) => {
    res.json({ jobId: '12345' });
});

router.get('/jobs/:id', (req, res) => {
    const dummyBase64Png = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII=";
    res.json({ status: 'done', image: dummyBase64Png });
});

module.exports = router;