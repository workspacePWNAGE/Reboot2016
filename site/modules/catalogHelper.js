const { readItems } = require('./items');
const { readUsers } = require('./database');

const TYPES = ['hat', 'shirt', 'pants', 'face'];

function getOwnerCounts() {
    const counts = new Map();
    for (const user of readUsers()) {
        if (!Array.isArray(user.inventory)) continue;
        for (const id of new Set(user.inventory.map(Number))) {
            counts.set(id, (counts.get(id) || 0) + 1);
        }
    }
    return counts;
}

const price = i => Number(i.price) || 0;
const SORTS = {
    newest: (a, b) => b.id - a.id,
    oldest: (a, b) => a.id - b.id,
    sales: (a, b) => (b.sales - a.sales) || (b.id - a.id),
    'price-low': (a, b) => (price(a) - price(b)) || (b.id - a.id),
    'price-high': (a, b) => (price(b) - price(a)) || (b.id - a.id)
};

function getCatalogPageData(query) {
    const ownerCounts = getOwnerCounts();
    const items = readItems().map(i => ({ ...i, sales: ownerCounts.get(Number(i.id)) || 0 }));

    const q = (query.q || '').trim().toLowerCase();
    const type = TYPES.includes(query.type) ? query.type : '';
    const sort = Object.prototype.hasOwnProperty.call(SORTS, query.sort) ? query.sort : '';

    const filtered = items.filter(i =>
        (!type || i.type === type) &&
        (!q || (i.name && i.name.toLowerCase().includes(q)) || (i.description && i.description.toLowerCase().includes(q)))
    );
    if (sort) filtered.sort(SORTS[sort]);

    const page = Math.max(1, parseInt(query.page, 10) || 1);
    const limit = 12;
    const total = filtered.length;
    const totalPages = Math.ceil(total / limit) || 1;
    const currentPage = Math.min(page, totalPages);

    const startIndex = (currentPage - 1) * limit;
    const catalogItems = filtered.slice(startIndex, startIndex + limit);

    const prevPage = currentPage > 1 ? currentPage - 1 : null;
    const nextPage = currentPage < totalPages ? currentPage + 1 : null;

    const buildUrl = (overrides = {}) => {
        const v = { q, type, sort, page: 1, ...overrides };
        const params = new URLSearchParams();
        if (v.q) params.set('q', v.q);
        if (v.type) params.set('type', v.type);
        if (v.sort) params.set('sort', v.sort);
        if (v.page > 1) params.set('page', v.page);
        const str = params.toString();
        return `/catalog${str ? '?' + str : ''}`;
    };
    const pageUrl = p => buildUrl({ page: p });
    const filterUrl = overrides => buildUrl({ ...overrides, page: 1 });

    const paginationItems = [];
    for (let i = 1; i <= totalPages; i++) {
        if (i === 1 || i === totalPages || (i >= currentPage - 2 && i <= currentPage + 2)) {
            paginationItems.push({ page: i, current: i === currentPage });
        } else if (paginationItems[paginationItems.length - 1]?.page !== '...') {
            paginationItems.push({ ellipsis: true });
        }
    }

    return {
        catalogItems,
        q,
        type,
        sort,
        total,
        totalPages,
        currentPage,
        from: total === 0 ? 0 : startIndex + 1,
        to: Math.min(startIndex + limit, total),
        prevPage,
        nextPage,
        pageUrl,
        filterUrl,
        paginationItems
    };
}

module.exports = { getCatalogPageData };
