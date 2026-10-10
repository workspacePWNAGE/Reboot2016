const express = require('express');
const { VIEWS_DIR } = require('./config');
const { getPeoplePageData } = require('./people');

const PAGES = [
    { route: '/',       view: 'index' },
    { route: '/verity',       view: 'verity' },
    { route: '/login',  view: 'login' },
    { route: '/signup', view: 'signup' },
    { route: '/people', view: 'people', data: (req) => getPeoplePageData(req.query) },
];

module.exports = function mountPages(app) {
    app.set('view engine', 'ejs');
    app.set('views', VIEWS_DIR);

    const router = express.Router();

    for (const { route, view, data } of PAGES) {
        router.get(route, (req, res) => {
            const extra = typeof data === 'function' ? data(req) : data;
            res.render(view, { ...extra, query: req.query });
        });
    }

    app.use(router);
};