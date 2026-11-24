#pragma once

#include <string>
#include <vector>

#include "models.h"

namespace vt {

struct RutubeVideo {
    std::string id;
    std::string title;
    std::string author;
    std::string description;
    std::string thumbnail;
    std::string duration;
    std::string views;
    std::string published;
    std::string url;
};

struct RutubeSearchResult {
    std::vector<RutubeVideo> videos;
    std::string error;
};

struct RutubePlayback {
    std::string m3u8;
    std::string error;

    bool ok() const {
        return !m3u8.empty();
    }
};

class Rutube {
public:
    RutubeSearchResult search(const std::string& query, int page = 1);

    bool getVideo(const std::string& videoId,
                  RutubeVideo& video,
                  std::string* error = nullptr);

    RutubePlayback getPlayback(const std::string& videoId);
};

} // namespace vt
