const { saveUsers } = require('./database');

function isBanned(user, now = Date.now()) {
    if (!user || !user.banned) return false;
    if (user.banExpiration === null || user.banExpiration === undefined) return true;
    return now < user.banExpiration;
}

function clearBan(user) {
    user.banned = false;
    delete user.banReason;
    delete user.banExpiration;
    delete user.bannedAt;
}

function banUser(users, index, hours, reason) {
    const user = users[index];
    user.banned = true;
    user.banReason = reason;
    user.banExpiration = hours === null ? null : Date.now() + hours * 60 * 60 * 1000;
    user.bannedAt = Date.now();
    return user;
}

function unbanUser(users, index) {
    const user = users[index];
    if (!isBanned(user)) {
        // clear out an expired leftover too
        if (user.banned) clearBan(user);
        return false;
    }
    clearBan(user);
    return true;
}

function checkBan(users, index) {
    const user = users[index];
    if (!user || !user.banned) return null;

    if (!isBanned(user)) {
        clearBan(user);
        saveUsers(users);
        return null;
    }

    const permanent = user.banExpiration === null || user.banExpiration === undefined;
    return {
        status: 403,
        body: {
            error: permanent
                ? `This account has been permanently banned. Reason: ${user.banReason}`
                : `This account is banned until ${new Date(user.banExpiration).toUTCString()}. Reason: ${user.banReason}`,
            banned: true,
            reason: user.banReason,
            expiresAt: permanent ? null : user.banExpiration
        }
    };
}

module.exports = { isBanned, banUser, unbanUser, checkBan };
