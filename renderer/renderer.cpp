#include "renderer.hpp"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

const float FOV_DEG = 60.0f;
const float NEAR_Z = 1.25f;
const float FAR_Z = 5000.0f;
const float START_DIST = 10.0f;
const float EXPAND = 0.1f;

const float DIST_MIN = 0.5f;
const float DIST_MAX = 2000.0f;

const Vector3 MODEL_LOOK = {0.0f, 0.0f, -1.0f};

const Vector3 AMBIENT_COLOR = {0.50f, 0.50f, 0.50f};
const Vector3 LAMP0_COLOR = {0.50f, 0.50f, 0.50f};
const Vector3 LAMP0_DIR = {-0.35f, -0.75f, 0.55f};
const Vector3 LAMP1_COLOR = {0.25f, 0.25f, 0.25f};
const Vector3 FOG_COLOR = {0.75f, 0.75f, 0.75f};
const float FOG_START = 0.0f;
const float FOG_END = 1000000.0f;

const float SPECULAR = 0.4f;
const float GLOSS = 9.0f;

constexpr int PART_COUNT = RenderRequest::PART_COUNT;

// Indices into Impl::parts, matching RenderRequest::colors / PART_KEYS.
enum PartId
{
    P_HEAD = 0,
    P_TORSO,
    P_LARM,
    P_RARM,
    P_LLEG,
    P_RLEG
};

struct Part
{
    const char *file;
    Vector3 pos;
    Vector3 scale;
    Model model;
    // What each material looked like straight after loading, so a render can
    // swap textures in and then put everything back.
    std::vector<Texture2D> origTex;
    std::vector<Color> origColor;
};

const char *VS_SRC =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "in vec2 vertexTexCoord;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matModel;\n"
    "uniform vec3 CameraPosition;\n"
    "uniform vec3 Lamp0Dir;\n"
    "uniform vec3 Lamp0Color;\n"
    "uniform vec3 Lamp1Color;\n"
    "uniform vec2 SpecParams;\n"
    "out vec3 vNormal;\n"
    "out vec3 vView;\n"
    "out float vDepth;\n"
    "out vec3 vDiffuse;\n"
    "out float vSpec;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "    vec3 n = vertexNormal;\n"
    "    if (dot(n, n) < 1e-6) n = vec3(0.0, 1.0, 0.0);\n"
    "    vec3 posWorld = (matModel * vec4(vertexPosition, 1.0)).xyz;\n"
    "    vec3 normalWorld = normalize(mat3(matModel) * n);\n"
    "    vec4 hpos = mvp * vec4(vertexPosition, 1.0);\n"
    "    gl_Position = hpos;\n"
    "    vNormal = normalWorld;\n"
    "    vView = CameraPosition - posWorld;\n"
    "    vDepth = hpos.w;\n"
    "    vTexCoord = vertexTexCoord;\n"
    "    float ndotl = dot(normalWorld, -Lamp0Dir);\n"
    "    float ndoth = dot(normalize(-Lamp0Dir + normalize(vView)), normalWorld);\n"
    "    float ltX = max(ndotl, 0.0);\n"
    "    float ltY = (ndotl < 0.0 || ndoth < 0.0) ? 0.0 : pow(ndoth, SpecParams.y);\n"
    "    vDiffuse = ltX * Lamp0Color + max(-ndotl, 0.0) * Lamp1Color;\n"
    "    vSpec = ltY * SpecParams.x;\n"
    "}\n";

