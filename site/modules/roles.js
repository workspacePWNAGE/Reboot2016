const ROLES = { user: 0, moderator: 1, administrator: 2 };

const LABELS = { user: 'User', moderator: 'Moderator', administrator: 'Administrator' };

const ALIASES = {
    user: 'user', member: 'user', none: 'user', normal: 'user',
    mod: 'moderator', moderator: 'moderator',
    admin: 'administrator', administrator: 'administrator'
};

function getRole(user) {
    return user && Object.prototype.hasOwnProperty.call(ROLES, user.role) ? user.role : 'user';
}

function rankOf(user) {
    return ROLES[getRole(user)];
}

function roleLabel(role) {
    return LABELS[role] || LABELS.user;
}

function withArticle(role) {
    const label = roleLabel(role);
    return `${/^[AEIOU]/i.test(label) ? 'an' : 'a'} ${label}`;
}

function roleAtLeast(user, minRole) {
    return rankOf(user) >= ROLES[minRole];
}

function parseRole(text) {
    const key = String(text || '').toLowerCase();
    return Object.prototype.hasOwnProperty.call(ALIASES, key) ? ALIASES[key] : null;
}

function canModerate(actor, target) {
    if (actor.id === target.id) return false;
    return rankOf(actor) > rankOf(target) || getRole(actor) === 'administrator';
}

module.exports = { ROLES, getRole, rankOf, roleLabel, withArticle, roleAtLeast, parseRole, canModerate };