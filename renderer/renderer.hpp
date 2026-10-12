#pragma once

#include "hat.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

struct RenderRequest
{
    static constexpr int PART_COUNT = 6;

    int size = 512;                 
    bool hq = true;
    std::string face = "face.png";  
    std::string shirt;              
    std::string pants;              

    std::vector<HatModel> hats;

    std::array<unsigned, PART_COUNT> colors = {
        0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F, 0x7F7F7F};
};

inline constexpr const char *PART_KEYS[RenderRequest::PART_COUNT] = {
    "head", "torso", "larm", "rarm", "lleg", "rleg"};

class AvatarRenderer
{
public:
    AvatarRenderer();
    ~AvatarRenderer();

    AvatarRenderer(const AvatarRenderer &) = delete;
    AvatarRenderer &operator=(const AvatarRenderer &) = delete;

    bool init(const std::string &contentDir, const std::string &textureDir, std::string &err);

    bool render(const RenderRequest &req, std::string &pngBase64, std::string &err);

    void pump();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};