const char *FS_SRC =
    "#version 330\n"
    "in vec3 vNormal;\n"
    "in vec3 vView;\n"
    "in float vDepth;\n"
    "in vec3 vDiffuse;\n"
    "in float vSpec;\n"
    "in vec2 vTexCoord;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform sampler2D texture0;\n"
    "uniform int useTexture;\n"
    "uniform vec3 AmbientColor;\n"
    "uniform vec3 Lamp0Dir;\n"
    "uniform vec3 Lamp0Color;\n"
    "uniform vec3 Lamp1Color;\n"
    "uniform vec3 FogColor;\n"
    "uniform vec4 FogParams;\n"
    "uniform vec2 SpecParams;\n"
    "uniform int HQ;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 albedo = colDiffuse;\n"
    "    if (useTexture != 0) {\n"
    "        vec4 texColor = texture(texture0, vTexCoord);\n"
    "        albedo = vec4(mix(colDiffuse.rgb, texColor.rgb, texColor.a), colDiffuse.a);\n"
    "    }\n"
    "    float shadow = 1.0;\n"
    "    vec3 lightGrid = vec3(0.0);\n"
    "    vec3 diffuseIntensity;\n"
    "    vec3 specular;\n"
    "    if (HQ != 0) {\n"
    "        vec3 normal = normalize(vNormal);\n"
    "        float ndotl = dot(normal, -Lamp0Dir);\n"
    "        diffuseIntensity = clamp(ndotl, 0.0, 1.0) * Lamp0Color + max(-ndotl, 0.0) * Lamp1Color;\n"
    "        float specularIntensity = step(0.0, ndotl) * SpecParams.x;\n"
    "        float h = clamp(dot(normal, normalize(-Lamp0Dir + normalize(vView))), 0.0, 1.0);\n"
    "        specular = Lamp0Color * (specularIntensity * shadow * pow(h, SpecParams.y));\n"
    "    } else {\n"
    "        diffuseIntensity = vDiffuse;\n"
    "        specular = Lamp0Color * (vSpec * shadow);\n"
    "    }\n"
    "    vec3 diffuse = (AmbientColor + diffuseIntensity * shadow + lightGrid) * albedo.rgb;\n"
    "    vec3 color = diffuse + specular;\n"
    "    float fogAlpha = clamp((FogParams.z - vDepth) * FogParams.w, 0.0, 1.0);\n"
    "    color = mix(FogColor, color, fogAlpha);\n"
    "    finalColor = vec4(color, albedo.a);\n"
    "}\n";

struct Mat3
{
    float m[3][3];
}; // m[row][col]

Mat3 mul(const Mat3 &a, const Mat3 &b)
{
    Mat3 r = {};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

Mat3 rotX(float a)
{
    float c = cosf(a), s = sinf(a);
    return {{{1, 0, 0}, {0, c, -s}, {0, s, c}}};
}

Mat3 rotZ(float a)
{
    float c = cosf(a), s = sinf(a);
    return {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
}

Vector3 column(const Mat3 &a, int c)
{
    return {a.m[0][c], a.m[1][c], a.m[2][c]};
}

Mat3 lookAtRotation(Vector3 forward)
{
    Vector3 up = {0, 1, 0};
    Vector3 f = Vector3Normalize(forward);
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, up));
    Vector3 u = Vector3CrossProduct(r, f);
    return {{{r.x, u.x, -f.x}, {r.y, u.y, -f.y}, {r.z, u.z, -f.z}}};
}

Vector3 thumbnailDirection(Vector3 look)
{
    bool noTilt = fabsf(look.y) > 0.95f;
    if (noTilt)
    {
        look = {0, 0, -1};
    }
    else
    {
        look.y = 0;
        look = Vector3Normalize(look);
    }

    Mat3 rotation = lookAtRotation(look);

    Mat3 euler = mul(rotZ(45.0f * DEG2RAD), rotX(35.0f * DEG2RAD));
    rotation = mul(rotation, euler);

    return Vector3Negate(column(rotation, 2));
}

bool boxInFrustum(const BoundingBox &b, Vector3 camPos, Vector3 target)
{
    Vector3 up = {0, 1, 0};
    Vector3 f = Vector3Normalize(Vector3Subtract(target, camPos));
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, up));
    Vector3 u = Vector3CrossProduct(r, f);

    float tanHalf = tanf(FOV_DEG * DEG2RAD * 0.5f);
    float aspect = 1.0f;

    for (int i = 0; i < 8; i++)
    {
        Vector3 corner = {
            (i & 1) ? b.max.x : b.min.x,
            (i & 2) ? b.max.y : b.min.y,
            (i & 4) ? b.max.z : b.min.z};

        Vector3 d = Vector3Subtract(corner, camPos);
        float z = Vector3DotProduct(d, f);
        float x = Vector3DotProduct(d, r);
        float y = Vector3DotProduct(d, u);

        if (z < NEAR_Z || z > FAR_Z)
            return false;
        if (fabsf(y) > z * tanHalf)
            return false;
        if (fabsf(x) > z * tanHalf * aspect)
            return false;
    }
    return true;
}

