// Copyright 2026 The loong-pixelgrab Authors
//
// Shared translation logic — URL encoding, body construction, JSON parsing.

#include "translate/translate_backend.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <sstream>

#include "core/logger.h"

namespace pixelgrab {
namespace internal {

static constexpr const char* kBaiduApiUrl =
    "https://fanyi-api.baidu.com/api/trans/vip/translate";

bool TranslateBackend::IsSupported() const {
  return !config_.app_id.empty() && !config_.secret_key.empty();
}

std::string TranslateBackend::Translate(const char* text, const char* from,
                                        const char* to) {
  if (!text || !text[0] || !to || !to[0]) {
    last_error_detail_ = "Invalid parameters (empty text or target language)";
    return {};
  }
  if (!IsSupported()) {
    last_error_detail_ = "Translation not configured (missing app_id/secret)";
    PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
    return {};
  }

  last_error_detail_.clear();

  std::string body = BuildBaiduBody(text, from ? from : "auto", to);
  if (body.empty()) return {};

  PIXELGRAB_LOG_DEBUG("Sending translation request to Baidu API");
  std::string response = HttpPost(kBaiduApiUrl, body);
  if (response.empty()) {
    if (last_error_detail_.empty())
      last_error_detail_ = "HTTP request returned empty response";
    PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
    return {};
  }

  PIXELGRAB_LOG_DEBUG("Baidu API response: {}", response);
  return ParseBaiduResponse(response);
}

std::string TranslateBackend::BuildBaiduBody(const char* text, const char* from,
                                             const char* to) {
  std::string salt = GenerateSalt();
  // sign = md5(appid + q + salt + secret_key)
  std::string sign_input = config_.app_id + text + salt + config_.secret_key;
  std::string sign = ComputeMD5(sign_input);
  if (sign.empty()) {
    last_error_detail_ = "MD5 computation failed for translation signature";
    PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
    return {};
  }

  std::string body;
  body += "q=" + UrlEncode(text);
  body += "&from=" + UrlEncode(from);
  body += "&to=" + UrlEncode(to);
  body += "&appid=" + UrlEncode(config_.app_id);
  body += "&salt=" + UrlEncode(salt);
  body += "&sign=" + UrlEncode(sign);
  return body;
}

std::string TranslateBackend::UrlEncode(const std::string& str) {
  std::string result;
  result.reserve(str.size() * 3);
  for (unsigned char c : str) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      result += buf;
    }
  }
  return result;
}

// Parse Baidu Translate API JSON response.
// Success: {"from":"en","to":"zh","trans_result":[{"src":"hello","dst":"..."}]}
// Multiple results are joined with newlines.
// Error:   {"error_code":"54001","error_msg":"Invalid Sign"}
std::string TranslateBackend::ParseBaiduResponse(const std::string& json) {
  auto find_value = [&](const std::string& key) -> std::string {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
  };

  std::string error_code = find_value("error_code");
  if (!error_code.empty()) {
    std::string error_msg = find_value("error_msg");
    last_error_detail_ =
        "Baidu API error " + error_code + ": " + error_msg;
    PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
    return {};
  }

  // Collect all "dst" values from trans_result array.
  std::string result;
  std::string search = "\"dst\":\"";
  size_t pos = 0;
  while ((pos = json.find(search, pos)) != std::string::npos) {
    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) break;
    if (!result.empty()) result += '\n';
    // Unescape basic JSON sequences.
    std::string segment = json.substr(pos, end - pos);
    std::string decoded;
    decoded.reserve(segment.size());
    for (size_t i = 0; i < segment.size(); ++i) {
      if (segment[i] == '\\' && i + 1 < segment.size()) {
        char next = segment[i + 1];
        if (next == 'n') { decoded += '\n'; ++i; }
        else if (next == 't') { decoded += '\t'; ++i; }
        else if (next == '"') { decoded += '"'; ++i; }
        else if (next == '\\') { decoded += '\\'; ++i; }
        else if (next == '/') { decoded += '/'; ++i; }
        else if (next == 'u' && i + 5 < segment.size()) {
          unsigned long cp = std::strtoul(segment.c_str() + i + 2, nullptr, 16);
          if (cp > 0) {
            // Simple BMP codepoint to UTF-8.
            if (cp < 0x80) {
              decoded += static_cast<char>(cp);
            } else if (cp < 0x800) {
              decoded += static_cast<char>(0xC0 | (cp >> 6));
              decoded += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
              decoded += static_cast<char>(0xE0 | (cp >> 12));
              decoded += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
              decoded += static_cast<char>(0x80 | (cp & 0x3F));
            }
            i += 5;
          } else {
            decoded += segment[i];
          }
        } else {
          decoded += segment[i];
        }
      } else {
        decoded += segment[i];
      }
    }
    result += decoded;
    pos = end + 1;
  }

  if (result.empty()) {
    last_error_detail_ = "No 'dst' field found in Baidu response";
    PIXELGRAB_LOG_ERROR("{}", last_error_detail_);
  }
  return result;
}

std::string TranslateBackend::GenerateSalt() {
  auto seed = static_cast<unsigned>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(100000, 999999);
  return std::to_string(dist(rng));
}

}  // namespace internal
}  // namespace pixelgrab
