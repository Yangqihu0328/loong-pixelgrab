// Copyright 2026 The PixelGrab Authors
// Canvas window, toolbar, text dialog, and annotation helpers.

#ifndef PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_ANNOTATION_MANAGER_H_
#define PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_ANNOTATION_MANAGER_H_

#ifdef _WIN32

#include "platform/windows/win_app_defs.h"

class AnnotationManager {
 public:
  static LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK ToolbarWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK TextDlgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK ColorPanelWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void CreateToolbarButtons();
  void UpdateToolbarButtons();
  void CommitShape(int x1, int y1, int x2, int y2);
  bool PromptText(int scr_x, int scr_y, char* buf, int buf_size);
  bool CopyToClipboard(const PixelGrabImage* img);
  void PinAnnotation();
  void CopyAnnotation();
  void Cleanup();
  void Cancel();
  void Begin(RECT rc);
  void ShowColorPanel();
  void HideColorPanel();
  void UpdateDimRegion();
  void RaiseToolbar();

  bool IsAnnotating() const { return annotating_; }
  PixelGrabAnnotation* Ann() const { return ann_; }
  HWND Canvas() const { return canvas_; }
  HWND ToolbarWnd() const { return toolbar_wnd_; }
  RECT CanvasRect() const { return canvas_rect_; }
  void SetCanvasRect(RECT r) { canvas_rect_ = r; }
  AnnotTool CurrentTool() const { return current_tool_; }
  void SetCurrentTool(AnnotTool t) { current_tool_ = t; }
  POINT DragStart() const { return drag_start_; }
  void SetDragStart(POINT p) { drag_start_ = p; }
  POINT DragEnd() const { return drag_end_; }
  void SetDragEnd(POINT p) { drag_end_ = p; }
  bool IsDragging() const { return dragging_; }
  void SetDragging(bool v) { dragging_ = v; }
  bool TextOk() const { return text_ok_; }
  void SetTextOk(bool v) { text_ok_ = v; }
  bool TextDone() const { return text_done_; }
  void SetTextDone(bool v) { text_done_ = v; }
  HWND TextEditCtrl() const { return text_edit_ctrl_; }
  void SetTextEditCtrl(HWND h) { text_edit_ctrl_ = h; }
  bool CanvasResizing() const { return canvas_resizing_; }
  void SetCanvasResizing(bool v) { canvas_resizing_ = v; }
  int CanvasResizeEdge() const { return canvas_resize_edge_; }
  void SetCanvasResizeEdge(int e) { canvas_resize_edge_ = e; }
  POINT CanvasResizeStart() const { return canvas_resize_start_; }
  void SetCanvasResizeStart(POINT p) { canvas_resize_start_ = p; }
  RECT CanvasResizeOrig() const { return canvas_resize_orig_; }
  void SetCanvasResizeOrig(RECT r) { canvas_resize_orig_ = r; }

 private:
  bool                  annotating_ = false;
  PixelGrabAnnotation*  ann_ = nullptr;
  HWND                  canvas_ = nullptr;
  HWND                  toolbar_wnd_ = nullptr;
  RECT                  canvas_rect_ = {};
  AnnotTool             current_tool_ = kToolRect;
  POINT                 drag_start_ = {};
  POINT                 drag_end_ = {};
  bool                  dragging_ = false;
  bool                  text_ok_ = false;
  bool                  text_done_ = false;
  HWND                  text_edit_ctrl_ = nullptr;
  bool                  canvas_resizing_ = false;
  int                   canvas_resize_edge_ = 0;
  POINT                 canvas_resize_start_ = {};
  RECT                  canvas_resize_orig_ = {};
  uint32_t              current_color_ = 0xFFFF0000;
  float                 current_width_ = kWidthMedium;
  int                   current_font_size_ = kFontMedium;
  HWND                  color_panel_wnd_ = nullptr;
};

#endif  // _WIN32
#endif  // PIXELGRAB_EXAMPLES_PLATFORM_WINDOWS_CAPTURE_ANNOTATION_MANAGER_H_