float zoomExtentsDistance(const BoundingBox &extents, Vector3 focus, Vector3 dir)
{
    float low = DIST_MIN, current = START_DIST, high = DIST_MAX;
    float used = current;

    while (high - low >= 0.1f)
    {
        used = current;
        Vector3 camPos = Vector3Add(focus, Vector3Scale(dir, used));

        if (boxInFrustum(extents, camPos, focus))
        {
            float next = (low + current) * 0.5f;
            high = current;
            current = next;
        }
        else
        {
            float next = (current + high) * 0.5f;
            low = current;
            current = next;
        }
    }
    return used;
}

std::string base64Encode(const unsigned char *data, size_t len)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 2 < len; i += 3)
    {
        unsigned v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];
        out += T[v & 63];
    }
    if (i + 1 == len)
    {
        unsigned v = data[i] << 16;
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += "==";
    }
    else if (i + 2 == len)
    {
        unsigned v = (data[i] << 16) | (data[i + 1] << 8);
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

Color colorFromRgb(unsigned rgb)
{
    return {(unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8), (unsigned char)rgb, 255};
}

} // namespace

// ---------------------------------------------------------------------------

struct AvatarRenderer::Impl
{
    bool windowOpen = false;
    std::string dir;
    Shader shader = {};
    Vector3 camPos = {};
    Vector3 focus = {};
    int useTexLoc = -1;
    int hqLoc = -1;

    Part parts[PART_COUNT] = {
        {"head.obj", {0.0f, 1.5f, 0.0f}, {1, 1, 1}, {}, {}, {}},
        {"torso.obj", {0.0f, 0.0f, 0.0f}, {1, 1, 1}, {}, {}, {}},
        {"larm.obj", {-1.5f, 0.0f, 0.0f}, {1, 1, 1}, {}, {}, {}},
        {"rarm.obj", {1.5f, 0.0f, 0.0f}, {1, 1, 1}, {}, {}, {}},
        {"lleg.obj", {-0.5f, -2.0f, 0.0f}, {1, 1, 1}, {}, {}, {}},
        {"rleg.obj", {0.5f, -2.0f, 0.0f}, {1, 1, 1}, {}, {}, {}},
    };

    ~Impl() { shutdown(); }

    void shutdown()
    {
        if (!windowOpen)
            return;

        for (Part &p : parts)
        {
            if (p.model.meshCount == 0)
                continue;
            // Some raylib versions unload a material's shader inside
            // UnloadModel(); all six parts share one, so detach it first.
            for (int m = 0; m < p.model.materialCount; m++)
            {
                p.model.materials[m].shader.id = rlGetShaderIdDefault();
                p.model.materials[m].shader.locs = rlGetShaderLocsDefault();
            }
            UnloadModel(p.model);
            p.model = {};
        }
        UnloadShader(shader);
        CloseWindow();
        windowOpen = false;
    }
};

AvatarRenderer::AvatarRenderer() : impl_(new Impl) {}
AvatarRenderer::~AvatarRenderer() = default;

