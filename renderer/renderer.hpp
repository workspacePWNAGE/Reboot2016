#pragma once
// AvatarRenderer - owns the raylib/OpenGL side of the thumbnail renderer.
//
// This header deliberately has NO raylib includes. raylib and the Win32/winsock
// headers pulled in by cpp-httplib clash (Rectangle, CloseWindow, ShowCursor...),
// so raylib lives only in renderer.cpp and the HTTP server only in main.cpp.
//
// Threading: init() and render() must be called from the SAME thread (the one
// that owns the OpenGL context). The server calls them from the main thread only.

#include <array>
#include <memory>
#include <string>

struct RenderRequest
{
    static constexpr int PART_COUNT = 6;

    int size = 512;                 // square output, pixels
    bool hq = true;                 // per-pixel lighting (hq) or per-vertex (lq)
    std::string face = "face.png";  // file inside textureDir, "" = none
    std::string shirt;              // "" = none
    std::string pants;              // "" = none

    // Body colours as 0xRRGGBB: head, torso, larm, rarm, lleg, rleg
    std::array<unsigned, PART_COUNT> colors = {
        0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F};
};

// Query-string keys for the colours above, same order as RenderRequest::colors.
inline constexpr const char *PART_KEYS[RenderRequest::PART_COUNT] = {
    "head", "torso", "larm", "rarm", "lleg", "rleg"};

class AvatarRenderer
{
public:
    AvatarRenderer();
    ~AvatarRenderer();

    AvatarRenderer(const AvatarRenderer &) = delete;
    AvatarRenderer &operator=(const AvatarRenderer &) = delete;

    // Opens the hidden window, compiles the shader, loads the six .obj parts
    // from <contentDir>/fonts and works out the camera. Done once at startup.
    bool init(const std::string &contentDir, const std::string &textureDir, std::string &err);

    // Renders one avatar. On success pngBase64 holds the PNG, base64-encoded.
    bool render(const RenderRequest &req, std::string &pngBase64, std::string &err);

    // Keeps the (hidden) window's event queue serviced while idle.
    void pump();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};