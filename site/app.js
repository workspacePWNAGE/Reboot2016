const express = require('express');
const cors = require('cors');
const cookieParser = require('cookie-parser');
const crypto = require('crypto'); // Make sure crypto is required

const { PORT } = require('./modules/config');
require('./modules/database');

const app = express();
app.use(cors());
app.use(express.json());
app.use(cookieParser());
app.set('trust proxy', true);

app.use((req, res, next) => {
    res.locals.nonce = crypto.randomBytes(16).toString('base64');
    
    res.setHeader(
        "Content-Security-Policy", 
        `script-src 'self' 'nonce-${res.locals.nonce}' https://challenges.cloudflare.com; frame-src 'self' https://challenges.cloudflare.com;`
    );
    next();
});

app.use(require('./modules/assets'));
const session = require('./modules/session');
app.use(session.middleware);
app.use(session.router);
require('./modules/pages')(app);
app.use(require('./modules/avatar'));
app.use(require('./modules/auth'));
app.use(require('./modules/users'));
app.use(require('./modules/admin'));
app.use(require('./modules/profile'));

app.listen(PORT, () => {
    console.log(`running @ http://localhost:${PORT}`);
});