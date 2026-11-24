#include "rutube.h"

#include "http.h"
#include "json_util.h"
#include "util.h"

namespace vt {

namespace {

const char* kBase = "https://rutube.ru/api/";

const char* kUserAgent =
    "Mozilla/5.0 (PlayStation Vita 3.74) AppleWebKit/537.73 "
    "(KHTML, like Gecko) Silk/3.2";

JDocPtr request_json(const std::string& url, std::string* error) {
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    req.userAgent = kUserAgent;
    req.connectTimeout = 20;
    req.stallTimeout = 25;
    req.maxBytes = 16u << 20;

    req.headers.push_back("Accept: application/json");
    req.headers.push_back("Referer: https://rutube.ru/");
    req.headers.push_back("Origin: https://rutube.ru");

    HttpResponse response = http_request(req);

    if (!response.error.empty()) {
        if (error) *error = "Network error: " + response.error;
        return nullptr;
    }

    if (!response.ok()) {
        if (error) *error = "RUTUBE returned " + response.describe();
        return nullptr;
    }

    std::string parseError;
    JDocPtr doc = jparse(response.body, &parseError);

    if (!doc && error) {
        *error = parseError;
    }

    return doc;
}

std::string get_string(const JVal& obj,
                       const char* a,
                       const char* b = nullptr,
                       const char* c = nullptr) {
    std::string value = jstr(jget(obj, a));

    if (value.empty() && b)
        value = jstr(jget(obj, b));

    if (value.empty() && c)
        value = jstr(jget(obj, c));

    return value;
}

std::string get_author(const JVal& obj) {
    const JVal* author = jget(obj, "author");

    if (!author)
        author = jget(obj, "owner");

    if (!author)
        author = jget(obj, "channel");

    if (!author)
        return {};

    if (author->IsString())
        return jstr(author);

    std::string name = get_string(
        *author, "name", "title", "username"
    );

    return name;
}

RutubeVideo parse_video(const JVal& obj) {
    RutubeVideo video;

    video.id = get_string(obj, "id", "video_id");
    video.title = get_string(obj, "title", "name");
    video.description = get_string(obj, "description");
    video.thumbnail = get_string(
        obj, "thumbnail_url", "thumbnail", "picture"
    );

    video.author = get_author(obj);

    video.duration = get_string(
        obj, "duration", "duration_text"
    );

    video.views = get_string(
        obj, "hits", "views"
    );

    video.published = get_string(
        obj, "publication_ts", "created_ts"
    );

    if (!video.id.empty()) {
        video.url = "https://rutube.ru/video/" +
                    video.id + "/";
    }

    return video;
}

const JVal* find_results(const JVal& root) {
    if (root.IsArray())
        return &root;

    const char* keys[] = {
        "results",
        "items",
        "videos",
        "video"
    };

    for (const char* key : keys) {
        const JVal* value = jget(root, key);

        if (value && value->IsArray())
            return value;
    }

    return nullptr;
}

} // namespace

RutubeSearchResult Rutube::search(
    const std::string& query,
    int page
) {
    RutubeSearchResult result;

    if (query.empty()) {
        result.error = "Search query is empty";
        return result;
    }

    if (page < 1)
        page = 1;

    std::string url =
        std::string(kBase) +
        "search/video/?query=" + url_encode(query) +
        "&page=" + std::to_string(page) +
        "&limit=20";

    std::string error;
    JDocPtr doc = request_json(url, &error);

    if (!doc) {
        result.error = error;
        return result;
    }

    const JVal* items = find_results(*doc);

    if (!items) {
        result.error = "RUTUBE search returned no results array";
        return result;
    }

    for (const auto& item : items->GetArray()) {
        RutubeVideo video = parse_video(item);

        if (video.id.empty() || video.title.empty())
            continue;

        result.videos.push_back(std::move(video));
    }

    VT_LOG("rutube: search '%s' -> %u videos",
           query.c_str(),
           (unsigned)result.videos.size());

    return result;
}

bool Rutube::getVideo(
    const std::string& videoId,
    RutubeVideo& video,
    std::string* error
) {
    if (videoId.empty()) {
        if (error) *error = "Empty video ID";
        return false;
    }

    std::string url =
        std::string(kBase) +
        "video/" + videoId + "/";

    JDocPtr doc = request_json(url, error);

    if (!doc)
        return false;

    video = parse_video(*doc);

    if (video.id.empty())
        video.id = videoId;

    if (video.title.empty()) {
        if (error) *error = "Video information is incomplete";
        return false;
    }

    return true;
}

RutubePlayback Rutube::getPlayback(
    const std::string& videoId
) {
    RutubePlayback playback;

    if (videoId.empty()) {
        playback.error = "Empty video ID";
        return playback;
    }

    std::string url =
        std::string(kBase) +
        "play/options/" + videoId + "/?format=json";

    JDocPtr doc = request_json(url, &playback.error);

    if (!doc)
        return playback;

    const JVal* balancer = jget(*doc, "video_balancer");

    if (balancer) {
        playback.m3u8 = get_string(
            *balancer, "m3u8"
        );
    }

    if (playback.m3u8.empty()) {
        playback.m3u8 = jstr(jget(*doc, "m3u8"));
    }

    if (playback.m3u8.empty()) {
        playback.error =
            "RUTUBE did not return an HLS stream";
    }

    return playback;
}

} // namespace vt