bool AvatarRenderer::init(const std::string &contentDir, std::string &err)
{
    Impl &s = *impl_;
    s.dir = contentDir;

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "AvatarRenderer"); // real output size comes from the render texture
    if (!IsWindowReady())
    {
        err = "could not create an OpenGL context";
        return false;
    }
    s.windowOpen = true;

    s.shader = LoadShaderFromMemory(VS_SRC, FS_SRC);
    if (s.shader.id == rlGetShaderIdDefault())
    {
        err = "shader failed to compile (see log above)";
        return false;
    }
    s.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(s.shader, "matModel");
    s.shader.locs[SHADER_LOC_COLOR_DIFFUSE] = GetShaderLocation(s.shader, "colDiffuse");
    s.useTexLoc = GetShaderLocation(s.shader, "useTexture");
    s.hqLoc = GetShaderLocation(s.shader, "HQ");

    // Meshes: loaded once, kept for the lifetime of the process.
    BoundingBox extents = {};
    bool first = true;

    for (Part &p : s.parts)
    {
        std::string path = s.dir + "/fonts/" + p.file;
        p.model = LoadModel(path.c_str());
        if (p.model.meshCount == 0)
        {
            err = "failed to load " + path;
            return false;
        }

        for (int m = 0; m < p.model.materialCount; m++)
        {
            p.model.materials[m].shader = s.shader;
            p.origTex.push_back(p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture);
            p.origColor.push_back(p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].color);
        }

        BoundingBox b = GetModelBoundingBox(p.model);
        b.min = Vector3Add(Vector3Multiply(b.min, p.scale), p.pos);
        b.max = Vector3Add(Vector3Multiply(b.max, p.scale), p.pos);

        if (first)
        {
            extents = b;
            first = false;
        }
        else
        {
            extents.min = Vector3Min(extents.min, b.min);
            extents.max = Vector3Max(extents.max, b.max);
        }
    }

    extents.min = Vector3SubtractValue(extents.min, EXPAND);
    extents.max = Vector3AddValue(extents.max, EXPAND);

    // The character's geometry never changes, so the camera is solved once.
    s.focus = Vector3Scale(Vector3Add(extents.min, extents.max), 0.5f);
    Vector3 camDir = thumbnailDirection(MODEL_LOOK);
    float distance = zoomExtentsDistance(extents, s.focus, camDir);
    s.camPos = Vector3Add(s.focus, Vector3Scale(camDir, distance));

    std::printf("camera pos: %.3f %.3f %.3f  focus: %.3f %.3f %.3f  dist: %.3f  fov: %.1f\n",
                s.camPos.x, s.camPos.y, s.camPos.z, s.focus.x, s.focus.y, s.focus.z, distance, FOV_DEG);

    // Lighting uniforms are constant too.
    Vector3 lamp0Dir = Vector3Normalize(LAMP0_DIR);
    Vector2 specParams = {SPECULAR, GLOSS};
    Vector4 fogParams = {0.0f, 0.0f, FOG_END, 1.0f / (FOG_END - FOG_START)};

    SetShaderValue(s.shader, GetShaderLocation(s.shader, "CameraPosition"), &s.camPos, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "AmbientColor"), &AMBIENT_COLOR, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "Lamp0Color"), &LAMP0_COLOR, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "Lamp0Dir"), &lamp0Dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "Lamp1Color"), &LAMP1_COLOR, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "FogColor"), &FOG_COLOR, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "FogParams"), &fogParams, SHADER_UNIFORM_VEC4);
    SetShaderValue(s.shader, GetShaderLocation(s.shader, "SpecParams"), &specParams, SHADER_UNIFORM_VEC2);

    rlSetClipPlanes(NEAR_Z, FAR_Z);
    return true;
}

