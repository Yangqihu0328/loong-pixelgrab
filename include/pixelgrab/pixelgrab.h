// Copyright 2026 The loong-pixelgrab Authors
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.

#ifndef PIXELGRAB_PIXELGRAB_H_
#define PIXELGRAB_PIXELGRAB_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Export macro
// ---------------------------------------------------------------------------
#if defined(_WIN32)
#if defined(PIXELGRAB_BUILDING)
#define PIXELGRAB_API __declspec(dllexport)
#else
#define PIXELGRAB_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define PIXELGRAB_API __attribute__((visibility("default")))
#else
#define PIXELGRAB_API
#endif

// ---------------------------------------------------------------------------
// Version (auto-generated from CMakeLists.txt via configure_file)
// ---------------------------------------------------------------------------
#include "pixelgrab/version.h"

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------
//
// General rules:
//   - Each PixelGrabContext is independent; different contexts may be used
//     concurrently from different threads without external synchronization.
//   - Operations on the SAME context, annotation, pin window, or recorder
//     handle are NOT thread-safe.  The caller must serialize access to a
//     single handle (e.g. with a mutex) if it is shared across threads.
//   - PixelGrabImage objects are immutable after creation; reading image
//     properties and data is safe from multiple threads simultaneously.
//   - pixelgrab_set_log_level() and pixelgrab_set_log_callback() are
//     process-global and internally synchronized.
//   - pixelgrab_version_*() and pixelgrab_color_*() utility functions are
//     stateless and safe to call from any thread at any time.
//
// Recommended pattern:
//   Create one PixelGrabContext per thread, or protect a shared context
//   with a mutex.
//

// ---------------------------------------------------------------------------
// Opaque handles
// ---------------------------------------------------------------------------
typedef struct PixelGrabContext PixelGrabContext;
typedef struct PixelGrabImage PixelGrabImage;
typedef struct PixelGrabAnnotation PixelGrabAnnotation;
typedef struct PixelGrabPinWindow PixelGrabPinWindow;
typedef struct PixelGrabRecorder PixelGrabRecorder;

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

/// Cross-platform window identifier.
/// Windows: HWND cast to uint64_t
/// macOS:   CGWindowID stored as uint64_t
/// Linux:   X11 Window stored as uint64_t
typedef uint64_t PixelGrabWindowId;

/// Error codes returned by pixelgrab functions.
typedef enum PixelGrabError {
  kPixelGrabOk = 0,
  kPixelGrabErrorNotInitialized = -1,
  kPixelGrabErrorInvalidParam = -2,
  kPixelGrabErrorCaptureFailed = -3,
  kPixelGrabErrorPermissionDenied = -4,
  kPixelGrabErrorOutOfMemory = -5,
  kPixelGrabErrorNotSupported = -6,
  kPixelGrabErrorAnnotationFailed = -10,
  kPixelGrabErrorClipboardEmpty = -11,
  kPixelGrabErrorClipboardFormatUnsupported = -12,
  kPixelGrabErrorWindowCreateFailed = -13,
  kPixelGrabErrorNoElement = -14,
  kPixelGrabErrorHistoryEmpty = -15,
  kPixelGrabErrorRecordFailed = -16,          ///< Recording operation failed
  kPixelGrabErrorEncoderNotAvailable = -17,   ///< Video encoder not available
  kPixelGrabErrorRecordInProgress = -18,      ///< A recording is already active
  kPixelGrabErrorWatermarkFailed = -19,       ///< Watermark operation failed
  kPixelGrabErrorOcrFailed = -20,             ///< OCR recognition failed
  kPixelGrabErrorTranslateFailed = -21,      ///< Translation operation failed
  kPixelGrabErrorUnknown = -99,
} PixelGrabError;

/// Clipboard content format.
typedef enum PixelGrabClipboardFormat {
  kPixelGrabClipboardNone = 0,    ///< No recognized content
  kPixelGrabClipboardImage = 1,   ///< Bitmap image
  kPixelGrabClipboardText = 2,    ///< Plain text (UTF-8)
  kPixelGrabClipboardHtml = 3,    ///< HTML fragment
} PixelGrabClipboardFormat;

/// Log severity levels for the internal logging system.
typedef enum PixelGrabLogLevel {
  kPixelGrabLogTrace = 0,   ///< Very detailed diagnostic info
  kPixelGrabLogDebug = 1,   ///< Debug-level messages
  kPixelGrabLogInfo = 2,    ///< Informational messages (default)
  kPixelGrabLogWarn = 3,    ///< Warnings
  kPixelGrabLogError = 4,   ///< Errors
  kPixelGrabLogFatal = 5,   ///< Fatal / critical errors
} PixelGrabLogLevel;

/// User-defined log callback function type.
///
/// @param level  The severity level of the message.
/// @param message  Null-terminated UTF-8 log message.
/// @param userdata  The opaque pointer passed to pixelgrab_set_log_callback.
typedef void (*pixelgrab_log_callback_t)(PixelGrabLogLevel level,
                                         const char* message,
                                         void* userdata);

/// Pixel format of captured image data.
typedef enum PixelGrabPixelFormat {
  kPixelGrabFormatBgra8 = 0,   ///< B8G8R8A8 (default, most common)
  kPixelGrabFormatRgba8 = 1,   ///< R8G8B8A8
  kPixelGrabFormatNative = 2,  ///< Platform native format, zero conversion
} PixelGrabPixelFormat;

