'use strict';

const SEP    = String.raw`[\s._\-*,'"~/\\:;#&=]`;
const SEP_NS = String.raw`[._\-*,'"~/\\:;#&=]`;
const MASK   = '[*#%]';
const VOWELS = new Set('aeiou');

const CLASSES = {
    a: 'a@4',
    b: 'b8',
    c: 'c(',
    e: 'e3',
    g: 'g9',
    i: 'i1!|y',
    l: 'l1|',
    o: 'o0',
    s: 's$5z',
    t: 't7+',
    u: 'uv'
};

const HOMOGLYPHS = {
    '\u0430': 'a', '\u0432': 'b', '\u0435': 'e', '\u043A': 'k', '\u043C': 'm', '\u043D': 'h',
    '\u043E': 'o', '\u0440': 'p', '\u0441': 'c', '\u0442': 't', '\u0443': 'y', '\u0445': 'x',
    '\u0456': 'i', '\u0455': 's', '\u0458': 'j', '\u0501': 'd', '\u04CF': 'l',
    '\u03B1': 'a', '\u03B2': 'b', '\u03B5': 'e', '\u03B9': 'i', '\u03BA': 'k', '\u03BD': 'v',
    '\u03BF': 'o', '\u03C1': 'p', '\u03C4': 't', '\u03C5': 'u', '\u03C7': 'x',
    '\u00DF': 'ss', '\u00F8': 'o', '\u0111': 'd', '\u0142': 'l', '\u0131': 'i', '\u00E6': 'ae',
    '\u20AC': 'e', '\u00A2': 'c', '\u00A1': 'i'
};

const MARKS  = /\p{M}/u;
const FORMAT = /\p{Cf}/u;
const EXTRA_INVISIBLE = /[\u034F\u115F\u1160\u17B4\u17B5\u180B-\u180F\u3164\uFFA0]/;
const CAMEL_MARK = '\u0001';

const DEFAULT_WHITELIST = [];

function escapeRe(s) {
    return s.replace(/[.*+?^${}()|[\]\\\/-]/g, '\\$&');
}

function normalize(text, camel = false) {
    const out = [];
    const starts = [];
    const ends = [];
    let i = 0;
    let prev = '';

    for (const cp of text) {
        const next = i + cp.length;

        if (camel && prev && cp !== cp.toLowerCase() && cp === cp.toUpperCase()
            && prev === prev.toLowerCase() && prev !== prev.toUpperCase()) {
            out.push(CAMEL_MARK); starts.push(i); ends.push(i);
        }

        const folded = cp.toLowerCase().normalize('NFKD');
        for (const ch of folded) {
            if (MARKS.test(ch) || FORMAT.test(ch) || EXTRA_INVISIBLE.test(ch)) continue;
            const rep = HOMOGLYPHS[ch] !== undefined ? HOMOGLYPHS[ch] : ch;
            for (let k = 0; k < rep.length; k++) {
                out.push(rep[k]); starts.push(i); ends.push(next);
            }
        }

        prev = cp;
        i = next;
    }

    return { text: out.join(''), starts, ends };
}

function buildSuffix(entry) {
    const parts = ['e?s'];
    if (entry.verb) parts.push('e?d', 'ing', 'in');
    if (entry.y) parts.push('y', 'ie');
    return `(?:${parts.join('|')})?`;
}

function buildRegex(entry) {
    const segments = entry.w.split(/\s+/).filter(Boolean);
    const compact = segments.join('');

    const classOf = (ch) => (CLASSES[ch] ? `[${CLASSES[ch]}]` : escapeRe(ch));

    const contiguousLetter = (ch) => {
        const base = classOf(ch);
        if (VOWELS.has(ch)) return `(?:${base}+|${MASK})${entry.vo ? '?' : ''}`;
        return `${base}+`;
    };
    const singleLetter = (ch) => {
        const base = classOf(ch);
        return VOWELS.has(ch) ? `(?:${base}|${MASK})` : base;
    };

    const contiguous = segments
        .map(seg => [...seg].map(contiguousLetter).join(`${SEP_NS}{0,3}`))
        .join(`${SEP}+`);

    const lookBehind = entry.sub ? '' : '(?<![a-z])';
    const lookAhead  = entry.sub ? '' : '(?![a-z])';
    const suffix     = entry.sub ? '' : buildSuffix(entry);

    let source = `${lookBehind}${contiguous}${suffix}${lookAhead}`;
    if (compact.length >= 4) {
        const spaced = segments
            .flatMap(seg => [...seg])
            .map(singleLetter)
            .join(`${SEP}{1,3}`);
        source += `|(?<![a-z])${spaced}(?![a-z])`;
    }

    return new RegExp(source, 'g');
}

