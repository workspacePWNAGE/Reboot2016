#pragma once
// Hat support. Like renderer.hpp this header has NO raylib / httplib includes,
// so both main.cpp (HTTP side) and renderer.cpp (OpenGL side) can use it.
//
// A hat is a Roblox *legacy XML* model (<roblox version="4"> ... <Item class="Hat">)
// stored on the site and served by /api/get-model?id=N. The renderer never reads
// hat files from disk: main.cpp downloads the model, then its mesh and texture,
// from --site-url, and this module only parses and checks what was downloaded.
//
// parseHatXml() checks:
//   1. every <url> in the file is on the configured public site (e.g. reboot16.xyz)
//   2. MeshId / TextureId use the site's asset endpoints, which also gives the ids:
//
//        http://reboot16.xyz/api/get-mesh?id=7
//        http://reboot16.xyz/api/get-texture?id=2
//
// (main.cpp then downloads mesh 7 and texture 2; a 404 means "does not exist".)
// Every problem found is reported, not just the first one.

#include <array>
#include <string>
#include <vector>

// The site's endpoints (see assets.js).
inline constexpr const char *HAT_MODEL_ENDPOINT = "/api/get-model";
inline constexpr const char *HAT_MESH_ENDPOINT = "/api/get-mesh";
inline constexpr const char *HAT_TEXTURE_ENDPOINT = "/api/get-texture";

struct HatModel
{
    std::string name;

    // Hat.AttachmentPoint: position + 3x3 rotation (row-major R00 R01 R02 R10 ...)
    std::array<float, 3> attachPos = {0, 0, 0};
    std::array<float, 9> attachRot = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    // SpecialMesh
    std::array<float, 3> scale = {1, 1, 1};
    std::array<float, 3> offset = {0, 0, 0};
    std::string meshUrl;
    std::string textureUrl; // may be empty (untextured hat)

    unsigned color = 0xA3A2A5; // Handle colour 0xRRGGBB (shows through transparent texture areas)

    // Filled in by parseHatXml: the ids inside the URLs.
    std::string meshId;
    std::string textureId; // empty when there is no texture

    // Filled in by the caller after downloading them from the site.
    std::string meshData;    // raw .obj or Roblox .mesh bytes
    std::string textureData; // raw image bytes
    std::string textureExt;  // ".png" / ".jpg" / ".bmp", from imageExtension()
};

// Parse + check a hat model. No disk, no network. Returns true only when `problems` is empty.
bool parseHatXml(const std::string &xml, const std::string &publicUrl, HatModel &out, std::vector<std::string> &problems);

// ".png" / ".jpg" / ".bmp" from the file's first bytes, "" if it is none of those.
std::string imageExtension(const std::string &bytes);

// Hat meshes as a flat triangle list (3 vertices per triangle, no index buffer).
struct MeshData
{
    std::vector<float> pos; // 3 floats per vertex
    std::vector<float> nrm; // 3 per vertex
    std::vector<float> uv;  // 2 per vertex
};

// Reads mesh bytes, detecting the format from the content:
//   "version 1.xx / 2.xx / 3.xx" header  -> Roblox .mesh
//   anything else                        -> .obj (v / vt / vn / f, any polygon size)
bool loadHatMeshData(const std::string &bytes, MeshData &out, std::string &err);