/// RGBA color value (8-bit per channel).
typedef struct PixelGrabColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} PixelGrabColor;

/// HSV color value.
typedef struct PixelGrabColorHsv {
  float h;  ///< Hue in degrees [0, 360)
  float s;  ///< Saturation [0, 1]
  float v;  ///< Value [0, 1]
} PixelGrabColorHsv;

/// DPI information for a display.
typedef struct PixelGrabDpiInfo {
  int screen_index;  ///< Screen index (0-based)
  float scale_x;     ///< Horizontal scale factor (1.0 = 96 DPI)
  float scale_y;     ///< Vertical scale factor
  int dpi_x;         ///< Horizontal DPI value
  int dpi_y;         ///< Vertical DPI value
} PixelGrabDpiInfo;

/// Shape drawing style for annotation tools.
typedef struct PixelGrabShapeStyle {
  uint32_t stroke_color;  ///< Stroke color in ARGB format (0xAARRGGBB)
  uint32_t fill_color;    ///< Fill color in ARGB (0 = no fill)
  float stroke_width;     ///< Stroke width in pixels
  int filled;             ///< Non-zero to enable fill
} PixelGrabShapeStyle;

/// Information about a pin window.
typedef struct PixelGrabPinInfo {
  int id;             ///< Pin window ID (manager-assigned)
  int x;              ///< Window position X (screen coordinates)
  int y;              ///< Window position Y (screen coordinates)
  int width;          ///< Window width in pixels
  int height;         ///< Window height in pixels
  float opacity;      ///< Opacity (0.0 = transparent, 1.0 = opaque)
  int is_visible;     ///< Non-zero if the window is visible
  int content_type;   ///< 0 = image, 1 = text
} PixelGrabPinInfo;

/// UI element bounding rectangle.
typedef struct PixelGrabElementRect {
  int x;              ///< Element left edge (screen coordinates)
  int y;              ///< Element top edge
  int width;          ///< Element width in pixels
  int height;         ///< Element height in pixels
  char name[256];     ///< Element name/label (UTF-8)
  char role[64];      ///< Element role (e.g. "button", "edit", "window")
} PixelGrabElementRect;

/// Capture history entry.
typedef struct PixelGrabHistoryEntry {
  int id;              ///< Unique history entry ID
  int region_x;        ///< Captured region X
  int region_y;        ///< Captured region Y
  int region_width;    ///< Captured region width
  int region_height;   ///< Captured region height
  int64_t timestamp;   ///< Unix timestamp (seconds)
} PixelGrabHistoryEntry;

/// Information about a display screen / monitor.
typedef struct PixelGrabScreenInfo {
  int index;       ///< Screen index (0-based)
  int x;           ///< Left edge X in virtual screen coordinates
  int y;           ///< Top edge Y in virtual screen coordinates
  int width;       ///< Width in pixels
  int height;      ///< Height in pixels
  int is_primary;  ///< Non-zero if this is the primary screen
  char name[128];  ///< Display name (UTF-8, null-terminated)
} PixelGrabScreenInfo;

/// Information about a window.
typedef struct PixelGrabWindowInfo {
  PixelGrabWindowId id;    ///< Platform window identifier
  int x;                   ///< Window position X
  int y;                   ///< Window position Y
  int width;               ///< Window width in pixels
  int height;              ///< Window height in pixels
  int is_visible;          ///< Non-zero if the window is visible
  char title[256];         ///< Window title (UTF-8, null-terminated)
  char process_name[128];  ///< Owner process name (UTF-8, null-terminated)
} PixelGrabWindowInfo;

/// Recording state.
typedef enum PixelGrabRecordState {
  kPixelGrabRecordIdle = 0,       ///< Not started
  kPixelGrabRecordRecording = 1,  ///< Actively recording
  kPixelGrabRecordPaused = 2,     ///< Paused
  kPixelGrabRecordStopped = 3,    ///< Stopped / finalized
} PixelGrabRecordState;

/// Watermark position presets.
typedef enum PixelGrabWatermarkPosition {
  kPixelGrabWatermarkTopLeft = 0,
  kPixelGrabWatermarkTopRight = 1,
  kPixelGrabWatermarkBottomLeft = 2,
  kPixelGrabWatermarkBottomRight = 3,
  kPixelGrabWatermarkCenter = 4,
  kPixelGrabWatermarkCustom = 5,  ///< Use x, y for custom position
} PixelGrabWatermarkPosition;

/// Text watermark configuration.
typedef struct PixelGrabTextWatermarkConfig {
  const char* text;       ///< Watermark text (UTF-8, must not be NULL)
  const char* font_name;  ///< Font family name (NULL = system default)
  int font_size;          ///< Font size in points (0 = default 16)
  uint32_t color;         ///< Text color in ARGB format (0xAARRGGBB)
  PixelGrabWatermarkPosition position;  ///< Position preset
  int x;                  ///< Custom X (only used when position == Custom)
  int y;                  ///< Custom Y (only used when position == Custom)
  int margin;             ///< Margin from edges in pixels (0 = default 10)
  float rotation;         ///< Text rotation in degrees (0 = horizontal)
} PixelGrabTextWatermarkConfig;

/// Audio source type for recording.
typedef enum PixelGrabAudioSource {
  kPixelGrabAudioNone = 0,        ///< No audio recording (default)
  kPixelGrabAudioMicrophone = 1,  ///< Microphone input
  kPixelGrabAudioSystem = 2,      ///< System audio (loopback)
  kPixelGrabAudioBoth = 3,        ///< Both microphone and system audio
} PixelGrabAudioSource;