function getPath(obj, pathStr) {
    return pathStr.split('.').reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

function setPath(obj, pathStr, value) {
    const keys = pathStr.split('.');
    const last = keys.pop();
    const target = keys.reduce((o, k) => (o == null ? undefined : o[k]), obj);
    if (target) target[last] = value;
}

class ProfanityFilter {
    /**
     * @param {object}  [options]
     * @param {number}  [options.minSeverity=1]
     * @param {boolean} [options.strict=false]
     * @param {string}  [options.censorStyle='stars']
     * @param {string}  [options.replacement='[censored]']
     * @param {number}  [options.maxLength=5000]
     * @param {Array}   [options.words]
     * @param {Array}   [options.allow]
     */
    constructor(options = {}) {
        const { words, allow, ...rest } = options;
        this.options = {
            minSeverity: 1,
            strict: false,
            censorStyle: 'stars',
            replacement: '[censored]',
            maxLength: 5000,
            ...rest
        };
        this.entries = new Map();
        this.whitelist = new Set(DEFAULT_WHITELIST);
        this._compiled = null;

        this.addWords(require('./profanityList'));
        if (words) this.addWords(words);
        if (allow) this.allow(allow);
    }

    addWords(list, defaultSeverity = 2) {
        for (const item of [].concat(list)) {
            let w, sev, flags;
            if (typeof item === 'string') [w, sev, flags] = [item, defaultSeverity, ''];
            else if (Array.isArray(item)) [w, sev = defaultSeverity, flags = ''] = item;
            else ({ word: w, severity: sev = defaultSeverity, flags = '' } = item);

            w = String(w).toLowerCase().trim();
            if (!w) continue;

            const key = `${w}|${flags}`;
            this.entries.set(key, {
                w, sev,
                sub: flags.includes('s'),
                vo: flags.includes('v'),
                verb: flags.includes('g'),
                y: flags.includes('y')
            });
        }
        this._compiled = null;
        return this;
    }

    removeWords(list) {
        for (const word of [].concat(list)) {
            const w = String(word).toLowerCase().trim();
            for (const [key, entry] of this.entries) {
                if (entry.w === w) this.entries.delete(key);
            }
        }
        this._compiled = null;
        return this;
    }

    allow(list) {
        for (const w of [].concat(list)) this.whitelist.add(String(w).toLowerCase().trim());
        return this;
    }

    disallow(list) {
        for (const w of [].concat(list)) this.whitelist.delete(String(w).toLowerCase().trim());
        return this;
    }

    _getCompiled() {
        if (!this._compiled) {
            this._compiled = [...this.entries.values()].map(entry => ({ entry, re: buildRegex(entry) }));
        }
        return this._compiled;
    }

    _isWhitelisted(text, start, end) {
        const isTokenChar = (c) => /[a-z0-9]/.test(c);
        let l = start;
        let r = end;
        while (l > 0 && isTokenChar(text[l - 1])) l--;
        while (r < text.length && isTokenChar(text[r])) r++;
        return this.whitelist.has(text.slice(l, r));
    }

    check(text, opts = {}) {
        const o = { ...this.options, ...opts };
        const result = { profane: false, severity: 0, matches: [], cleaned: text, truncated: false };

        if (typeof text !== 'string' || !text) return result;

        const input = text.length > o.maxLength ? text.slice(0, o.maxLength) : text;
        result.truncated = input.length < text.length;

        const compiled = this._getCompiled();
        const found = [];

        for (const camel of (o.strict ? [false, true] : [false])) {
            const norm = normalize(input, camel);

            for (const { entry, re } of compiled) {
                if (entry.sev < o.minSeverity) continue;

                for (const m of norm.text.matchAll(re)) {
                    const s = m.index;
                    const e = s + m[0].length;
                    if (e <= s) continue;
                    if (this._isWhitelisted(norm.text, s, e)) continue;

                    found.push({
                        word: entry.w,
                        severity: entry.sev,
                        start: norm.starts[s],
                        end: norm.ends[e - 1]
                    });
                }
            }
        }

        if (!found.length) return result;

        found.sort((a, b) => a.start - b.start || (b.end - b.start) - (a.end - a.start));
        const merged = [];
        for (const f of found) {
            const last = merged[merged.length - 1];
            if (last && f.start < last.end) {
                if (f.end > last.end) last.end = f.end;
                if (f.severity > last.severity) { last.severity = f.severity; last.word = f.word; }
            } else {
                merged.push({ ...f });
            }
        }

        for (const m of merged) m.text = input.slice(m.start, m.end);

        let cleaned = input;
        for (let k = merged.length - 1; k >= 0; k--) {
            const m = merged[k];
            cleaned = cleaned.slice(0, m.start) + this._mask(m.text, o) + cleaned.slice(m.end);
        }

        result.matches = merged;
        result.profane = true;
        result.severity = Math.max(...merged.map(m => m.severity));
        result.cleaned = cleaned + text.slice(input.length);
        return result;
    }

    isProfane(text, opts) {
        return this.check(text, opts).profane;
    }

    censor(text, opts) {
        return this.check(text, opts).cleaned;
    }

    _mask(str, o) {
        const chars = [...str];
        switch (o.censorStyle) {
            case 'first':   return chars[0] + '*'.repeat(chars.length - 1);
            case 'grawlix': return chars.map((_, i) => '@#$%&!'[i % 6]).join('');
            case 'fixed':   return o.replacement;
            default:        return '*'.repeat(chars.length);
        }
    }

    middleware(fields, opts = {}) {
        const {
            mode = 'reject',
            status = 400,
            message = 'Your input contains inappropriate text...',
            ...checkOpts
        } = opts;
        const list = [].concat(fields);

        return (req, res, next) => {
            for (const field of list) {
                const value = getPath(req.body, field);
                if (typeof value !== 'string') continue;

                const result = this.check(value, checkOpts);
                if (!result.profane) continue;

                if (mode === 'censor') setPath(req.body, field, result.cleaned);
                else return res.status(status).json({ error: message, field });
            }
            next();
        };
    }
}

const defaultFilter = new ProfanityFilter();
module.exports = defaultFilter;
module.exports.ProfanityFilter = ProfanityFilter;
module.exports.normalize = normalize;