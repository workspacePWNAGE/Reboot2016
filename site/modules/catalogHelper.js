const { readItems } = require('./items');
const { readUsers } = require('./database');

// item id -> how many users own it (counted once per user)
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

function getCatalogPageData(query) {
    const items = readItems();
    const q = (query.q || '').trim().toLowerCase();
    
    const filtered = q ? items.filter(i => 
        (i.name && i.name.toLowerCase().includes(q)) || 
        (i.description && i.description.toLowerCase().includes(q))
    ) : items;

    const page = Math.max(1, parseInt(query.page, 10) || 1);
    const limit = 12;
    const total = filtered.length;
    const totalPages = Math.ceil(total / limit) || 1;
    const currentPage = Math.min(page, totalPages);
    
    const startIndex = (currentPage - 1) * limit;
    const ownerCounts = getOwnerCounts();
    const catalogItems = filtered
        .slice(startIndex, startIndex + limit)
        .map(i => ({ ...i, sales: ownerCounts.get(Number(i.id)) || 0 }));

    const prevPage = currentPage > 1 ? currentPage - 1 : null;
    const nextPage = currentPage < totalPages ? currentPage + 1 : null;

    const pageUrl = p => {
        const params = new URLSearchParams();
        if (q) params.set('q', q);
        if (p > 1) params.set('page', p);
        const str = params.toString();
        return `/catalog${str ? '?' + str : ''}`;
    };

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
        total,
        totalPages,
        currentPage,
        from: total === 0 ? 0 : startIndex + 1,
        to: Math.min(startIndex + limit, total),
        prevPage,
        nextPage,
        pageUrl,
        paginationItems
    };
}

module.exports = { getCatalogPageData };