/// Audio device information.
typedef struct PixelGrabAudioDeviceInfo {
  char id[256];       ///< Platform device ID (UTF-8)
  char name[256];     ///< Human-readable device name (UTF-8)
  int is_default;     ///< Non-zero if this is the default device
  int is_input;       ///< 1 = microphone, 0 = system audio (loopback)
} PixelGrabAudioDeviceInfo;

/// Screen recording configuration.
typedef struct PixelGrabRecordConfig {
  const char* output_path;  ///< Output file path (UTF-8, .mp4)
  int region_x;             ///< Recording region left edge
  int region_y;             ///< Recording region top edge
  int region_width;         ///< Recording region width (0 = primary screen)
  int region_height;        ///< Recording region height (0 = primary screen)
  int fps;                  ///< Frame rate (0 = default 30, range 1-60)
  int bitrate;              ///< Bitrate in bps (0 = default 4000000 = 4Mbps)
  const PixelGrabTextWatermarkConfig* watermark;  ///< System watermark (NULL = none)
  const PixelGrabTextWatermarkConfig* user_watermark;  ///< User watermark rendered
                            ///< at top-left, top-right, and bottom-left corners.
                            ///< NULL = no user watermark.
  int auto_capture;         ///< Non-zero: internal capture thread (auto mode).
                            ///< 0 (default): manual mode — caller feeds frames
                            ///< via pixelgrab_recorder_write_frame().
  PixelGrabAudioSource audio_source;  ///< Audio source (0 = no audio)
  const char* audio_device_id;        ///< Audio device ID (NULL = default)
  int audio_sample_rate;              ///< Audio sample rate (0 = default 44100)
  int gpu_hint;                       ///< GPU acceleration hint:
                                      ///<   0 = auto (try GPU, fall back to CPU)
                                      ///<       [default, zero-initialized]
                                      ///<   1 = prefer GPU (report error if
                                      ///<       unavailable)
                                      ///<  -1 = force CPU (never use GPU)
} PixelGrabRecordConfig;

// ---------------------------------------------------------------------------
// Context management
// ---------------------------------------------------------------------------

/// Create a new pixelgrab context. The context initializes the platform
/// capture backend. The caller must destroy it with pixelgrab_context_destroy().
///
/// @return A new context, or NULL on failure.
PIXELGRAB_API PixelGrabContext* pixelgrab_context_create(void);

/// Destroy a pixelgrab context and release all associated resources.
///
/// @param ctx  Context to destroy. NULL is safely ignored.
PIXELGRAB_API void pixelgrab_context_destroy(PixelGrabContext* ctx);

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

/// Get the error code from the last failed operation on this context.
PIXELGRAB_API PixelGrabError pixelgrab_get_last_error(
    const PixelGrabContext* ctx);

/// Get a human-readable error message for the last failed operation.
///
/// Lifetime: The returned string is valid until the next API call on the
/// same context.  Copy the string if you need it beyond that.
///
/// @return UTF-8 error message. Never returns NULL.
PIXELGRAB_API const char* pixelgrab_get_last_error_message(
    const PixelGrabContext* ctx);

// ---------------------------------------------------------------------------
// Screen / monitor information
// ---------------------------------------------------------------------------

/// Get the number of connected screens / monitors.
///
/// @param ctx  Initialized context.
/// @return Number of screens, or -1 on error.
PIXELGRAB_API int pixelgrab_get_screen_count(PixelGrabContext* ctx);

/// Get information about a specific screen.
///
/// @param ctx           Initialized context.
/// @param screen_index  Zero-based screen index.
/// @param out_info      Output structure (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_get_screen_info(
    PixelGrabContext* ctx, int screen_index, PixelGrabScreenInfo* out_info);

// ---------------------------------------------------------------------------
// Capture operations
// ---------------------------------------------------------------------------

/// Capture the entire contents of a screen.
///
/// @param ctx           Initialized context.
/// @param screen_index  Zero-based screen index.
/// @return Captured image, or NULL on failure. Caller must free with
///         pixelgrab_image_destroy().
PIXELGRAB_API PixelGrabImage* pixelgrab_capture_screen(PixelGrabContext* ctx,
                                                       int screen_index);

/// Capture a rectangular region in virtual screen coordinates.
///
/// @param ctx     Initialized context.
/// @param x       Left edge of the region.
/// @param y       Top edge of the region.
/// @param width   Width of the region in pixels.
/// @param height  Height of the region in pixels.
/// @return Captured image, or NULL on failure.
PIXELGRAB_API PixelGrabImage* pixelgrab_capture_region(PixelGrabContext* ctx,
                                                       int x, int y, int width,
                                                       int height);

/// Capture the contents of a specific window.
///
/// @param ctx        Initialized context.
/// @param window_id  Platform window identifier.
/// @return Captured image, or NULL on failure.
PIXELGRAB_API PixelGrabImage* pixelgrab_capture_window(
    PixelGrabContext* ctx, PixelGrabWindowId window_id);

// ---------------------------------------------------------------------------
// Window enumeration
// ---------------------------------------------------------------------------

/// Enumerate visible top-level windows.
///
/// @param ctx          Initialized context.
/// @param out_windows  Caller-allocated array to receive window info.
/// @param max_count    Maximum number of entries in out_windows.
/// @return Number of windows written, or -1 on error.
PIXELGRAB_API int pixelgrab_enumerate_windows(PixelGrabContext* ctx,
                                              PixelGrabWindowInfo* out_windows,
                                              int max_count);

