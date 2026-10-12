#include "hat.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <set>

namespace
{

// Tweaks for .mesh files. If hats look twice as big / small, or the texture is
// upside down, these are the two knobs.
constexpr float V1_POSITION_SCALE = 0.5f; // version 1.xx text meshes store doubled coordinates
constexpr bool FLIP_V = false;            // Roblox v is top-down, same as raylib's texture rows

constexpr uint32_t MAX_VERTS = 2000000;

// ---------------------------------------------------------------------------
// Tiny XML reader (enough for Roblox legacy models: elements, attributes,
// text, CDATA, comments, entities)
// ---------------------------------------------------------------------------

struct Node
{
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::string text;
    std::vector<Node> children;

    const std::string *attr(const char *key) const
    {
        for (const auto &a : attrs)
            if (a.first == key)
                return &a.second;
        return nullptr;
    }

    const Node *child(const char *name) const
    {
        for (const Node &c : children)
            if (c.tag == name)
                return &c;
        return nullptr;
    }
};

std::string trim(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a]))
        a++;
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}

std::string decodeEntities(const std::string &in)
{
    if (in.find('&') == std::string::npos)
        return in;

    std::string o;
    o.reserve(in.size());
    for (size_t k = 0; k < in.size(); k++)
    {
        if (in[k] != '&')
        {
            o += in[k];
            continue;
        }
        size_t semi = in.find(';', k);
        if (semi == std::string::npos || semi - k > 8)
        {
            o += in[k];
            continue;
        }
        std::string ent = in.substr(k + 1, semi - k - 1);
        if (ent == "amp")
            o += '&';
        else if (ent == "lt")
            o += '<';
        else if (ent == "gt")
            o += '>';
        else if (ent == "quot")
            o += '"';
        else if (ent == "apos")
            o += '\'';
        else if (ent.size() > 1 && ent[0] == '#')
        {
            long code = (ent[1] == 'x' || ent[1] == 'X') ? std::strtol(ent.c_str() + 2, nullptr, 16)
                                                         : std::strtol(ent.c_str() + 1, nullptr, 10);
            o += (code > 0 && code < 128) ? (char)code : '?';
        }
        else
        {
            o += in[k];
            continue;
        }
        k = semi;
    }
    return o;
}

