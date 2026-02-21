// Copyright 2026 The loong-pixelgrab Authors
// Tests for: pixelgrab_version_string, pixelgrab_version_major,
//            pixelgrab_version_minor, pixelgrab_version_patch

#include <cstring>

#include "gtest/gtest.h"
#include "pixelgrab/pixelgrab.h"

TEST(VersionTest, VersionStringIsNotNull) {
  const char* ver = pixelgrab_version_string();
  ASSERT_NE(ver, nullptr);
}

TEST(VersionTest, VersionStringMatchesMacro) {
  EXPECT_STREQ(pixelgrab_version_string(), PIXELGRAB_VERSION_STRING);
}

TEST(VersionTest, VersionStringMatchesExpected) {
  EXPECT_STREQ(pixelgrab_version_string(), "1.3.0");
}

TEST(VersionTest, MajorVersion) {
  EXPECT_EQ(pixelgrab_version_major(), PIXELGRAB_VERSION_MAJOR);
  EXPECT_EQ(pixelgrab_version_major(), 1);
}

TEST(VersionTest, MinorVersion) {
  EXPECT_EQ(pixelgrab_version_minor(), PIXELGRAB_VERSION_MINOR);
  EXPECT_EQ(pixelgrab_version_minor(), 3);
}

TEST(VersionTest, PatchVersion) {
  EXPECT_EQ(pixelgrab_version_patch(), PIXELGRAB_VERSION_PATCH);
  EXPECT_EQ(pixelgrab_version_patch(), 0);
}
