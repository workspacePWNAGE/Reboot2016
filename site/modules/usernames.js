// Default / reset usernames.
//
// A reset username is "player_<userid>". It is NOT subject to the 3-20 character rule that
// applies to signups, so it can be longer than 20 characters for large ids.
// To make sure nobody can sign up with someone else's future reset name, the whole
// player_<number> pattern is reserved at registration.

const RESERVED_PATTERN = /^player_\d+$/i;

function defaultUsername(userId) {
    return `player_${userId}`;
}

function isReservedUsername(name) {
    return RESERVED_PATTERN.test(String(name));
}

/**
 * Sets users[index].username to player_<id>. Does NOT save.
 * Returns { changed, oldName, newName, conflict }.
 * `conflict` is true (and nothing changes) if a different account already uses that exact name.
 */
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
