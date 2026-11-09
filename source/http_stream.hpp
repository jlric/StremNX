#pragma once

#include <mpv/client.h>

#include <string>

// The Ounce libmpv build does not include its HTTPS stream handler. Register a
// libcurl-backed stream so mpv can demux ordinary HTTP media files.
int addHttpStream(mpv_handle* mpv, const std::string& url);
