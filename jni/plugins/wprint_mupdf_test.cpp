/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <limits>
#include <string.h>

extern "C" {
#include "wprint_mupdf.h"
#include "lib_wprint.h"
}

constexpr int DEFAULT_DPI = 300;

static double g_mockWidth = 612.0;
static double g_mockHeight = 792.0;
static pdf_render_ifc_t g_mock_ifc;

static int mock_openDocument(pdf_render_ifc_t *obj, const char *fileName) {
    return 1; // 1 page
}

static status_t mock_getPageAttributes(
    pdf_render_ifc_t *obj, int page, double *width, double *height) {
    *width = g_mockWidth;
    *height = g_mockHeight;
    return OK;
}

static status_t mock_renderPageStripe(
    pdf_render_ifc_t *obj, int page, int width, int height, float zoom, char *buffer) {
    return OK;
}

static void mock_destroy(pdf_render_ifc_t *obj) {
}

extern "C" pdf_render_ifc_t *create_pdf_render_ifc() {
    return &g_mock_ifc;
}

class WprintMupdfTest : public ::testing::Test {
 protected:
  void SetUp() override {
      g_mockWidth = 612.0;
      g_mockHeight = 792.0;

      memset(&g_mock_ifc, 0, sizeof(g_mock_ifc));
      g_mock_ifc.openDocument = mock_openDocument;
      g_mock_ifc.getPageAttributes = mock_getPageAttributes;
      g_mock_ifc.renderPageStripe = mock_renderPageStripe;
      g_mock_ifc.destroy = mock_destroy;

      memset(&image_info_, 0, sizeof(image_info_));
      image_info_.pdf_render_resolution = DEFAULT_DPI;
      image_info_.decoder_data.page = 1;
      image_info_.decoder_data.urlPath = "test.pdf";

      plugin_ = wprint_mupdf_decode_ifc;
      ASSERT_NE(plugin_, nullptr);
      plugin_->init(&image_info_);
  }

  void TearDown() override {
      if (plugin_) {
          plugin_->cleanup(&image_info_);
      }
  }

  const image_decode_ifc_t *plugin_ = nullptr;
  wprint_image_info_t image_info_;
};

TEST_F(WprintMupdfTest, GetHdr_ValidDimensions_ReturnsOK) {
    g_mockWidth = 612.0;
    g_mockHeight = 792.0;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, OK);
    // Single-precision float division (300.0f / 72.0f) truncated yields 2549 and 3299
    EXPECT_EQ(image_info_.width, 2549u);
    EXPECT_EQ(image_info_.height, 3299u);
}

TEST_F(WprintMupdfTest, GetHdr_OverflowDimensions_ReturnsError) {
    // 9084pt x 9084pt at 300 DPI yields 37850x37850 pixels.
    // In 32-bit int, 37850 * 37850 * 3 = 4,297,867,500 bytes which wraps to ~2.9 MB.
    // Without the fix, malloc(2.9 MB) succeeds and returns OK, but allocates a
    // buffer that is too small.
    // With the fix, totalBytes > INT32_MAX returns ERROR (Fixed).
    g_mockWidth = 9084.0;
    g_mockHeight = 9084.0;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}

TEST_F(WprintMupdfTest, GetHdr_NegativeDimensions_ReturnsError) {
    g_mockWidth = -10.0;
    g_mockHeight = 792.0;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}

TEST_F(WprintMupdfTest, GetHdr_ZeroDimensions_ReturnsError) {
    g_mockWidth = 0.0;
    g_mockHeight = 0.0;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}

TEST_F(WprintMupdfTest, GetHdr_ExtremeResolution_ReturnsError) {
    // 3914 DPI on a 612pt x 792pt page yields 33269x43054 pixels.
    // In 32-bit int, 33269 * 43054 * 3 = 4,297,060,578 bytes which wraps to ~2.09 MB.
    // Without the fix, malloc(2.09 MB) succeeds and returns OK, but allocates a
    // buffer that is too small.
    // With the fix, totalBytes > INT32_MAX returns ERROR (Fixed).
    g_mockWidth = 612.0;
    g_mockHeight = 792.0;
    image_info_.pdf_render_resolution = 3914;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}

TEST_F(WprintMupdfTest, GetHdr_NegativeResolution_ReturnsError) {
    g_mockWidth = 612.0;
    g_mockHeight = 792.0;
    image_info_.pdf_render_resolution = -300;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}

TEST_F(WprintMupdfTest, GetHdr_NaNDimensions_ReturnsError) {
    g_mockWidth = std::numeric_limits<double>::quiet_NaN();
    g_mockHeight = 792.0;

    status_t result = plugin_->get_hdr(&image_info_);
    EXPECT_EQ(result, ERROR);
}
