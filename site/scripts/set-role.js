const { readUsers, saveUsers } = require('../modules/database');
const { parseRole, withArticle } = require('../modules/roles');

const [, , who, roleArg] = process.argv;
const role = parseRole(roleArg);

if (!who || !role) {
    console.log('Usage: node scripts/set-role.js <userid or username> <user|moderator|administrator>');
    process.exit(1);
}

const users = readUsers();
const index = /^\d+$/.test(who)
    ? users.findIndex(u => u.id === parseInt(who, 10))
    : users.findIndex(u => u.username.toLowerCase() === who.toLowerCase());

if (index === -1) {
    console.log(`User '${who}' not found.`);
    process.exit(1);
}

users[index].role = role;
saveUsers(users);
console.log(`${users[index].username} (ID: ${users[index].id}) is now ${withArticle(role)}.`);