// ---------------------------------------------------------------------------
// Image accessors
// ---------------------------------------------------------------------------

/// Get image width in pixels.
PIXELGRAB_API int pixelgrab_image_get_width(const PixelGrabImage* image);

/// Get image height in pixels.
PIXELGRAB_API int pixelgrab_image_get_height(const PixelGrabImage* image);

/// Get image row stride in bytes.
PIXELGRAB_API int pixelgrab_image_get_stride(const PixelGrabImage* image);

/// Get the pixel format of the image.
PIXELGRAB_API PixelGrabPixelFormat pixelgrab_image_get_format(
    const PixelGrabImage* image);

/// Get a pointer to the raw pixel data.
/// The returned pointer is valid for the lifetime of the PixelGrabImage.
/// Since images are immutable, concurrent reads from multiple threads are safe.
PIXELGRAB_API const uint8_t* pixelgrab_image_get_data(
    const PixelGrabImage* image);

/// Get the total size of the pixel data in bytes.
PIXELGRAB_API size_t pixelgrab_image_get_data_size(
    const PixelGrabImage* image);

/// Destroy a captured image and free its memory.
///
/// @param image  Image to destroy. NULL is safely ignored.
PIXELGRAB_API void pixelgrab_image_destroy(PixelGrabImage* image);

// ---------------------------------------------------------------------------
// DPI awareness
// ---------------------------------------------------------------------------

/// Enable system DPI awareness. Call once after context creation.
/// Windows: Sets per-monitor DPI awareness (V2).
/// macOS:   No-op (Retina is automatic).
/// Linux:   Reads Xft.dpi / GDK_SCALE.
///
/// @param ctx  Initialized context.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_enable_dpi_awareness(
    PixelGrabContext* ctx);

/// Get DPI information for a specific screen.
///
/// @param ctx           Initialized context.
/// @param screen_index  Zero-based screen index.
/// @param out_info      Output structure (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_get_dpi_info(
    PixelGrabContext* ctx, int screen_index, PixelGrabDpiInfo* out_info);

/// Convert logical coordinates to physical pixel coordinates.
///
/// @param ctx            Initialized context.
/// @param screen_index   Zero-based screen index.
/// @param logical_x      Logical X coordinate.
/// @param logical_y      Logical Y coordinate.
/// @param out_physical_x Output physical X (caller-allocated).
/// @param out_physical_y Output physical Y (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_logical_to_physical(
    PixelGrabContext* ctx, int screen_index, int logical_x, int logical_y,
    int* out_physical_x, int* out_physical_y);

/// Convert physical pixel coordinates to logical coordinates.
PIXELGRAB_API PixelGrabError pixelgrab_physical_to_logical(
    PixelGrabContext* ctx, int screen_index, int physical_x, int physical_y,
    int* out_logical_x, int* out_logical_y);

// ---------------------------------------------------------------------------
// Color picker
// ---------------------------------------------------------------------------

/// Pick the pixel color at the given virtual screen coordinates.
///
/// @param ctx        Initialized context.
/// @param x          X coordinate in virtual screen space.
/// @param y          Y coordinate in virtual screen space.
/// @param out_color  Output color (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_pick_color(PixelGrabContext* ctx, int x,
                                                  int y,
                                                  PixelGrabColor* out_color);

/// Convert an RGB color to HSV color space.
PIXELGRAB_API void pixelgrab_color_rgb_to_hsv(const PixelGrabColor* rgb,
                                              PixelGrabColorHsv* out_hsv);

/// Convert an HSV color to RGB color space.
PIXELGRAB_API void pixelgrab_color_hsv_to_rgb(const PixelGrabColorHsv* hsv,
                                              PixelGrabColor* out_rgb);

/// Convert an RGB color to a hex string (e.g. "#RRGGBB" or "#RRGGBBAA").
///
/// @param color          Input color.
/// @param buf            Output buffer (caller-allocated, at least 10 bytes).
/// @param buf_size       Size of buf in bytes.
/// @param include_alpha  Non-zero to include alpha channel (#RRGGBBAA).
PIXELGRAB_API void pixelgrab_color_to_hex(const PixelGrabColor* color,
                                          char* buf, int buf_size,
                                          int include_alpha);

/// Parse a hex color string to RGB.
/// Supports "#RGB", "#RRGGBB", and "#RRGGBBAA" formats.
///
/// @param hex        Input hex string (e.g. "#FF0000").
/// @param out_color  Output color (caller-allocated).
/// @return kPixelGrabOk on success, kPixelGrabErrorInvalidParam on bad format.
PIXELGRAB_API PixelGrabError pixelgrab_color_from_hex(
    const char* hex, PixelGrabColor* out_color);

/// Capture a magnified region around a point on screen.
///
/// @param ctx           Initialized context.
/// @param x             Center X in virtual screen coordinates.
/// @param y             Center Y in virtual screen coordinates.
/// @param radius        Sampling radius in pixels.
/// @param magnification Magnification factor (2-32).
/// @return Magnified image, or NULL on failure. Output size =
///         (radius*2+1) * magnification. Caller must free with
///         pixelgrab_image_destroy().
PIXELGRAB_API PixelGrabImage* pixelgrab_get_magnifier(PixelGrabContext* ctx,
                                                      int x, int y,
                                                      int radius,
                                                      int magnification);

