const express = require('express');
const { VIEWS_DIR } = require('./config');
const { getPeoplePageData } = require('./people');
const { getCatalogPageData } = require('./catalogHelper');
const { readUsers } = require('./database');

const PAGES = [
    { route: '/', view: 'index' },
    { route: '/verity', view: 'verity' },
    { route: '/info', view: 'info' },
    { route: '/people', view: 'people', data: req => getPeoplePageData(req.query) },
    { route: '/catalog', view: 'catalog', data: req => getCatalogPageData(req.query) }
];

module.exports = function mountPages(app) {
    app.set('view engine', 'ejs');
    app.set('views', VIEWS_DIR);

    const router = express.Router();

    const redirectIfLoggedIn = (req, res, next) => {
        if (req.user) return res.redirect('/');
        next();
    };

    router.get('/login', redirectIfLoggedIn, (req, res) => {
        res.render('login', { query: req.query });
    });

    router.get('/signup', redirectIfLoggedIn, (req, res) => {
        res.render('signup', { query: req.query });
    });

    router.get('/settings', (req, res) => {
        if (!req.user) return res.redirect('/signup');

        const user = readUsers().find(u => u.id === req.user.id);

        res.render('settings', {
            query: req.query,
            bio: user ? user.bio || '' : ''
        });
    });

    router.get('/character', (req, res) => {
        if (!req.user) return res.redirect('/signup');

        const user = readUsers().find(u => u.id === req.user.id);

        if (!user) {
            return res.redirect('/signup');
        }

        res.render('character', {
            query: req.query,
            user
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