struct Parser
{
    const std::string &s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string &src) : s(src) {}

    bool startsWith(const char *lit) const { return s.compare(i, std::strlen(lit), lit) == 0; }

    void skipWs()
    {
        while (i < s.size() && std::isspace((unsigned char)s[i]))
            i++;
    }

    bool skipPast(const char *lit)
    {
        size_t p = s.find(lit, i);
        if (p == std::string::npos)
        {
            err = std::string("unterminated section, expected ") + lit;
            return false;
        }
        i = p + std::strlen(lit);
        return true;
    }

    bool parseDocument(Node &root)
    {
        if (s.compare(0, 3, "\xEF\xBB\xBF") == 0)
            i = 3;
        for (;;)
        {
            skipWs();
            if (i >= s.size())
            {
                err = "no root element";
                return false;
            }
            if (startsWith("<?"))
            {
                if (!skipPast("?>"))
                    return false;
            }
            else if (startsWith("<!--"))
            {
                if (!skipPast("-->"))
                    return false;
            }
            else if (startsWith("<!"))
            {
                if (!skipPast(">"))
                    return false;
            }
            else
                break;
        }
        if (s[i] != '<')
        {
            err = "not an XML document";
            return false;
        }
        return parseElement(root, 0);
    }

    bool parseElement(Node &out, int depth)
    {
        if (depth > 64)
        {
            err = "XML is nested too deeply";
            return false;
        }

        i++; // '<'
        size_t start = i;
        while (i < s.size() && !std::isspace((unsigned char)s[i]) && s[i] != '>' && s[i] != '/')
            i++;
        out.tag = s.substr(start, i - start);
        if (out.tag.empty())
        {
            err = "empty tag name";
            return false;
        }

        // attributes, then '>' or '/>'
        for (;;)
        {
            skipWs();
            if (i >= s.size())
            {
                err = "unexpected end of file inside <" + out.tag + ">";
                return false;
            }
            if (s[i] == '/')
            {
                if (i + 1 < s.size() && s[i + 1] == '>')
                {
                    i += 2;
                    return true;
                }
                err = "stray '/' inside <" + out.tag + ">";
                return false;
            }
            if (s[i] == '>')
            {
                i++;
                break;
            }

            size_t ks = i;
            while (i < s.size() && s[i] != '=' && s[i] != '>' && s[i] != '/' && !std::isspace((unsigned char)s[i]))
                i++;
            std::string key = s.substr(ks, i - ks);
            skipWs();
            if (i >= s.size() || s[i] != '=')
            {
                err = "attribute '" + key + "' has no value in <" + out.tag + ">";
                return false;
            }
            i++;
            skipWs();
            if (i >= s.size() || (s[i] != '"' && s[i] != '\''))
            {
                err = "attribute '" + key + "' is not quoted in <" + out.tag + ">";
                return false;
            }
            char q = s[i++];
            size_t ve = s.find(q, i);
            if (ve == std::string::npos)
            {
                err = "unterminated attribute value in <" + out.tag + ">";
                return false;
            }
            out.attrs.emplace_back(key, decodeEntities(s.substr(i, ve - i)));
            i = ve + 1;
        }

        // content
        for (;;)
        {
            if (i >= s.size())
            {
                err = "unexpected end of file, <" + out.tag + "> is never closed";
                return false;
            }
            if (s[i] != '<')
            {
                size_t e = s.find('<', i);
                if (e == std::string::npos)
                    e = s.size();
                out.text += decodeEntities(s.substr(i, e - i));
                i = e;
                continue;
            }
            if (startsWith("<!--"))
            {
                if (!skipPast("-->"))
                    return false;
                continue;
            }
            if (startsWith("<![CDATA["))
            {
                size_t cs = i + 9;
                size_t ce = s.find("]]>", cs);
                if (ce == std::string::npos)
                {
                    err = "unterminated CDATA section";
                    return false;
                }
                out.text += s.substr(cs, ce - cs);
                i = ce + 3;
                continue;
            }
            if (startsWith("</"))
            {
                size_t ce = s.find('>', i);
                if (ce == std::string::npos)
                {
                    err = "unterminated closing tag";
                    return false;
                }
                std::string closing = trim(s.substr(i + 2, ce - i - 2));
                if (closing != out.tag)
                {
                    err = "found </" + closing + "> but expected </" + out.tag + ">";
                    return false;
                }
                i = ce + 1;
                return true;
            }
            out.children.emplace_back();
            if (!parseElement(out.children.back(), depth + 1))
                return false;
        }
    }
};

// ---------------------------------------------------------------------------
// Reading Roblox properties out of the tree
// ---------------------------------------------------------------------------

const Node *findItem(const Node &n, std::initializer_list<const char *> classes)
{
    if (n.tag == "Item")
    {
        const std::string *c = n.attr("class");
        if (c)
            for (const char *k : classes)
                if (*c == k)
                    return &n;
    }
    for (const Node &ch : n.children)
        if (const Node *f = findItem(ch, classes))
            return f;
    return nullptr;
}

const Node *prop(const Node &item, const char *name)
{
    const Node *props = item.child("Properties");
    if (!props)
        return nullptr;
    for (const Node &p : props->children)
    {
        const std::string *n = p.attr("name");
        if (n && *n == name)
            return &p;
    }
    return nullptr;
}

bool toFloat(const Node *n, float &out)
{
    if (!n)
        return false;
    std::string t = trim(n->text);
    if (t.empty())
        return false;
    char *end = nullptr;
    float v = std::strtof(t.c_str(), &end);
    if (*end != '\0' || !std::isfinite(v))
        return false;
    out = v;
    return true;
}

// Absent property = keep the default (true). Present but broken = false.
bool readVec3(const Node *p, std::array<float, 3> &v)
{
    if (!p)
        return true;
    return toFloat(p->child("X"), v[0]) && toFloat(p->child("Y"), v[1]) && toFloat(p->child("Z"), v[2]);
}

