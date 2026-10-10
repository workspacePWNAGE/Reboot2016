const express = require('express');
const { VIEWS_DIR } = require('./config');
const { getPeoplePageData } = require('./people');
const { readUsers } = require('./database');

const PAGES = [
    { route: '/', view: 'index' },
    { route: '/verity', view: 'verity' },
    { route: '/people', view: 'people', data: (req) => getPeoplePageData(req.query) },
];

module.exports = function mountPages(app) {
    app.set('view engine', 'ejs');
    app.set('views', VIEWS_DIR);

    const router = express.Router();

    const redirectIfLoggedIn = (req, res, next) => {
        if (req.user) {
            return res.redirect('/');
        }
        next();
    };

    router.get('/login', redirectIfLoggedIn, (req, res) => {
        res.render('login', { query: req.query });
    });

    router.get('/signup', redirectIfLoggedIn, (req, res) => {
        res.render('signup', { query: req.query });
    });

    router.get('/settings', (req, res) => {
        if (!req.user) {
            return res.redirect('/login');
        }
        const users = readUsers();
        const user = users.find(u => u.id === req.user.id);
        
        res.render('settings', { 
            query: req.query, 
            bio: user ? (user.bio || '') : '' 
        });
    });

    for (const { route, view, data } of PAGES) {
        router.get(route, (req, res) => {
            const extra = typeof data === 'function' ? data(req) : data;
            res.render(view, { ...extra, query: req.query });
        });
    }

    app.use(router);
};