// ---------------------------------------------------------------------------
// Annotation engine
// ---------------------------------------------------------------------------

/// Create an annotation session on top of a base image.
/// The engine works on a copy; the original image is not modified.
///
/// @param ctx         Initialized context.
/// @param base_image  Source image to annotate (not modified).
/// @return Annotation session, or NULL on failure.
PIXELGRAB_API PixelGrabAnnotation* pixelgrab_annotation_create(
    PixelGrabContext* ctx, const PixelGrabImage* base_image);

/// Destroy an annotation session and all its shapes.
PIXELGRAB_API void pixelgrab_annotation_destroy(PixelGrabAnnotation* ann);

// --- Shape addition (returns shape_id >= 0, or -1 on failure) ---

/// Add a rectangle shape.
PIXELGRAB_API int pixelgrab_annotation_add_rect(
    PixelGrabAnnotation* ann, int x, int y, int width, int height,
    const PixelGrabShapeStyle* style);

/// Add an ellipse shape.
PIXELGRAB_API int pixelgrab_annotation_add_ellipse(
    PixelGrabAnnotation* ann, int cx, int cy, int rx, int ry,
    const PixelGrabShapeStyle* style);

/// Add a line shape.
PIXELGRAB_API int pixelgrab_annotation_add_line(PixelGrabAnnotation* ann,
                                                int x1, int y1, int x2,
                                                int y2,
                                                const PixelGrabShapeStyle* style);

/// Add an arrow shape.
PIXELGRAB_API int pixelgrab_annotation_add_arrow(
    PixelGrabAnnotation* ann, int x1, int y1, int x2, int y2, float head_size,
    const PixelGrabShapeStyle* style);

/// Add a freehand pencil stroke.
/// @param points       Interleaved x,y array: [x0,y0,x1,y1,...].
/// @param point_count  Number of points (NOT array length).
PIXELGRAB_API int pixelgrab_annotation_add_pencil(
    PixelGrabAnnotation* ann, const int* points, int point_count,
    const PixelGrabShapeStyle* style);

/// Add a text label.
PIXELGRAB_API int pixelgrab_annotation_add_text(PixelGrabAnnotation* ann,
                                                int x, int y,
                                                const char* text,
                                                const char* font_name,
                                                int font_size, uint32_t color);

/// Apply mosaic (pixelation) effect to a region.
/// @param block_size  Mosaic block size in pixels.
PIXELGRAB_API int pixelgrab_annotation_add_mosaic(PixelGrabAnnotation* ann,
                                                  int x, int y, int width,
                                                  int height, int block_size);

/// Apply gaussian blur effect to a region.
/// @param radius  Blur radius.
PIXELGRAB_API int pixelgrab_annotation_add_blur(PixelGrabAnnotation* ann,
                                                int x, int y, int width,
                                                int height, int radius);

// --- Shape operations ---

/// Remove a shape by its ID.
PIXELGRAB_API PixelGrabError pixelgrab_annotation_remove_shape(
    PixelGrabAnnotation* ann, int shape_id);

// --- Undo / Redo ---

/// Undo the last annotation operation.
PIXELGRAB_API PixelGrabError pixelgrab_annotation_undo(
    PixelGrabAnnotation* ann);

/// Redo the last undone operation.
PIXELGRAB_API PixelGrabError pixelgrab_annotation_redo(
    PixelGrabAnnotation* ann);

/// Check if undo is available. Returns non-zero if yes.
PIXELGRAB_API int pixelgrab_annotation_can_undo(
    const PixelGrabAnnotation* ann);

/// Check if redo is available. Returns non-zero if yes.
PIXELGRAB_API int pixelgrab_annotation_can_redo(
    const PixelGrabAnnotation* ann);

// --- Result ---

/// Get the current annotated result image (base + all shapes).
///
/// Lifetime: The returned pointer is owned by the annotation session.
/// Do NOT call pixelgrab_image_destroy() on it.  The pointer is invalidated
/// by ANY subsequent mutation (add shape, remove, undo, redo) or when the
/// session is destroyed.  Copy the image data if you need it beyond that.
PIXELGRAB_API const PixelGrabImage* pixelgrab_annotation_get_result(
    PixelGrabAnnotation* ann);

/// Export the annotated result as an independent image.
/// Caller must free with pixelgrab_image_destroy().
PIXELGRAB_API PixelGrabImage* pixelgrab_annotation_export(
    PixelGrabAnnotation* ann);

// ---------------------------------------------------------------------------
// UI Element Detection & Smart Snapping
// ---------------------------------------------------------------------------

/// Detect the most precise UI element at screen coordinates (x, y).
/// Returns kPixelGrabOk on success, kPixelGrabErrorNoElement if nothing found.
PIXELGRAB_API PixelGrabError pixelgrab_detect_element(
    PixelGrabContext* ctx, int x, int y, PixelGrabElementRect* out_rect);

/// Detect all nested UI elements at screen coordinates (x, y).
/// out_rects: caller-allocated array; max_count: array capacity.
/// Returns the number of elements written, or -1 on error.
PIXELGRAB_API int pixelgrab_detect_elements(
    PixelGrabContext* ctx, int x, int y,
    PixelGrabElementRect* out_rects, int max_count);

/// Snap to the nearest UI element boundary within snap_distance pixels.
/// Returns kPixelGrabOk if snapped, kPixelGrabErrorNoElement if nothing nearby.
PIXELGRAB_API PixelGrabError pixelgrab_snap_to_element(
    PixelGrabContext* ctx, int x, int y, int snap_distance,
    PixelGrabElementRect* out_rect);

