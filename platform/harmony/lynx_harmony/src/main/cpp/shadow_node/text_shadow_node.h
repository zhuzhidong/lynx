// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_SHADOW_NODE_H_
#define PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_SHADOW_NODE_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "platform/harmony/lynx_harmony/src/main/cpp/lynx_context.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/shadow_node/base_text_shadow_node.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/shadow_node/shadow_node.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/shadow_node/text/text_attributes.h"
#include "platform/harmony/lynx_harmony/src/main/cpp/text/style_harmony.h"

namespace lynx {
namespace tasm {
namespace harmony {
class ParagraphBuilderHarmony;
class FontCollectionHarmony;
class ParagraphHarmony;
class InlineTruncationShadowNode;
class TextShadowNode final : public BaseTextShadowNode,
                             public CustomMeasureFunc {
 public:
  TextShadowNode(int sign, const std::string& tag);
  static ShadowNode* Make(int sign, const std::string& tag) {
    return new TextShadowNode(sign, tag);
  }

  void OnLayoutBefore() override;
  void UpdateLayout(float left, float top, float width, float height) override;
  ~TextShadowNode() override;
  void OnContextReady() override;
  virtual bool IsTextShadowNode() const override { return true; }
  fml::RefPtr<fml::RefCountedThreadSafeStorage> getExtraBundle() override;
  LayoutResult Measure(float width, MeasureMode width_mode, float height,
                       MeasureMode height_mode, bool final_measure) override;
  void Align() override;

  void OnAppendToParagraph(ParagraphBuilderHarmony& builder, float width,
                           float height) override;
  void OnPropsUpdate(const std::string& name,
                     const lepus::Value& value) override;

  // ffrt-parallelization iteration 2 (point P1): pre-measure pool hooks.
  // Enqueue a snapshot builder once this node's prop bundle is complete.
  void UpdateProps(PropBundleHarmony* props) override;
  // Worker-thread store (before the ffrt::wait barrier); main thread reads
  // only after JoinPending.
  void StorePreMeasured(fml::RefPtr<ParagraphHarmony> paragraph,
                        float layout_max_width, uint32_t generation,
                        int32_t char_count);
  // Bump the content generation so a stale pre-measured paragraph is never
  // consumed (called for descendant text updates as well).
  void BumpPreMeasureGeneration() {
    content_generation_.fetch_add(1, std::memory_order_relaxed);
  }
  // ffrt-parallelization iteration 2 (point P1): snapshot + enqueue with the
  // current content; called from this node's prop bundle completion, from
  // descendant updates/attach (see BaseTextShadowNode). Bumps the generation
  // so older pre-measured snapshots miss.
  void MaybeEnqueuePreMeasure();

 private:
  void HandleWhiteSpace();
  fml::RefPtr<ParagraphHarmony> TryConsumePreMeasured(float layout_max_width);
  fml::RefPtr<ParagraphHarmony> HandleTextOverflowAndTruncation(
      fml::RefPtr<ParagraphHarmony> paragraph, float width,
      MeasureMode width_mode, float height, MeasureMode height_mode,
      bool final_measure);
  InlineTruncationShadowNode* FindInlineTruncationNode();
  int32_t FindTruncationLineIndex(ParagraphHarmony* paragraph,
                                  float layout_max_height);
  float ConstructTextWidthConstraint(MeasureMode width_mode, float width);
  float ConstructTextHeightConstraint(MeasureMode height_mode, float height);
  float CalcTextTranslateOffset(float layout_max_width, float text_max_width);
  void HandleMaxWidthMode(BaseTextShadowNode* shadow_node,
                          ParagraphHarmony* paragraph, float width,
                          MeasureMode width_mode, float layout_max_width,
                          float text_max_width);
  void UpdateLineHeight(BaseTextShadowNode* node, double line_height);
  void DispatchLayoutEvent();
  bool IsTextOverflow(ParagraphHarmony* paragraph, float layout_max_height);

  fml::RefPtr<ParagraphHarmony> ReBuildParagraph(
      ParagraphBuilderHarmony* builder, const LayoutResult result,
      const float layout_max_width, const float offset);
  fml::RefPtr<ParagraphHarmony> LayoutNode(BaseTextShadowNode* shadow_node,
                                           ParagraphBuilderHarmony* builder,
                                           float width, MeasureMode width_mode,
                                           float height,
                                           MeasureMode height_mode,
                                           bool final_measure,
                                           fml::RefPtr<ParagraphHarmony>
                                               pre_built = nullptr);
  LayoutResult GetLayoutResult(ParagraphHarmony* paragraph, float width,
                               MeasureMode width_mode, float height,
                               MeasureMode height_mode);

  std::unique_ptr<ParagraphStyleHarmony> paragraph_style_;
  std::shared_ptr<FontCollectionHarmony> font_collection_;
  std::unique_ptr<ParagraphBuilderHarmony> paragraph_builder_;
  fml::RefPtr<ParagraphHarmony> paragraph_;
  InlineTruncationShadowNode* inline_truncation_shadow_node_;
  int32_t origin_char_count_{0};
  int32_t ellipsis_count_{0};
  // ffrt-parallelization iteration 2 (point P1): pre-measure state.
  // content_generation_ is main-thread written, worker read at store time.
  // The pre_measured_* fields are worker written before the ffrt::wait
  // barrier and main-thread read only after it.
  std::atomic<uint32_t> content_generation_{0};
  fml::RefPtr<ParagraphHarmony> pre_measured_paragraph_;
  float pre_measured_width_{0.f};
  uint32_t pre_measured_generation_{0};
  int32_t pre_measured_char_count_{0};
  bool pre_measured_valid_{false};
  std::string rich_type_;
};

}  // namespace harmony
}  // namespace tasm
}  // namespace lynx

#endif  // PLATFORM_HARMONY_LYNX_HARMONY_SRC_MAIN_CPP_SHADOW_NODE_TEXT_SHADOW_NODE_H_