bool readCFrame(const Node *p, std::array<float, 3> &pos, std::array<float, 9> &rot)
{
    if (!p)
        return true;
    static const char *RK[9] = {"R00", "R01", "R02", "R10", "R11", "R12", "R20", "R21", "R22"};
    if (!toFloat(p->child("X"), pos[0]) || !toFloat(p->child("Y"), pos[1]) || !toFloat(p->child("Z"), pos[2]))
        return false;
    for (int k = 0; k < 9; k++)
        if (!toFloat(p->child(RK[k]), rot[k]))
            return false;
    return true;
}

// <Content name="MeshId"><url>...</url></Content>   (empty / <null/> -> "")
std::string contentUrl(const Node *p)
{
    if (!p)
        return "";
    const Node *u = p->child("url");
    return u ? trim(u->text) : "";
}

void collectUrls(const Node &n, std::vector<std::string> &out)
{
    if (n.tag == "url")
    {
        std::string u = trim(n.text);
        if (!u.empty())
            out.push_back(u);
    }
    for (const Node &c : n.children)
        collectUrls(c, out);
}

// A few common BrickColors, only used for hats that have no texture.
unsigned brickColorRgb(int id)
{
    switch (id)
    {
    case 1: return 0xF2F3F3;    // White
    case 5: return 0xD7C59A;    // Brick yellow
    case 18: return 0xCC8E69;   // Nougat
    case 21: return 0xC4281C;   // Bright red
    case 23: return 0x0D69AC;   // Bright blue
    case 24: return 0xF5CD30;   // Bright yellow
    case 26: return 0x1B2A35;   // Black
    case 37: return 0x4B974B;   // Bright green
    case 38: return 0xA05F35;   // Dark orange
    case 102: return 0x6E99CA;  // Medium blue
    case 106: return 0xDA8541;  // Bright orange
    case 192: return 0x694028;  // Reddish brown
    case 194: return 0xA3A2A5;  // Medium stone grey
    case 199: return 0x635F62;  // Dark stone grey
    case 217: return 0x7C5C46;  // Brown
    case 1003: return 0x111111; // Really black
    default: return 0xA3A2A5;
    }
}

void readHandleColor(const Node &hat, unsigned &color)
{
    const Node *handle = nullptr;
    // prefer the part called Handle, else the first part
    std::vector<const Node *> stack = {&hat};
    while (!stack.empty())
    {
        const Node *n = stack.back();
        stack.pop_back();
        if (n->tag == "Item")
        {
            const std::string *c = n->attr("class");
            if (c && (*c == "Part" || *c == "MeshPart"))
            {
                const Node *nm = prop(*n, "Name");
                if (!handle)
                    handle = n;
                if (nm && trim(nm->text) == "Handle")
                {
                    handle = n;
                    break;
                }
            }
        }
        for (const Node &ch : n->children)
            stack.push_back(&ch);
    }
    if (!handle)
        return;

    if (const Node *c3 = prop(*handle, "Color3uint8"))
    {
        std::string t = trim(c3->text);
        if (!t.empty())
        {
            color = (unsigned)(std::strtoul(t.c_str(), nullptr, 10) & 0xFFFFFF);
            return;
        }
    }
    if (const Node *bc = prop(*handle, "BrickColor"))
    {
        std::string t = trim(bc->text);
        if (!t.empty())
            color = brickColorRgb(std::atoi(t.c_str()));
    }
}

// ---------------------------------------------------------------------------
// URL -> local file
// ---------------------------------------------------------------------------