// ---------------------------------------------------------------------------
// Capture History & Region Recall
// ---------------------------------------------------------------------------

/// Get the number of history entries.
PIXELGRAB_API int pixelgrab_history_count(PixelGrabContext* ctx);

/// Get information about a specific history entry.
/// index: 0 = most recent, increasing = older.
PIXELGRAB_API PixelGrabError pixelgrab_history_get_entry(
    PixelGrabContext* ctx, int index, PixelGrabHistoryEntry* out_entry);

/// Recapture the region from a specific history entry by its ID.
/// Caller must free with pixelgrab_image_destroy().
PIXELGRAB_API PixelGrabImage* pixelgrab_history_recapture(
    PixelGrabContext* ctx, int history_id);

/// Recapture the most recent capture region.
/// Caller must free with pixelgrab_image_destroy().
PIXELGRAB_API PixelGrabImage* pixelgrab_recapture_last(
    PixelGrabContext* ctx);

/// Clear all capture history entries.
PIXELGRAB_API void pixelgrab_history_clear(PixelGrabContext* ctx);

/// Set the maximum number of history entries (default 50).
PIXELGRAB_API void pixelgrab_history_set_max_count(
    PixelGrabContext* ctx, int max_count);

// ---------------------------------------------------------------------------
// Pin Windows (Floating Overlay)
// ---------------------------------------------------------------------------

/// Pin an image as a floating topmost window.
/// x, y: initial screen position.
/// Returns a pin window handle, or NULL on failure.
PIXELGRAB_API PixelGrabPinWindow* pixelgrab_pin_image(
    PixelGrabContext* ctx, const PixelGrabImage* image, int x, int y);

/// Pin text as a floating topmost window.
PIXELGRAB_API PixelGrabPinWindow* pixelgrab_pin_text(
    PixelGrabContext* ctx, const char* text, int x, int y);

/// Pin clipboard content as a floating topmost window.
PIXELGRAB_API PixelGrabPinWindow* pixelgrab_pin_clipboard(
    PixelGrabContext* ctx, int x, int y);

/// Destroy a single pin window.
PIXELGRAB_API void pixelgrab_pin_destroy(PixelGrabPinWindow* pin);

/// Set pin window opacity (0.0 = fully transparent, 1.0 = fully opaque).
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_opacity(
    PixelGrabPinWindow* pin, float opacity);

/// Get pin window opacity.
PIXELGRAB_API float pixelgrab_pin_get_opacity(
    const PixelGrabPinWindow* pin);

/// Move pin window to the specified screen position.
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_position(
    PixelGrabPinWindow* pin, int x, int y);

/// Resize pin window.
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_size(
    PixelGrabPinWindow* pin, int width, int height);

/// Show or hide pin window (visible: non-zero = show, 0 = hide).
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_visible(
    PixelGrabPinWindow* pin, int visible);

/// Process system events for all pin windows (call in your message loop).
/// Returns the number of currently active pin windows.
PIXELGRAB_API int pixelgrab_pin_process_events(PixelGrabContext* ctx);

/// Get the number of currently active pin windows.
PIXELGRAB_API int pixelgrab_pin_count(PixelGrabContext* ctx);

/// Destroy all pin windows.
PIXELGRAB_API void pixelgrab_pin_destroy_all(PixelGrabContext* ctx);

// ---------------------------------------------------------------------------
// Pin Window — Enumeration, Content Access & Multi-Pin Operations
// ---------------------------------------------------------------------------

/// Enumerate all active pin window IDs.
///
/// @param ctx        Initialized context.
/// @param out_ids    Caller-allocated array to receive pin IDs.
/// @param max_count  Capacity of out_ids array.
/// @return Number of IDs written, or -1 on error.
PIXELGRAB_API int pixelgrab_pin_enumerate(PixelGrabContext* ctx,
                                          int* out_ids, int max_count);

/// Get information about a specific pin window.
///
/// @param pin       Pin window handle.
/// @param out_info  Output structure (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_pin_get_info(
    PixelGrabPinWindow* pin, PixelGrabPinInfo* out_info);

/// Get a copy of the pin window's image content.
/// Returns NULL for text-type pins or on failure.
/// Caller must free with pixelgrab_image_destroy().
///
/// @param pin  Pin window handle.
/// @return Copied image, or NULL.
PIXELGRAB_API PixelGrabImage* pixelgrab_pin_get_image(
    PixelGrabPinWindow* pin);

/// Replace the pin window's image content.
/// Only valid for image-type pins.
///
/// @param pin    Pin window handle.
/// @param image  New image content (not retained after call returns).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_image(
    PixelGrabPinWindow* pin, const PixelGrabImage* image);

/// Show or hide all pin windows at once.
///
/// @param ctx      Initialized context.
/// @param visible  Non-zero = show all, 0 = hide all.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_pin_set_visible_all(
    PixelGrabContext* ctx, int visible);

/// Duplicate a pin window with an offset.
/// Creates a new pin with the same content at (original_x + offset_x,
/// original_y + offset_y). Only image-type pins are supported.
/// Caller must free with pixelgrab_pin_destroy().
///
/// @param pin       Source pin window handle.
/// @param offset_x  Horizontal offset from the source position.
/// @param offset_y  Vertical offset from the source position.
/// @return New pin window handle, or NULL on failure.
PIXELGRAB_API PixelGrabPinWindow* pixelgrab_pin_duplicate(
    PixelGrabPinWindow* pin, int offset_x, int offset_y);

