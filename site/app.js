const express = require('express');
const cookieParser = require('cookie-parser');

const { PORT } = require('./modules/config');
require('./modules/database');

const app = express();
app.use(express.json());
app.use(cookieParser());

app.use(require('./modules/assets'));
require('./modules/pages')(app);
app.use(require('./modules/auth'));
app.use(require('./modules/users'));
app.use(require('./modules/admin'));

app.listen(PORT, () => {
    console.log(`running @ http://localhost:${PORT}`);
});