std::string lowered(std::string s)
{
    for (char &c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string stripTrailingSlashes(std::string s)
{
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

// "http://reboot16.xyz/api/get-mesh?id=5"  ->  rest = "/api/get-mesh?id=5"
// `configured` is a bare host (http and https both accepted) or a full URL.
bool urlOnSite(const std::string &url, const std::string &configured, std::string &rest)
{
    const std::string base = stripTrailingSlashes(configured);
    std::vector<std::string> candidates;
    if (base.find("://") != std::string::npos)
        candidates.push_back(base);
    else
    {
        candidates.push_back("http://" + base);
        candidates.push_back("https://" + base);
    }

    const std::string lowUrl = lowered(url);
    for (const std::string &c : candidates)
    {
        if (lowUrl.compare(0, c.size(), lowered(c)) != 0)
            continue;
        if (url.size() == c.size())
        {
            rest.clear();
            return true;
        }
        char next = url[c.size()];
        if (next == '/' || next == '?' || next == '#')
        {
            rest = url.substr(c.size());
            return true;
        }
    }
    return false;
}

// "/api/get-mesh?id=5" with endpoint "/api/get-mesh"  ->  id "5"
bool parseAssetRest(std::string rest, const char *endpoint, std::string &id)
{
    size_t hash = rest.find('#');
    if (hash != std::string::npos)
        rest.erase(hash);

    size_t q = rest.find('?');
    std::string path = rest.substr(0, q);
    std::string query = q == std::string::npos ? "" : rest.substr(q + 1);
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();
    if (path != endpoint)
        return false;

    size_t pos = 0;
    while (pos <= query.size())
    {
        size_t amp = query.find('&', pos);
        std::string pair = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        if (pair.compare(0, 3, "id=") == 0)
        {
            std::string v = pair.substr(3);
            if (v.empty() || v.size() > 12)
                return false;
            for (unsigned char c : v)
                if (!std::isdigit(c))
                    return false;
            id = v;
            return true;
        }
        if (amp == std::string::npos)
            break;
        pos = amp + 1;
    }
    return false;
}

// ---------------------------------------------------------------------------
// .mesh readers
// ---------------------------------------------------------------------------

bool allFinite(const std::vector<float> &v)
{
    for (float f : v)
        if (!std::isfinite(f))
            return false;
    return true;
}

bool parseMeshText(const std::string &data, size_t pos, MeshData &out, std::string &err)
{
    // line 2 is the face count; the vertex data follows as [x,y,z][nx,ny,nz][u,v,w] groups
    size_t eol = data.find('\n', pos);
    size_t i = eol == std::string::npos ? data.size() : eol + 1;

    std::vector<float> nums;
    while (i < data.size())
    {
        size_t open = data.find('[', i);
        if (open == std::string::npos)
            break;
        size_t close = data.find(']', open);
        if (close == std::string::npos)
        {
            err = "unterminated [ ] group in mesh";
            return false;
        }
        const char *c = data.c_str() + open + 1;
        for (int k = 0; k < 3; k++)
        {
            char *end = nullptr;
            float v = std::strtof(c, &end);
            if (end == c)
            {
                err = "bad number in mesh";
                return false;
            }
            nums.push_back(v);
            c = end;
            while (*c == ',' || std::isspace((unsigned char)*c))
                c++;
        }
        i = close + 1;
    }

    if (nums.empty() || nums.size() % 9 != 0)
    {
        err = "mesh vertex data is malformed";
        return false;
    }
    size_t verts = nums.size() / 9;
    if (verts % 3 != 0 || verts > MAX_VERTS)
    {
        err = "mesh has an invalid vertex count";
        return false;
    }

    for (size_t v = 0; v < verts; v++)
    {
        const float *g = &nums[v * 9];
        out.pos.push_back(g[0] * V1_POSITION_SCALE);
        out.pos.push_back(g[1] * V1_POSITION_SCALE);
        out.pos.push_back(g[2] * V1_POSITION_SCALE);
        out.nrm.push_back(g[3]);
        out.nrm.push_back(g[4]);
        out.nrm.push_back(g[5]);
        out.uv.push_back(g[6]);
        out.uv.push_back(FLIP_V ? 1.0f - g[7] : g[7]);
    }
    return true;
}

bool parseMeshBinary(const std::string &data, size_t pos, int major, MeshData &out, std::string &err)
{
    const unsigned char *p = (const unsigned char *)data.data();
    const size_t n = data.size();

    auto u16 = [&](size_t o) { uint16_t v; std::memcpy(&v, p + o, 2); return v; };
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, p + o, 4); return v; };
    auto f32 = [&](size_t o) { float v; std::memcpy(&v, p + o, 4); return v; };

    if (pos + 12 > n)
    {
        err = "mesh file is truncated";
        return false;
    }

    uint16_t headerSize = u16(pos);
    unsigned vertSize = p[pos + 2];
    unsigned faceSize = p[pos + 3];
    uint32_t numVerts, numFaces, numLods = 0;

    if (major == 2)
    {
        numVerts = u32(pos + 4);
        numFaces = u32(pos + 8);
    }
    else
    {
        if (headerSize < 16 || pos + 16 > n)
        {
            err = "mesh header is truncated";
            return false;
        }
        numLods = u16(pos + 6);
        numVerts = u32(pos + 8);
        numFaces = u32(pos + 12);
    }

    if (headerSize < 12 || pos + headerSize > n || vertSize < 32 || faceSize < 12)
    {
        err = "mesh header is invalid";
        return false;
    }
    if (numVerts == 0 || numVerts > MAX_VERTS || numFaces == 0 || numFaces > MAX_VERTS)
    {
        err = "mesh has an invalid vertex/face count";
        return false;
    }

    size_t vOff = pos + headerSize;
    size_t fOff = vOff + (size_t)numVerts * vertSize;
    size_t fEnd = fOff + (size_t)numFaces * faceSize;
    if (fEnd > n)
    {
        err = "mesh file is truncated";
        return false;
    }

    // version 3 stores level-of-detail ranges after the faces; draw LOD 0 only
    uint32_t faceStart = 0, faceStop = numFaces;
    if (major == 3 && numLods >= 2 && fEnd + (size_t)numLods * 4 <= n)
    {
        uint32_t a = u32(fEnd), b = u32(fEnd + 4);
        if (a < b && b <= numFaces)
        {
            faceStart = a;
            faceStop = b;
        }
    }

    for (uint32_t f = faceStart; f < faceStop; f++)
    {
        for (int k = 0; k < 3; k++)
        {
            uint32_t idx = u32(fOff + (size_t)f * faceSize + k * 4);
            if (idx >= numVerts)
            {
                err = "mesh face points at a missing vertex";
                return false;
            }
            size_t o = vOff + (size_t)idx * vertSize;
            out.pos.push_back(f32(o + 0));
            out.pos.push_back(f32(o + 4));
            out.pos.push_back(f32(o + 8));
            out.nrm.push_back(f32(o + 12));
            out.nrm.push_back(f32(o + 16));
            out.nrm.push_back(f32(o + 20));
            float v = f32(o + 28);
            out.uv.push_back(f32(o + 24));
            out.uv.push_back(FLIP_V ? 1.0f - v : v);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// .obj reader (v / vt / vn / f, any polygon, negative indices, missing vt/vn)
// ---------------------------------------------------------------------------

// OBJ's v runs bottom-up, raylib's texture rows run top-down (raylib's own OBJ
// loader flips it too).
constexpr bool OBJ_FLIP_V = true;

struct ObjCorner
{
    long v = 0, t = 0, n = 0; // 1-based, negative = relative, 0 = absent
};

bool readFloats(const char *&s, int count, float *out)
{
    for (int k = 0; k < count; k++)
    {
        char *end = nullptr;
        out[k] = std::strtof(s, &end);
        if (end == s)
            return false;
        s = end;
    }
    return true;
}

// "12", "12/5", "12//3" or "12/5/3"
bool readCorner(const char *&s, ObjCorner &c)
{
    char *end = nullptr;
    c.v = std::strtol(s, &end, 10);
    if (end == s || c.v == 0)
        return false;
    s = end;
    c.t = c.n = 0;
    if (*s == '/')
    {
        s++;
        if (*s != '/')
        {
            c.t = std::strtol(s, &end, 10);
            if (end == s)
                return false;
            s = end;
        }
        if (*s == '/')
        {
            s++;
            c.n = std::strtol(s, &end, 10);
            if (end == s)
                return false;
            s = end;
        }
    }
    return true;
}

bool parseObj(const std::string &data, MeshData &out, std::string &err)
{
    std::vector<float> P, T, N; // positions, uvs, normals as read
    std::vector<ObjCorner> face;

    auto resolve = [](long idx, size_t count, size_t &outIdx)
    {
        long r = idx > 0 ? idx - 1 : (long)count + idx;
        if (r < 0 || (size_t)r >= count)
            return false;
        outIdx = (size_t)r;
        return true;
    };

    size_t pos = 0;
    while (pos < data.size())
    {
        size_t eol = data.find('\n', pos);
        if (eol == std::string::npos)
            eol = data.size();
        std::string line = data.substr(pos, eol - pos); // own copy, so number parsing can't run into the next line
        pos = eol + 1;

        const char *s = line.c_str();
        while (*s == ' ' || *s == '\t')
            s++;
        const bool ws2 = s[0] != '\0' && s[1] != '\0' && (s[2] == ' ' || s[2] == '\t');

        if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t'))
        {
            s += 2;
            float v[3];
            if (!readFloats(s, 3, v))
            {
                err = "bad vertex line in .obj";
                return false;
            }
            P.insert(P.end(), v, v + 3);
        }
        else if (s[0] == 'v' && s[1] == 't' && ws2)
        {
            s += 3;
            float v[2];
            if (!readFloats(s, 2, v))
            {
                err = "bad texture coordinate line in .obj";
                return false;
            }
            T.insert(T.end(), v, v + 2);
        }
        else if (s[0] == 'v' && s[1] == 'n' && ws2)
        {
            s += 3;
            float v[3];
            if (!readFloats(s, 3, v))
            {
                err = "bad normal line in .obj";
                return false;
            }
            N.insert(N.end(), v, v + 3);
        }
        else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t'))
        {
            s += 2;
            face.clear();
            for (;;)
            {
                while (*s == ' ' || *s == '\t' || *s == '\r')
                    s++;
                if (*s == '\0')
                    break;
                ObjCorner c;
                if (!readCorner(s, c))
                {
                    err = "bad face line in .obj";
                    return false;
                }
                face.push_back(c);
            }
            if (face.size() < 3)
            {
                err = "face with fewer than 3 vertices in .obj";
                return false;
            }

            // fan-triangulate: (0, k, k+1)
            for (size_t k = 1; k + 1 < face.size(); k++)
            {
                const ObjCorner *tri[3] = {&face[0], &face[k], &face[k + 1]};
                size_t pi[3];
                for (int j = 0; j < 3; j++)
                    if (!resolve(tri[j]->v, P.size() / 3, pi[j]))
                    {
                        err = ".obj face uses a vertex that doesn't exist";
                        return false;
                    }

                // flat normal, for corners that don't carry one
                const float *a = &P[pi[0] * 3], *b = &P[pi[1] * 3], *c = &P[pi[2] * 3];
                float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                float fn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
                float len = std::sqrt(fn[0] * fn[0] + fn[1] * fn[1] + fn[2] * fn[2]);
                if (len > 0.0f)
                    for (float &x : fn)
                        x /= len;

                for (int j = 0; j < 3; j++)
                {
                    out.pos.insert(out.pos.end(), &P[pi[j] * 3], &P[pi[j] * 3] + 3);

                    size_t ni;
                    if (tri[j]->n != 0 && resolve(tri[j]->n, N.size() / 3, ni))
                        out.nrm.insert(out.nrm.end(), &N[ni * 3], &N[ni * 3] + 3);
                    else
                        out.nrm.insert(out.nrm.end(), fn, fn + 3);

                    size_t ti;
                    if (tri[j]->t != 0 && resolve(tri[j]->t, T.size() / 2, ti))
                    {
                        out.uv.push_back(T[ti * 2]);
                        out.uv.push_back(OBJ_FLIP_V ? 1.0f - T[ti * 2 + 1] : T[ti * 2 + 1]);
                    }
                    else
                    {
                        out.uv.push_back(0.0f);
                        out.uv.push_back(0.0f);
                    }
                }
                if (out.pos.size() / 3 > MAX_VERTS)
                {
                    err = ".obj has too many triangles";
                    return false;
                }
            }
        }
        // everything else (o, g, s, usemtl, mtllib, comments) is ignored
    }

    if (out.pos.empty())
    {
        err = ".obj contains no faces";
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool parseHatXml(const std::string &xml, const std::string &publicUrl, HatModel &out, std::vector<std::string> &problems)
{
    problems.clear();
    out = HatModel{};

    const std::string base = stripTrailingSlashes(publicUrl);
    if (base.empty())
    {
        problems.push_back("hats are disabled: no hat URL is configured (set --hat-url)");
        return false;
    }

    if (xml.compare(0, 8, "<roblox!") == 0)
    {
        problems.push_back("this is a binary .rbxm; save the hat as a legacy XML model (.rbxmx / XML .rbxm)");
        return false;
    }

    Node root;
    Parser parser(xml);
    if (!parser.parseDocument(root))
    {
        problems.push_back("could not read the model XML: " + parser.err);
        return false;
    }
    if (root.tag != "roblox")
    {
        problems.push_back("not a Roblox model (root element is <" + root.tag + ">)");
        return false;
    }

    const Node *hat = findItem(root, {"Hat", "Accessory", "Accoutrement"});
    if (!hat)
    {
        problems.push_back("no Hat item found in the model");
        return false;
    }

    if (const Node *nm = prop(*hat, "Name"))
        out.name = trim(nm->text);
    if (out.name.empty())
        out.name = "Hat";

    if (!readCFrame(prop(*hat, "AttachmentPoint"), out.attachPos, out.attachRot))
        problems.push_back("the hat's AttachmentPoint is malformed");

    const Node *mesh = findItem(*hat, {"SpecialMesh", "FileMesh"});
    if (!mesh)
    {
        problems.push_back("the hat has no SpecialMesh");
    }
    else
    {
        out.meshUrl = contentUrl(prop(*mesh, "MeshId"));
        out.textureUrl = contentUrl(prop(*mesh, "TextureId"));
        if (!readVec3(prop(*mesh, "Scale"), out.scale))
            problems.push_back("the mesh Scale is malformed");
        if (!readVec3(prop(*mesh, "Offset"), out.offset))
            problems.push_back("the mesh Offset is malformed");
        if (out.meshUrl.empty())
            problems.push_back("the mesh has no MeshId");
    }

    readHandleColor(*hat, out.color);

    // 1) every url in the file must be on the configured site
    std::vector<std::string> urls;
    collectUrls(root, urls);
    std::set<std::string> seen;
    for (const std::string &u : urls)
    {
        std::string rest;
        if (seen.insert(u).second && !urlOnSite(u, base, rest))
            problems.push_back("URL is not on " + base + " : " + u);
    }

    // 2) MeshId / TextureId must use the asset endpoints; that is also where the ids come from
    auto idOf = [&](const std::string &url, const char *what, const char *endpoint, std::string &idOut)
    {
        if (url.empty())
            return;
        std::string rest;
        if (!urlOnSite(url, base, rest))
            return; // already reported above
        if (!parseAssetRest(rest, endpoint, idOut))
            problems.push_back(std::string(what) + " URL must look like " + base + endpoint + "?id=<number> : " + url);
    };
    idOf(out.meshUrl, "mesh", HAT_MESH_ENDPOINT, out.meshId);
    idOf(out.textureUrl, "texture", HAT_TEXTURE_ENDPOINT, out.textureId);

    return problems.empty();
}

std::string imageExtension(const std::string &b)
{
    if (b.size() >= 8 && b.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0)
        return ".png";
    if (b.size() >= 3 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xD8 && (unsigned char)b[2] == 0xFF)
        return ".jpg";
    if (b.size() >= 2 && b[0] == 'B' && b[1] == 'M')
        return ".bmp";
    return "";
}

namespace
{

bool parseRobloxMesh(const std::string &data, MeshData &out, std::string &err)
{
    size_t eol = data.find('\n');
    if (eol == std::string::npos)
    {
        err = "mesh file is truncated";
        return false;
    }
    std::string version = trim(data.substr(8, eol - 8));

    if (version.compare(0, 2, "1.") == 0)
        return parseMeshText(data, eol + 1, out, err);
    if (version.compare(0, 2, "2.") == 0)
        return parseMeshBinary(data, eol + 1, 2, out, err);
    if (version.compare(0, 2, "3.") == 0)
        return parseMeshBinary(data, eol + 1, 3, out, err);

    err = "unsupported mesh version " + version;
    return false;
}

} // namespace

bool loadHatMeshData(const std::string &data, MeshData &out, std::string &err)
{
    out = MeshData{};

    if (data.empty())
    {
        err = "mesh is empty";
        return false;
    }

    bool ok = data.compare(0, 8, "version ") == 0 ? parseRobloxMesh(data, out, err) : parseObj(data, out, err);
    if (!ok)
        return false;

    if (out.pos.empty() || !allFinite(out.pos) || !allFinite(out.nrm) || !allFinite(out.uv))
    {
        err = "mesh contains no triangles or invalid numbers";
        return false;
    }
    return true;
}