/// Get the native window handle for a pin window.
/// On Windows this returns HWND, on macOS NSWindow*, on Linux the X11 Window.
/// Returns NULL if the pin is invalid.
///
/// @param pin  Pin window handle.
/// @return Native window handle (cast to platform type), or NULL.
PIXELGRAB_API void* pixelgrab_pin_get_native_handle(PixelGrabPinWindow* pin);

/// Capture the entire screen with all pin windows temporarily hidden.
/// Equivalent to: hide all pins -> capture screen -> show all pins.
/// Caller must free with pixelgrab_image_destroy().
///
/// @param ctx           Initialized context.
/// @param screen_index  Zero-based screen index.
/// @return Captured image, or NULL on failure.
PIXELGRAB_API PixelGrabImage* pixelgrab_capture_screen_exclude_pins(
    PixelGrabContext* ctx, int screen_index);

/// Capture a region with all pin windows temporarily hidden.
/// Equivalent to: hide all pins -> capture region -> show all pins.
/// Caller must free with pixelgrab_image_destroy().
///
/// @param ctx     Initialized context.
/// @param x       Left edge of the region.
/// @param y       Top edge of the region.
/// @param width   Width of the region in pixels.
/// @param height  Height of the region in pixels.
/// @return Captured image, or NULL on failure.
PIXELGRAB_API PixelGrabImage* pixelgrab_capture_region_exclude_pins(
    PixelGrabContext* ctx, int x, int y, int width, int height);

// ---------------------------------------------------------------------------
// Clipboard Reading
// ---------------------------------------------------------------------------

/// Get the current clipboard content format.
PIXELGRAB_API PixelGrabClipboardFormat pixelgrab_clipboard_get_format(
    PixelGrabContext* ctx);

/// Read an image from the clipboard. Caller must free with pixelgrab_image_destroy().
/// Returns NULL if clipboard does not contain an image.
PIXELGRAB_API PixelGrabImage* pixelgrab_clipboard_get_image(
    PixelGrabContext* ctx);

/// Read text from the clipboard. Caller must free with pixelgrab_free_string().
/// Returns NULL if clipboard does not contain text.
PIXELGRAB_API char* pixelgrab_clipboard_get_text(PixelGrabContext* ctx);

/// Free a string allocated by the library.
PIXELGRAB_API void pixelgrab_free_string(char* str);

// ---------------------------------------------------------------------------
// Screen Recording
// ---------------------------------------------------------------------------

/// Check if screen recording is supported on this platform.
/// @return Non-zero if supported.
PIXELGRAB_API int pixelgrab_recorder_is_supported(PixelGrabContext* ctx);

/// Create a screen recorder.
/// The config pointer is not retained after this call returns.
///
/// @param ctx     Initialized context.
/// @param config  Recording configuration.
/// @return Recorder handle, or NULL on failure. Caller must free with
///         pixelgrab_recorder_destroy().
PIXELGRAB_API PixelGrabRecorder* pixelgrab_recorder_create(
    PixelGrabContext* ctx, const PixelGrabRecordConfig* config);

/// Destroy a recorder and release all resources.
/// If recording is in progress, it is stopped first.
///
/// @param recorder  Recorder to destroy. NULL is safely ignored.
PIXELGRAB_API void pixelgrab_recorder_destroy(PixelGrabRecorder* recorder);

/// Start recording.
PIXELGRAB_API PixelGrabError pixelgrab_recorder_start(
    PixelGrabRecorder* recorder);

/// Pause recording. Frames are not captured while paused.
PIXELGRAB_API PixelGrabError pixelgrab_recorder_pause(
    PixelGrabRecorder* recorder);

/// Resume a paused recording.
PIXELGRAB_API PixelGrabError pixelgrab_recorder_resume(
    PixelGrabRecorder* recorder);

/// Stop recording and finalize the output file.
PIXELGRAB_API PixelGrabError pixelgrab_recorder_stop(
    PixelGrabRecorder* recorder);

/// Get the current recording state.
PIXELGRAB_API PixelGrabRecordState pixelgrab_recorder_get_state(
    const PixelGrabRecorder* recorder);

/// Get the total recorded duration in milliseconds.
PIXELGRAB_API int64_t pixelgrab_recorder_get_duration_ms(
    const PixelGrabRecorder* recorder);

/// Write a single video frame to the recorder (manual mode only).
///
/// This function is intended for use when auto_capture is 0 (manual mode).
/// The image must be in BGRA8 format with dimensions matching the recording
/// region. When auto_capture is enabled, this returns kPixelGrabErrorRecordFailed.
///
/// @param recorder  Active recorder (must be in Recording state).
/// @param frame     Frame image to encode. Not modified or retained.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_recorder_write_frame(
    PixelGrabRecorder* recorder, const PixelGrabImage* frame);

// ---------------------------------------------------------------------------
// Watermark
// ---------------------------------------------------------------------------

/// Check if watermark rendering is supported on this platform.
/// @return Non-zero if supported.
PIXELGRAB_API int pixelgrab_watermark_is_supported(PixelGrabContext* ctx);

/// Apply a text watermark to an image. The image is modified in-place.
///
/// @param ctx     Initialized context.
/// @param image   Target image (will be modified).
/// @param config  Text watermark configuration.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_watermark_apply_text(
    PixelGrabContext* ctx, PixelGrabImage* image,
    const PixelGrabTextWatermarkConfig* config);

