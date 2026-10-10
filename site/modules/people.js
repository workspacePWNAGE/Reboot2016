const { readUsers } = require('./database');

const PER_PAGE = 14;
const MAX_QUERY_LENGTH = 20;
const PAGE_WINDOW = 2;

function buildPageItems(page, totalPages) {
    const wanted = new Set([1, totalPages]);
    for (let p = page - PAGE_WINDOW; p <= page + PAGE_WINDOW; p++) {
        if (p >= 1 && p <= totalPages) wanted.add(p);
    }

    const items = [];
    let last = 0;
    for (const p of [...wanted].sort((a, b) => a - b)) {
        if (p - last > 1) items.push({ ellipsis: true });
        items.push({ page: p, current: p === page });
        last = p;
    }
    return items;
}

function getPeoplePageData(query = {}) {
    const q = String(query.q ?? '').trim().slice(0, MAX_QUERY_LENGTH);
    const needle = q.toLowerCase();

    const matches = readUsers()
        .filter(u => !needle || u.username.toLowerCase().includes(needle))
        .sort((a, b) => a.id - b.id);

    const totalPages = Math.max(1, Math.ceil(matches.length / PER_PAGE));

    let page = parseInt(query.page, 10);
    if (!Number.isInteger(page) || page < 1) page = 1;
    if (page > totalPages) page = totalPages;

    const start = (page - 1) * PER_PAGE;
    const people = matches.slice(start, start + PER_PAGE).map(u => ({ id: u.id, username: u.username }));

    const pageUrl = (p) => `/people?page=${p}${q ? `&q=${encodeURIComponent(q)}` : ''}`;

    return {
        q,
        people,
        page,
        totalPages,
        total: matches.length,
        from: people.length ? start + 1 : 0,
        to: start + people.length,
        items: buildPageItems(page, totalPages),
        prevPage: page > 1 ? page - 1 : null,
        nextPage: page < totalPages ? page + 1 : null,
        pageUrl
    };
}

module.exports = { getPeoplePageData, PER_PAGE };
