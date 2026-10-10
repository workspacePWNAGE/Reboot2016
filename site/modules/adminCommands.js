const { readUsers, saveUsers } = require('./database');
const { isBanned, banUser, unbanUser } = require('./bans');
const { resetUsername } = require('./usernames');
const { ROLES, getRole, rankOf, roleLabel, withArticle, parseRole, canModerate } = require('./roles');
const adminLog = require('./adminLog');

const MAX_BUX_GRANT = 1000000;

function findIndexById(users, raw) {
    if (!/^\d+$/.test(raw || '')) return -1;
    const id = parseInt(raw, 10);
    return users.findIndex(u => u.id === id);
}

function banText(user) {
    if (!isBanned(user)) return 'no';
    const until = user.banExpiration === null || user.banExpiration === undefined
        ? 'permanently'
        : `until ${new Date(user.banExpiration).toUTCString()}`;
    return `yes, ${until} (${user.banReason})`;
}

const COMMAND_LIST = [
    {
        name: 'help', role: 'moderator',
        usage: 'help', desc: 'Prints the commands you can use.',
        run: ({ actor }) => {
            const mine = COMMAND_LIST.filter(c => rankOf(actor) >= ROLES[c.role]);
            return `Available Commands (${roleLabel(getRole(actor))}):\n` +
                mine.map(c => `- ${c.usage} : ${c.desc}`).join('\n');
        }
    },
    {
        name: 'whoami', role: 'moderator',
        usage: 'whoami', desc: 'Shows who you are logged in as.',
        run: ({ actor }) => `Logged in as ${actor.username} (ID: ${actor.id}) - ${roleLabel(getRole(actor))}`
    },
    {
        name: 'get-userid', role: 'moderator',
        usage: 'get-userid <username>', desc: 'Returns the ID of a given username.',
        run: ({ args, users }) => {
            const name = args[0];
            if (!name) return 'Error: Missing username. Usage: get-userid <username>';

            const found = users.find(u => u.username.toLowerCase() === name.toLowerCase());
            return found ? `User ${found.username} has ID: ${found.id}` : `Error: User '${name}' not found.`;
        }
    },
    {
        name: 'userinfo', role: 'moderator',
        usage: 'userinfo <userid>', desc: "Shows a user's name, role, Bux and ban status.",
        run: ({ args, users }) => {
            const index = findIndexById(users, args[0]);
            if (!args[0]) return 'Error: Invalid syntax. Usage: userinfo <userid>';
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const u = users[index];
            return `User: ${u.username} (ID: ${u.id})\nRole: ${roleLabel(getRole(u))}\nBux: ${u.bux ?? 0}\nBanned: ${banText(u)}`;
        }
    },
    {
        name: 'ban', role: 'moderator', audit: true,
        usage: 'ban <userid> <time hours/inf> <reason>', desc: 'Bans a user (and resets their username).',
        run: ({ actor, args, users }) => {
            const timeParam = args[1];
            const reason = args.slice(2).join(' ');

            if (!/^\d+$/.test(args[0] || '') || !timeParam || !reason) {
                return 'Error: Invalid syntax. Usage: ban <userid> <time hours/inf> <reason>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't ban ${users[index].username}.`;

            let hours = null;
            if (timeParam.toLowerCase() !== 'inf') {
                hours = parseFloat(timeParam);
                if (!Number.isFinite(hours) || hours <= 0) return "Error: Time must be a positive number of hours or 'inf'.";
            }

            banUser(users, index, hours, reason);
            const bannedName = users[index].username;
            const reset = resetUsername(users, index);
            saveUsers(users);

            const durationText = hours === null ? 'permanently' : `${timeParam} hours`;
            let output = `Success: User ${bannedName} (ID: ${users[index].id}) has been banned ${durationText} for: ${reason}`;
            if (reset.changed) output += `\nTheir username was reset to ${reset.newName}.`;
            else if (reset.conflict) output += `\nWarning: couldn't reset their username, another account already uses ${reset.newName}.`;
            return output;
        }
    },
    {
        name: 'unban', role: 'moderator', audit: true,
        usage: 'unban <userid>', desc: "Lifts a user's ban.",
        run: ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '')) return 'Error: Invalid syntax. Usage: unban <userid>';

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't unban ${users[index].username}.`;

            const wasBanned = unbanUser(users, index);
            saveUsers(users);

            const u = users[index];
            return wasBanned
                ? `Success: User ${u.username} (ID: ${u.id}) has been unbanned.`
                : `User ${u.username} (ID: ${u.id}) is not banned.`;
        }
    },
    {
        name: 'reset-username', role: 'moderator', audit: true,
        usage: 'reset-username <userid>', desc: "Resets a user's username to player_<userid>.",
        run: ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '')) return 'Error: Invalid syntax. Usage: reset-username <userid>';

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;
            if (!canModerate(actor, users[index])) return `Error: You can't reset the username of ${users[index].username}.`;

            const reset = resetUsername(users, index);

            if (reset.conflict) return `Error: Another account already uses the username ${reset.newName}.`;
            if (!reset.changed) return `User ID ${users[index].id} already has the username ${reset.newName}.`;

            saveUsers(users);
            return `Success: Username of ID ${users[index].id} reset from ${reset.oldName} to ${reset.newName}.`;
        }
    },
    {
        name: 'give-bux', role: 'administrator', audit: true,
        usage: 'give-bux <userid> <amount>', desc: 'Gives a user Bux.',
        run: ({ args, users }) => {
            if (!/^\d+$/.test(args[0] || '') || !/^\d+$/.test(args[1] || '')) {
                return 'Error: Invalid syntax. Usage: give-bux <userid> <amount>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const amount = parseInt(args[1], 10);
            if (amount < 1 || amount > MAX_BUX_GRANT) return `Error: Amount must be between 1 and ${MAX_BUX_GRANT}.`;

            const u = users[index];
            u.bux = (u.bux ?? 0) + amount;
            saveUsers(users);

            return `Success: Gave ${amount} Bux to ${u.username} (ID: ${u.id}). New balance: ${u.bux}.`;
        }
    },
    {
        name: 'set-role', role: 'administrator', audit: true,
        usage: 'set-role <userid> <user/moderator/administrator>', desc: "Sets a user's role.",
        run: ({ actor, args, users }) => {
            if (!/^\d+$/.test(args[0] || '') || !args[1]) {
                return 'Error: Invalid syntax. Usage: set-role <userid> <user/moderator/administrator>';
            }

            const index = findIndexById(users, args[0]);
            if (index === -1) return `Error: User ID ${args[0]} not found.`;

            const role = parseRole(args[1]);
            if (!role) return `Error: Unknown role '${args[1]}'. Roles: user, moderator, administrator.`;

            const u = users[index];
            if (u.id === actor.id) return "Error: You can't change your own role.";
            if (getRole(u) === role) return `${u.username} (ID: ${u.id}) is already ${withArticle(role)}.`;

            const before = getRole(u);
            u.role = role;
            saveUsers(users);

            return `Success: ${u.username} (ID: ${u.id}) changed from ${roleLabel(before)} to ${roleLabel(role)}.`;
        }
    },
    {
        name: 'audit', role: 'administrator',
        usage: 'audit [count]', desc: 'Shows the latest staff actions (default 10, max 50).',
        run: ({ args }) => {
            const count = Math.min(Math.max(parseInt(args[0], 10) || 10, 1), 50);
            const entries = adminLog.recent(count);
            if (!entries.length) return 'No staff actions recorded yet.';
            return entries.map(e => `${e.time}  ${e.actor} (${e.actorId}): ${e.command}  ->  ${e.result}`).join('\n');
        }
    },
    {
        name: 'upload', role: 'administrator',
        usage: 'upload', desc: 'Instructs how to use the upload endpoint.',
        run: () => "Upload sys ready! To upload an asset, make a multipart/form-data POST request to /api/admin/upload with 'type' (texture, sound, place) and the 'file' attached, while logged in as an administrator."
    }
];

const COMMANDS = new Map(COMMAND_LIST.map(c => [c.name, c]));

function runCommand(actor, line) {
    const args = line.trim().split(/\s+/);
    const name = args.shift().toLowerCase();

    const cmd = COMMANDS.get(name);
    if (!cmd) return `Unknown command: '${name}'. Type 'help' for a list of commands.`;
    if (rankOf(actor) < ROLES[cmd.role]) return `You don't have permission to use '${name}'.`;

    const output = cmd.run({ actor, args, users: readUsers() });
    if (cmd.audit) adminLog.record(actor, line, output);
    return output;
}

module.exports = { runCommand, COMMAND_LIST };