// Copyright 2024 PixelGrab Authors. All rights reserved.
// Windows UI Automation element detector implementation.

#include "platform/windows/win_element_detector.h"

#include <comdef.h>

#include <algorithm>
#include <string>
#include <vector>

namespace pixelgrab {
namespace internal {

// Helper: BSTR to UTF-8 std::string, then free the BSTR.
static std::string BstrToUtf8(BSTR bstr) {
  if (!bstr) return "";
  int len = SysStringLen(bstr);
  if (len == 0) {
    SysFreeString(bstr);
    return "";
  }
  int utf8_len =
      WideCharToMultiByte(CP_UTF8, 0, bstr, len, nullptr, 0, nullptr, nullptr);
  if (utf8_len <= 0) {
    SysFreeString(bstr);
    return "";
  }
  std::string result(utf8_len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, bstr, len, &result[0], utf8_len, nullptr,
                      nullptr);
  SysFreeString(bstr);
  return result;
}

// Helper: map UIA control type ID to a short role string.
static std::string ControlTypeToRole(CONTROLTYPEID ct) {
  switch (ct) {
    case UIA_ButtonControlTypeId:
      return "button";
    case UIA_EditControlTypeId:
      return "edit";
    case UIA_TextControlTypeId:
      return "text";
    case UIA_WindowControlTypeId:
      return "window";
    case UIA_MenuControlTypeId:
    case UIA_MenuItemControlTypeId:
      return "menu";
    case UIA_ListControlTypeId:
    case UIA_ListItemControlTypeId:
      return "list";
    case UIA_TabControlTypeId:
    case UIA_TabItemControlTypeId:
      return "tab";
    case UIA_TreeControlTypeId:
    case UIA_TreeItemControlTypeId:
      return "tree";
    case UIA_ToolBarControlTypeId:
      return "toolbar";
    case UIA_StatusBarControlTypeId:
      return "statusbar";
    case UIA_CheckBoxControlTypeId:
      return "checkbox";
    case UIA_RadioButtonControlTypeId:
      return "radio";
    case UIA_ComboBoxControlTypeId:
      return "combobox";
    case UIA_ScrollBarControlTypeId:
      return "scrollbar";
    case UIA_PaneControlTypeId:
      return "pane";
    case UIA_GroupControlTypeId:
      return "group";
    case UIA_ImageControlTypeId:
      return "image";
    case UIA_HyperlinkControlTypeId:
      return "link";
    case UIA_TitleBarControlTypeId:
      return "titlebar";
    default:
      return "unknown";
  }
}

WinElementDetector::WinElementDetector() = default;

WinElementDetector::~WinElementDetector() {
  if (automation_) {
    automation_->Release();
    automation_ = nullptr;
  }
  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
}

bool WinElementDetector::Initialize() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
    // S_FALSE means COM was already initialized on this thread.
    // RPC_E_CHANGED_MODE means it was initialized with different threading.
    // Both are acceptable for our usage.
    com_initialized_ = (hr != RPC_E_CHANGED_MODE);
  } else {
    return false;
  }

  hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                        __uuidof(IUIAutomation),
                        reinterpret_cast<void**>(&automation_));
  return SUCCEEDED(hr) && automation_ != nullptr;
}

bool WinElementDetector::FillElementInfo(IUIAutomationElement* elem,
                                         ElementInfo* out, int depth) const {
  if (!elem || !out) return false;

  // Bounding rectangle.
  RECT rect = {};
  HRESULT hr = elem->get_CurrentBoundingRectangle(&rect);
  if (FAILED(hr)) return false;

  out->x = rect.left;
  out->y = rect.top;
  out->width = rect.right - rect.left;
  out->height = rect.bottom - rect.top;
  out->depth = depth;

  // Skip elements with zero or negative size.
  if (out->width <= 0 || out->height <= 0) return false;

  // Name.
  BSTR name = nullptr;
  hr = elem->get_CurrentName(&name);
  if (SUCCEEDED(hr)) {
    out->name = BstrToUtf8(name);  // Frees BSTR internally.
  }

  // Role (control type).
  CONTROLTYPEID ct = 0;
  hr = elem->get_CurrentControlType(&ct);
  if (SUCCEEDED(hr)) {
    out->role = ControlTypeToRole(ct);
  }

  return true;
}

bool WinElementDetector::DetectElement(int screen_x, int screen_y,
                                       ElementInfo* out_info) {
  if (!automation_ || !out_info) return false;

  POINT pt;
  pt.x = screen_x;
  pt.y = screen_y;

  IUIAutomationElement* elem = nullptr;
  HRESULT hr = automation_->ElementFromPoint(pt, &elem);
  if (FAILED(hr) || !elem) return false;

  bool ok = FillElementInfo(elem, out_info, 0);
  elem->Release();
  return ok;
}

int WinElementDetector::DetectElements(int screen_x, int screen_y,
                                       ElementInfo* out_infos,
                                       int max_count) {
  if (!automation_ || !out_infos || max_count <= 0) return -1;

  POINT pt;
  pt.x = screen_x;
  pt.y = screen_y;

  // Start with the deepest element at this point.
  IUIAutomationElement* deepest = nullptr;
  HRESULT hr = automation_->ElementFromPoint(pt, &deepest);
  if (FAILED(hr) || !deepest) return 0;

  // Walk up the tree to collect ancestors.
  // We collect from deepest → root, then reverse so largest is first.
  struct ElemEntry {
    IUIAutomationElement* elem;
    int depth;
  };

  std::vector<ElemEntry> stack;
  stack.push_back({deepest, 0});

  IUIAutomationTreeWalker* walker = nullptr;
  hr = automation_->get_ControlViewWalker(&walker);
  if (SUCCEEDED(hr) && walker) {
    IUIAutomationElement* current = deepest;
    current->AddRef();  // We'll Release in the loop.
    int depth = 1;

    while (depth < max_count) {
      IUIAutomationElement* parent = nullptr;
      hr = walker->GetParentElement(current, &parent);
      current->Release();
      if (FAILED(hr) || !parent) break;

      // Check if this is the desktop root (stop there).
      CONTROLTYPEID ct = 0;
      parent->get_CurrentControlType(&ct);
      // Desktop pane / root element check: stop if we've gone too far.
      BOOL is_offscreen = FALSE;
      parent->get_CurrentIsOffscreen(&is_offscreen);

      RECT rect = {};
      parent->get_CurrentBoundingRectangle(&rect);
      int w = rect.right - rect.left;
      int h = rect.bottom - rect.top;
      if (w <= 0 || h <= 0) {
        parent->Release();
        break;
      }

      stack.push_back({parent, depth});
      current = parent;
      current->AddRef();
      ++depth;
    }
    if (current != deepest) {
      // current was AddRef'd in the loop; release extra ref.
    }
    walker->Release();
  }

  // Reverse so index 0 = largest (outermost) element.
  std::reverse(stack.begin(), stack.end());

  int written = 0;
  for (size_t i = 0; i < stack.size() && written < max_count; ++i) {
    ElementInfo info;
    if (FillElementInfo(stack[i].elem, &info,
                        static_cast<int>(stack.size() - 1 - i))) {
      out_infos[written++] = info;
    }
  }

  // Release all elements.
  for (auto& entry : stack) {
    if (entry.elem) entry.elem->Release();
  }

  return written;
}

// Factory implementation.
std::unique_ptr<ElementDetector> CreatePlatformElementDetector() {
  auto detector = std::make_unique<WinElementDetector>();
  if (!detector->Initialize()) return nullptr;
  return detector;
}

}  // namespace internal
}  // namespace pixelgrab
