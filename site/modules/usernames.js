const RESERVED_PATTERN = /^player_\d+$/i;

function defaultUsername(userId) {
    return `player_${userId}`;
}

function isReservedUsername(name) {
    return RESERVED_PATTERN.test(String(name));
}

function resetUsername(users, index) {
    const user = users[index];
    const newName = defaultUsername(user.id);
    const oldName = user.username;

    if (oldName === newName) return { changed: false, oldName, newName, conflict: false };

    const taken = users.some((u, i) => i !== index && u.username.toLowerCase() === newName.toLowerCase());
    if (taken) return { changed: false, oldName, newName, conflict: true };

    user.username = newName;
    return { changed: true, oldName, newName, conflict: false };
}

module.exports = { defaultUsername, isReservedUsername, resetUsername };
