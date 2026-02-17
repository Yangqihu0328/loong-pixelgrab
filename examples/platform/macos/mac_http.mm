// Copyright 2026 The PixelGrab Authors
// macOS implementation of IPlatformHttp using NSURLSession.

#include "core/platform_http.h"

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <cstdio>

class MacPlatformHttp : public IPlatformHttp {
 public:
  std::string HttpsGet(const char* host, const char* path) override {
    @autoreleasepool {
      NSString* url_str = [NSString stringWithFormat:@"https://%s%s",
                           host, path];
      NSURL* url = [NSURL URLWithString:url_str];
      if (!url) return "";

      NSMutableURLRequest* request =
          [NSMutableURLRequest requestWithURL:url];
      [request setHTTPMethod:@"GET"];
      [request setValue:@"PixelGrab/1.0" forHTTPHeaderField:@"User-Agent"];
      [request setTimeoutInterval:15.0];

      __block NSData* responseData = nil;
      __block NSError* responseError = nil;
      dispatch_semaphore_t sem = dispatch_semaphore_create(0);

      NSURLSessionDataTask* task =
          [[NSURLSession sharedSession]
              dataTaskWithRequest:request
               completionHandler:^(NSData* data, NSURLResponse*, NSError* err) {
                 responseData = data;
                 responseError = err;
                 dispatch_semaphore_signal(sem);
               }];
      [task resume];
      dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW,
                                                  15 * NSEC_PER_SEC));

      if (responseError || !responseData) return "";

      return std::string(
          static_cast<const char*>([responseData bytes]),
          [responseData length]);
    }
  }

  void OpenUrlInBrowser(const char* url) override {
    @autoreleasepool {
      NSString* ns_url = [NSString stringWithUTF8String:url];
      [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:ns_url]];
    }
  }
};

std::unique_ptr<IPlatformHttp> CreatePlatformHttp() {
  return std::make_unique<MacPlatformHttp>();
}

#endif  // __APPLE__
