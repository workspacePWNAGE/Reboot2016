const express = require('express');
const router = express.Router();
const { readUsers } = require('./database');

router.get('/profile', (req, res) => {
    const users = readUsers();
    let profileUser = null;
    let isOwnProfile = false;

    const idParam = req.query.id;

    if (idParam) {
        const targetId = parseInt(idParam, 10);
        profileUser = users.find(u => u.id === targetId);
        
        if (!profileUser) {
            return res.status(404).render('error', { error: "User couldn't be found!" });
        }

        if (req.user && req.user.id === profileUser.id) {
            isOwnProfile = true;
        }
    } else {
        if (!req.user) {
            return res.redirect('/login');
        }

        profileUser = users.find(u => u.id === req.user.id);

        if (!profileUser) {
            return res.status(404).render('error', { error: "Current user session not found!" });
        }

        isOwnProfile = true;
    }

    res.render('profile', {
        profileUser: {
            id: profileUser.id,
            username: profileUser.username,
            bio: profileUser.bio || "This person hasn't written a bio yet!",
        },
        isOwnProfile
    });
});

module.exports = router;