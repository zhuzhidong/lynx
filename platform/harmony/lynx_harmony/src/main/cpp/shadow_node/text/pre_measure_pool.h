// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_PRE_MEASURE_POOL_H_
#define PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_PRE_MEASURE_POOL_H_

#include <atomic>
#include <memory>
#include <mutex>

#include "base/include/trace/ffrt_profiling.h"
#include <utility>
#include <vector>

#include "base/include/fml/memory/ref_counted.h"
#include "ffrt/ffrt.h"
#include "hitrace/trace.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/shadow_node/shadow_node.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/text/font_collection_harmony.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/text/paragraph_builder_harmony.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/text/paragraph_harmony.h"

namespace lynx {
namespace tasm {
namespace harmony {

class TextShadowNode;

// FFRT text pre-measure pool (ffrt-parallelization iteration 2, point P1).
//
// Main thread snapshots a populated ParagraphBuilderHarmony (cheap style/text
// accumulation) once a text node's props are complete during a patch; FFRT
// workers then run the expensive CreateParagraph + Layout off the layout
// critical path. The layout pass waits on the drain task handle only
// (ffrt::wait({handle}) — NOT the global ffrt::wait, which would also block
// on unrelated ArkUI FFRT tasks) before the first Measure consumption, and
// reuses the pre-measured paragraph when the generation and width constraint
// match; otherwise the original serial path runs unchanged.
//
// Thread-safety contract:
// - queue_ / draining_ / drain_handle_ only touched under queue_mutex_;
// - the snapshot builder is owned by the work item (main builds, worker
//   consumes; no shared mutable state);
// - StorePreMeasured writes node fields from the worker before the scoped
//   wait barrier; the main thread reads them only after the barrier.
class TextPreMeasurePool {
 public:
  static TextPreMeasurePool& Instance() {
    static TextPreMeasurePool pool;
    return pool;
  }

  // Main thread: enqueue one pre-measure request. `snapshot` must be a fully
  // populated builder (style/text accumulated); `layout_max_width` is the
  // guessed constraint in the same unit TextShadowNode uses.
  void Enqueue(fml::RefPtr<ShadowNode> node,
               std::unique_ptr<ParagraphBuilderHarmony> snapshot,
               std::shared_ptr<FontCollectionHarmony> fonts,
               float layout_max_width, uint32_t generation) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.push_back(WorkItem{std::move(node), std::move(snapshot),
                              std::move(fonts), layout_max_width,
                              generation});
    if (draining_) {
      return;  // drain task in flight will pick this up
    }
    draining_ = true;
    // submit_h gives a handle so JoinPending can wait on this task alone.
    // task_handle::p is private, so validity is tracked with has_handle_.
    drain_handle_ = ffrt::submit_h([this] { Drain(); });
    has_handle_ = true;
  }

  // Main thread: wait for the pending drain task only (scoped — never the
  // global FFRT queue, which also carries ArkUI work).
  void JoinPending() {
    ffrt::task_handle handle;
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      if (!draining_) {
        return;
      }
      if (!has_handle_) {
        // submit_h failed; leave the item for the next wave (serial now).
        draining_ = false;
        return;
      }
      handle = drain_handle_;
    }
    // FFRT_P1_region B/E: the actual wait window on the main thread.
    if (lynx::base::IsFFRTProfilingEnabled()) OH_HiTrace_StartTrace("FFRT_P1_region");
    ffrt::wait({handle});
    if (lynx::base::IsFFRTProfilingEnabled()) OH_HiTrace_FinishTrace();
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      draining_ = false;
    }
  }

 private:
  struct WorkItem {
    fml::RefPtr<ShadowNode> node;
    std::unique_ptr<ParagraphBuilderHarmony> snapshot;
    std::shared_ptr<FontCollectionHarmony> fonts;
    float layout_max_width{0.f};
    uint32_t generation{0};
  };

  void Drain() {
    for (;;) {
      WorkItem item;
      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_.empty()) {
          return;  // JoinPending resets draining_ after the scoped wait
        }
        item = std::move(queue_.front());
        queue_.erase(queue_.begin());
      }
      // Expensive path: typography build + layout, off the main thread.
      // Reuse the same measurement split the serial path uses
      // (CreateParagraph(width) + Layout(layout_max_width)).
      fml::RefPtr<ParagraphHarmony> paragraph =
          item.snapshot->CreateParagraph(item.fonts, item.layout_max_width);
      if (paragraph) {
        paragraph->Layout(item.layout_max_width);
        paragraph->SetText(item.snapshot->GetText());
      }
      if (item.node && item.node->IsTextShadowNode()) {
        auto* text = static_cast<TextShadowNode*>(item.node.get());
        text->StorePreMeasured(std::move(paragraph), item.layout_max_width,
                               item.generation,
                               item.snapshot->GetCharCount());
      }
    }
  }

  std::mutex queue_mutex_;
  std::vector<WorkItem> queue_;      // guarded by queue_mutex_
  bool draining_{false};             // guarded by queue_mutex_
  bool has_handle_{false};           // guarded by queue_mutex_
  ffrt::task_handle drain_handle_;   // guarded by queue_mutex_
};

}  // namespace harmony
}  // namespace tasm
}  // namespace lynx

#endif  // PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_PRE_MEASURE_POOL_H_
