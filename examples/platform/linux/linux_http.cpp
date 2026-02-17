// Copyright 2026 The PixelGrab Authors
// Linux implementation of IPlatformHttp using libcurl + xdg-open.

#include "core/platform_http.h"

#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include <cstdio>
#include <cstdlib>
#include <curl/curl.h>

static size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata) {
  auto* result = static_cast<std::string*>(userdata);
  size_t total = size * nmemb;
  result->append(ptr, total);
  return total;
}

class LinuxPlatformHttp : public IPlatformHttp {
 public:
  std::string HttpsGet(const char* host, const char* path) override {
    std::string url = "https://";
    url += host;
    url += path;

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string result;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PixelGrab/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      std::fprintf(stderr, "  [Linux] HTTPS GET failed: %s\n",
                   curl_easy_strerror(res));
      return "";
    }
    return result;
  }

  void OpenUrlInBrowser(const char* url) override {
    std::string cmd = "xdg-open '";
    cmd += url;
    cmd += "' &";
    int ret = std::system(cmd.c_str());
    (void)ret;
  }
};

std::unique_ptr<IPlatformHttp> CreatePlatformHttp() {
  return std::make_unique<LinuxPlatformHttp>();
}

#endif  // __linux__