bool AvatarRenderer::render(const RenderRequest &req, std::string &pngBase64, std::string &err)
{
    Impl &s = *impl_;
    if (!s.windowOpen)
    {
        err = "renderer is not initialised";
        return false;
    }
    if (req.size < 16 || req.size > 4096)
    {
        err = "size out of range";
        return false;
    }

    // ---- per-request textures -------------------------------------------
    const std::string texDir = s.dir + "/textures/";

    Texture2D faceTex = {}, shirtTex = {}, pantsTex = {}, torsoTex = {};
    Image shirtImg = {}, pantsImg = {};

    if (!req.face.empty())
        faceTex = LoadTexture((texDir + req.face).c_str());
    if (!req.shirt.empty())
        shirtImg = LoadImage((texDir + req.shirt).c_str());
    if (!req.pants.empty())
        pantsImg = LoadImage((texDir + req.pants).c_str());

    const bool hasShirt = shirtImg.data != nullptr;
    const bool hasPants = pantsImg.data != nullptr;

    if (hasShirt)
        shirtTex = LoadTextureFromImage(shirtImg);
    if (hasPants)
        pantsTex = LoadTextureFromImage(pantsImg);

    // Torso = pants layer with the shirt layered on top (same as before).
    if (hasShirt || hasPants)
    {
        int tw = hasShirt ? shirtImg.width : pantsImg.width;
        int th = hasShirt ? shirtImg.height : pantsImg.height;

        Image torso = GenImageColor(tw, th, BLANK);
        if (hasPants)
            ImageDraw(&torso, pantsImg, {0, 0, (float)pantsImg.width, (float)pantsImg.height}, {0, 0, (float)tw, (float)th}, WHITE);
        if (hasShirt)
            ImageDraw(&torso, shirtImg, {0, 0, (float)shirtImg.width, (float)shirtImg.height}, {0, 0, (float)tw, (float)th}, WHITE);
        torsoTex = LoadTextureFromImage(torso);
        UnloadImage(torso);
    }
    if (hasShirt)
        UnloadImage(shirtImg);
    if (hasPants)
        UnloadImage(pantsImg);

    // Which texture each part gets, in PartId order.
    const Texture2D *partTex[PART_COUNT] = {
        &faceTex,  // head
        &torsoTex, // torso
        &shirtTex, // larm
        &shirtTex, // rarm
        &pantsTex, // lleg
        &pantsTex, // rleg
    };

    // ---- draw -----------------------------------------------------------
    bool ok = true;
    RenderTexture2D rt = LoadRenderTexture(req.size, req.size);
    if (rt.id == 0)
    {
        err = "could not allocate render target";
        ok = false;
    }
    else
    {
        for (int i = 0; i < PART_COUNT; i++)
        {
            if (partTex[i]->id == 0)
                continue;
            Part &p = s.parts[i];
            for (int m = 0; m < p.model.materialCount; m++)
            {
                p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = *partTex[i];
                p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            }
        }

        int hqInt = req.hq ? 1 : 0;
        SetShaderValue(s.shader, s.hqLoc, &hqInt, SHADER_UNIFORM_INT);

        Camera3D cam = {};
        cam.position = s.camPos;
        cam.target = s.focus;
        cam.up = {0, 1, 0};
        cam.fovy = FOV_DEG;
        cam.projection = CAMERA_PERSPECTIVE;

        BeginTextureMode(rt);
        ClearBackground(BLANK);
        BeginMode3D(cam);
        for (int i = 0; i < PART_COUNT; i++)
        {
            int hasTex = partTex[i]->id > 0 ? 1 : 0;
            SetShaderValue(s.shader, s.useTexLoc, &hasTex, SHADER_UNIFORM_INT);

            Part &p = s.parts[i];
            DrawModelEx(p.model, p.pos, {0, 1, 0}, 0.0f, p.scale, colorFromRgb(req.colors[i]));
        }
        EndMode3D();
        EndTextureMode();

        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        int pngSize = 0;
        unsigned char *png = ExportImageToMemory(img, ".png", &pngSize);
        UnloadImage(img);

        if (png == nullptr || pngSize <= 0)
        {
            err = "PNG encoding failed";
            ok = false;
        }
        else
        {
            pngBase64 = base64Encode(png, (size_t)pngSize);
            MemFree(png);
        }

        // Put the resident models back how we found them.
        for (Part &p : s.parts)
        {
            for (int m = 0; m < p.model.materialCount; m++)
            {
                p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].texture = p.origTex[m];
                p.model.materials[m].maps[MATERIAL_MAP_DIFFUSE].color = p.origColor[m];
            }
        }

        UnloadRenderTexture(rt);
    }

    if (faceTex.id > 0)
        UnloadTexture(faceTex);
    if (shirtTex.id > 0)
        UnloadTexture(shirtTex);
    if (pantsTex.id > 0)
        UnloadTexture(pantsTex);
    if (torsoTex.id > 0)
        UnloadTexture(torsoTex);

    return ok;
}

void AvatarRenderer::pump()
{
    PollInputEvents();
}
