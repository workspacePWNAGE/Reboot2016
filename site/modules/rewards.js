const { saveUsers } = require('./database');
const { DAILY_BUX } = require('./config');

function utcDay(date) {
    return date.toISOString().slice(0, 10);
}

function claimDailyBux(users, index, now = new Date()) {
    const user = users[index];
    const today = utcDay(now);

    if (user.lastDailyBux === today) return 0;

    user.bux = (user.bux ?? 0) + DAILY_BUX;
    user.lastDailyBux = today;
    saveUsers(users);

    return DAILY_BUX;
}

module.exports = { claimDailyBux };