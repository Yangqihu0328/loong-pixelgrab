// Copyright 2026 The loong-pixelgrab Authors

#ifndef PIXELGRAB_ANNOTATION_ANNOTATION_SESSION_H_
#define PIXELGRAB_ANNOTATION_ANNOTATION_SESSION_H_

#include <memory>
#include <vector>

#include "annotation/annotation_renderer.h"
#include "annotation/shape.h"
#include "core/image.h"

namespace pixelgrab {
namespace internal {

/// Command for undo/redo tracking.
struct AnnotationCommand {
  enum class Type { kAdd, kRemove };
  Type type;
  int shape_id;
  std::unique_ptr<Shape> shape_data;  // Saved shape for undo.
};

/// Manages an annotation session: maintains a list of shapes on a base image,
/// supports undo/redo, and renders the composited result.
class AnnotationSession {
 public:
  AnnotationSession(std::unique_ptr<Image> base_image,
                    std::unique_ptr<AnnotationRenderer> renderer);
  ~AnnotationSession();

  // Non-copyable.
  AnnotationSession(const AnnotationSession&) = delete;
  AnnotationSession& operator=(const AnnotationSession&) = delete;

  // --- Shape addition (returns shape_id >= 0, or -1 on error) ---

  int AddShape(std::unique_ptr<Shape> shape);
  int RemoveShape(int shape_id);

  // --- Undo / Redo ---

  bool Undo();
  bool Redo();
  bool CanUndo() const { return !undo_stack_.empty(); }
  bool CanRedo() const { return !redo_stack_.empty(); }

  // --- Result access ---

  /// Get the current output image (base + all shapes).
  /// Valid until next AddShape/RemoveShape/Undo/Redo/Redraw.
  const Image* GetResult();

  /// Export a deep copy of the current result.
  std::unique_ptr<Image> Export();

 private:
  /// Re-render base + all shapes into output_image_.
  void Redraw();

  /// Apply mosaic effect directly to pixel data.
  static void ApplyMosaic(Image* image, int x, int y, int w, int h,
                           int block_size);

  /// Apply blur (3-pass box blur) directly to pixel data.
  static void ApplyBlur(Image* image, int x, int y, int w, int h, int radius);

  std::unique_ptr<Image> base_image_;    // Original (read-only).
  std::unique_ptr<Image> output_image_;  // Composited result.
  std::unique_ptr<AnnotationRenderer> renderer_;

  std::vector<std::unique_ptr<Shape>> shapes_;
  std::vector<AnnotationCommand> undo_stack_;
  std::vector<AnnotationCommand> redo_stack_;
  int next_id_ = 0;
  bool dirty_ = true;  // True if output needs redraw.
};

}  // namespace internal
}  // namespace pixelgrab

#endif  // PIXELGRAB_ANNOTATION_ANNOTATION_SESSION_H_