/// Apply an image watermark (overlay) onto a target image.
///
/// @param ctx        Initialized context.
/// @param image      Target image (will be modified).
/// @param watermark  Source watermark image (not modified).
/// @param x          Watermark X position on target.
/// @param y          Watermark Y position on target.
/// @param opacity    Opacity (0.0 = transparent, 1.0 = fully opaque).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_watermark_apply_image(
    PixelGrabContext* ctx, PixelGrabImage* image,
    const PixelGrabImage* watermark, int x, int y, float opacity);

// ---------------------------------------------------------------------------
// Audio Device Query
// ---------------------------------------------------------------------------

/// Check if audio recording is supported on this platform.
/// @return Non-zero if supported.
PIXELGRAB_API int pixelgrab_audio_is_supported(PixelGrabContext* ctx);

/// Enumerate available audio devices.
///
/// @param ctx          Initialized context.
/// @param out_devices  Caller-allocated array to receive device info.
/// @param max_count    Maximum number of entries in out_devices.
/// @return Number of devices written, or -1 on error.
PIXELGRAB_API int pixelgrab_audio_enumerate_devices(
    PixelGrabContext* ctx, PixelGrabAudioDeviceInfo* out_devices,
    int max_count);

/// Get the default audio device.
///
/// @param ctx         Initialized context.
/// @param is_input    Non-zero for default microphone, 0 for default system
///                    audio (loopback).
/// @param out_device  Output device info (caller-allocated).
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_audio_get_default_device(
    PixelGrabContext* ctx, int is_input,
    PixelGrabAudioDeviceInfo* out_device);

// ---------------------------------------------------------------------------
// OCR (Optical Character Recognition)
// ---------------------------------------------------------------------------

/// Check if OCR is supported on this platform.
/// @return Non-zero if supported.
PIXELGRAB_API int pixelgrab_ocr_is_supported(PixelGrabContext* ctx);

/// Recognize text in an image using OCR.
///
/// @param ctx       Initialized context.
/// @param image     Source image to recognize text from.
/// @param language  BCP-47 language tag (e.g. "zh-Hans-CN", "en-US").
///                  Pass NULL for auto-detection from user profile.
/// @param out_text  On success, receives a newly allocated UTF-8 string.
///                  Caller must free with pixelgrab_free_string().
///                  Set to NULL on failure.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_ocr_recognize(
    PixelGrabContext* ctx, const PixelGrabImage* image,
    const char* language, char** out_text);

// ---------------------------------------------------------------------------
// Translation
// ---------------------------------------------------------------------------

/// Configure the translation provider credentials.
/// @param ctx         Initialized context.
/// @param provider    Provider name (e.g. "baidu"). Pass NULL for default.
/// @param app_id      Application ID from the translation service.
/// @param secret_key  Secret key from the translation service.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_translate_set_config(
    PixelGrabContext* ctx, const char* provider, const char* app_id,
    const char* secret_key);

/// Check if translation is supported (i.e. credentials are configured).
/// @return Non-zero if supported.
PIXELGRAB_API int pixelgrab_translate_is_supported(PixelGrabContext* ctx);

/// Translate text from one language to another.
///
/// @param ctx             Initialized context.
/// @param text            UTF-8 source text to translate.
/// @param source_lang     Source language code (e.g. "en", "zh", "auto").
///                        Pass NULL or "auto" for automatic detection.
/// @param target_lang     Target language code (e.g. "zh", "en").
/// @param out_translated  On success, receives a newly allocated UTF-8 string.
///                        Caller must free with pixelgrab_free_string().
///                        Set to NULL on failure.
/// @return kPixelGrabOk on success.
PIXELGRAB_API PixelGrabError pixelgrab_translate_text(
    PixelGrabContext* ctx, const char* text, const char* source_lang,
    const char* target_lang, char** out_translated);

// ---------------------------------------------------------------------------
// Version information
// ---------------------------------------------------------------------------

/// Get the library version as a string (e.g. "1.0.0").
PIXELGRAB_API const char* pixelgrab_version_string(void);

/// Get the major version number.
PIXELGRAB_API int pixelgrab_version_major(void);

/// Get the minor version number.
PIXELGRAB_API int pixelgrab_version_minor(void);

/// Get the patch version number.
PIXELGRAB_API int pixelgrab_version_patch(void);

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

/// Set the minimum log level. Messages below this level are discarded.
/// Default level is kPixelGrabLogInfo.
PIXELGRAB_API void pixelgrab_set_log_level(PixelGrabLogLevel level);

/// Set a user-defined log callback.
///
/// When a callback is registered, all log messages (at or above the current
/// level) are forwarded to the callback in addition to the default stderr
/// output.  Pass NULL as @p callback to unregister a previous callback.
///
/// @param callback  The callback function, or NULL to unregister.
/// @param userdata  Opaque pointer passed through to the callback.
PIXELGRAB_API void pixelgrab_set_log_callback(
    pixelgrab_log_callback_t callback, void* userdata);

/// Emit a log message at the given level through the pixelgrab logging system.
///
/// This can be used by host applications that want their own messages to flow
/// through the same logging pipeline.
///
/// @param level    Severity level.
/// @param message  Null-terminated UTF-8 string.
PIXELGRAB_API void pixelgrab_log(PixelGrabLogLevel level,
                                 const char* message);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // PIXELGRAB_PIXELGRAB_H_
