const express = require('express');
const cors = require('cors');
const cookieParser = require('cookie-parser');

const { PORT } = require('./modules/config');
require('./modules/database');

const app = express();
app.use(cors());
app.use(express.json());
app.use(cookieParser());
app.set('trust proxy', true);

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