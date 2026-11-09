#include "http_stream.hpp"

#include <mpv/stream_cb.h>
#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "log.hpp"

namespace {

constexpr size_t kChunkSize = 4 * 1024 * 1024;

struct HttpStream {
    std::string url;
    CURL* curl = nullptr;
    std::vector<char> cache;
    int64_t cacheStart = 0;
    int64_t pos = 0;
    int64_t size = -1;
    bool acceptsRanges = false;
    unsigned fetchCount = 0;
    std::atomic<bool> cancelled{false};

    ~HttpStream() { if (curl) curl_easy_cleanup(curl); }
};

struct Transfer {
    HttpStream* stream;
    int64_t requestedStart;
    int64_t total = -1;
    bool hasContentRange = false;
    bool overflow = false;
};

size_t writeChunk(char* data, size_t size, size_t count, void* userdata) {
    auto& x = *static_cast<Transfer*>(userdata);
    size_t n = size * count;
    if (x.stream->cancelled.load()) return 0;
    if (n > kChunkSize - x.stream->cache.size()) {
        x.overflow = true;
        return 0;
    }
    x.stream->cache.insert(x.stream->cache.end(), data, data + n);
    return n;
}

size_t readHeader(char* data, size_t size, size_t count, void* userdata) {
    auto& x = *static_cast<Transfer*>(userdata);
    size_t n = size * count;
    std::string line(data, n);
    for (char& c : line) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (line.compare(0, 14, "content-range:") == 0) {
        long long first = -1, last = -1, total = -1;
        if (sscanf(line.c_str() + 14, " bytes %lld-%lld/%lld", &first, &last, &total) == 3 &&
            first == x.requestedStart && last >= first) {
            x.hasContentRange = true;
            x.total = total;
        }
    }
    return n;
}

int checkCancel(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<Transfer*>(userdata)->stream->cancelled.load() ? 1 : 0;
}

bool fetch(HttpStream& stream, int64_t start) {
    if (stream.cancelled.load()) return false;
    if (!stream.curl) stream.curl = curl_easy_init();
    CURL* curl = stream.curl;
    if (!curl) return false;

    stream.cache.clear();
    stream.cacheStart = start;
    Transfer x{&stream, start};
    char range[80];
    snprintf(range, sizeof(range), "%lld-%lld", static_cast<long long>(start),
             static_cast<long long>(start + kChunkSize - 1));
    curl_easy_setopt(curl, CURLOPT_URL, stream.url.c_str());
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 40L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "StremNX/0.3.1");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "identity");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeChunk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &x);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, readHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &x);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, checkCancel);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &x);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    auto started = std::chrono::steady_clock::now();
    CURLcode rc = curl_easy_perform(curl);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (stream.cancelled.load()) return false;

    ++stream.fetchCount;
    if (stream.fetchCount <= 6 || stream.fetchCount % 8 == 0 || rc != CURLE_OK) {
        double mib = static_cast<double>(stream.cache.size()) / (1024 * 1024);
        logMsg("http stream: %.2f MiB in %lld ms (%.2f MiB/s), HTTP %ld, curl %d",
               mib, static_cast<long long>(elapsed), mib * 1000 / std::max<int64_t>(1, elapsed),
               status, static_cast<int>(rc));
    }

    // Some servers close an otherwise valid range response early. The bytes
    // received are still contiguous; the next read requests the remainder.
    if (status == 206 && x.hasContentRange && !stream.cache.empty() &&
        (rc == CURLE_OK || rc == CURLE_PARTIAL_FILE)) {
        stream.acceptsRanges = true;
        stream.size = x.total;
        return true;
    }
    // A small file can be served without byte ranges. Large non-range streams
    // cannot be safely sought or cached with this simple callback.
    if (start == 0 && status == 200 && rc == CURLE_OK && !x.overflow) {
        stream.acceptsRanges = false;
        stream.size = static_cast<int64_t>(stream.cache.size());
        return true;
    }
    logMsg("http stream: request failed (HTTP %ld, curl %d, range %s)", status,
           static_cast<int>(rc), x.hasContentRange ? "yes" : "no");
    return false;
}

int64_t httpRead(void* userdata, char* dst, uint64_t n) {
    auto& s = *static_cast<HttpStream*>(userdata);
    if (s.cancelled.load()) return -1;
    if (s.size >= 0 && s.pos >= s.size) return 0;
    if (s.pos < s.cacheStart || s.pos >= s.cacheStart + static_cast<int64_t>(s.cache.size())) {
        if (!s.acceptsRanges || !fetch(s, s.pos)) return -1;
    }
    size_t offset = static_cast<size_t>(s.pos - s.cacheStart);
    size_t available = s.cache.size() - offset;
    size_t take = static_cast<size_t>(std::min<uint64_t>(n, available));
    if (!take) return 0;
    memcpy(dst, s.cache.data() + offset, take);
    s.pos += static_cast<int64_t>(take);
    return static_cast<int64_t>(take);
}

int64_t httpSeek(void* userdata, int64_t offset) {
    auto& s = *static_cast<HttpStream*>(userdata);
    if (s.cancelled.load()) return MPV_ERROR_GENERIC;
    if (!s.acceptsRanges) return MPV_ERROR_UNSUPPORTED;
    if (offset < 0 || (s.size >= 0 && offset > s.size)) return MPV_ERROR_GENERIC;
    s.pos = offset;
    return offset;
}

int64_t httpSize(void* userdata) {
    int64_t size = static_cast<HttpStream*>(userdata)->size;
    return size >= 0 ? size : MPV_ERROR_UNSUPPORTED;
}

void httpCancel(void* userdata) {
    static_cast<HttpStream*>(userdata)->cancelled.store(true);
}

void httpClose(void* userdata) {
    delete static_cast<HttpStream*>(userdata);
}

int httpOpen(void* userdata, char*, mpv_stream_cb_info* info) {
    auto* url = static_cast<const std::string*>(userdata);
    auto* stream = new (std::nothrow) HttpStream;
    if (!stream) return MPV_ERROR_LOADING_FAILED;
    stream->url = *url;
    if (!fetch(*stream, 0)) {
        delete stream;
        return MPV_ERROR_LOADING_FAILED;
    }
    logMsg("http stream: opened (range %s, size %lld)",
           stream->acceptsRanges ? "yes" : "no", static_cast<long long>(stream->size));
    info->cookie = stream;
    info->read_fn = httpRead;
    info->seek_fn = httpSeek;
    info->size_fn = httpSize;
    info->close_fn = httpClose;
    info->cancel_fn = httpCancel;
    return 0;
}

}  // namespace

int addHttpStream(mpv_handle* mpv, const std::string& url) {
    return mpv_stream_cb_add_ro(mpv, "stremnxhttp", const_cast<std::string*>(&url), httpOpen);
}
