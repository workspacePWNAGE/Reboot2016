const express = require('express');
const { VIEWS_DIR } = require('./config');
 
const PAGES = [
    { route: '/', view: 'index' },
    { route: '/verity', view: 'verity' },
    { route: '/signup', view: 'signup' },
    { route: '/login', view: 'login' }
];

module.exports = function mountPages(app) {
    app.set('view engine', 'ejs');
    app.set('views', VIEWS_DIR);

    const router = express.Router();

    for (const { route, view, data } of PAGES) {
        router.get(route, (req, res) => {
            res.render(view, { ...data, query: req.query });
        });
    }

    app.use(router);
};