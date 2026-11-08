// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/renderer/starlight/layout/parallel_measure.h"
#include "base/include/trace/ffrt_profiling.h"

#if defined(LYNX_STARLIGHT_FFRT_PARALLEL)

#include <algorithm>
#include <vector>

#include "ffrt/ffrt.h"
#include "hitrace/trace.h"

namespace lynx {
namespace starlight {

bool ParallelMeasureIndices(size_t count, size_t threshold,
                            void (*fn)(void*, size_t), void* ctx) {
  if (fn == nullptr || count < threshold) {
    return false;
  }

  // ffrt-parallelization: font-cache warm-up. Run the first few measurements
  // on the calling thread (typically TASM) before dispatching to FFRT
  // workers.
  //
  // Mode notes (measured on P9): MOST_ON_TASM gives the real win (measure
  // phase -39.5% on a realistic article page, first frame -9.9% on the
  // text-stress workload). In ALL_ON_UI, definite-main-axis pages (e.g.
  // text-stress) never enter here (the whitelist requires an indefinite
  // main axis); indefinite-main-axis pages (article-style reading pages)
  // DO dispatch from the UI thread and measure at break-even (21.4ms wait
  // for 30 paragraph measures vs ~25ms serial) — no gain, no regression. Text measurement cold-starts the Skia font manager (loading
  // .ttf files, building glyph caches) behind internal locks — when 16
  // workers hit this simultaneously they serialize on those locks, making
  // each measurement ~14x slower. Warming up a representative sample first
  // populates the font/glyph caches so workers start on warm entries.
  // 8 items covers the typical variety of font styles in a list.
  constexpr size_t kWarmupCount = 8;
  const size_t warmup = std::min(kWarmupCount, count);
  if (lynx::base::IsFFRTProfilingEnabled())
    OH_HiTrace_StartTrace("FFRT_P1_Warmup");
  for (size_t idx = 0; idx < warmup; ++idx) {
    fn(ctx, idx);
  }
  if (lynx::base::IsFFRTProfilingEnabled()) OH_HiTrace_FinishTrace();

  // Remaining items go to FFRT workers in chunks. Chunk size controls the
  // effective worker concurrency: 2400 items / chunk 64 = ~4 concurrent
  // workers (vs 16 with chunk 16). Fewer concurrent workers means less
  // contention on Skia Typography's internal lock.
  //
  // The chunk adapts to the batch size so realistic small batches (30-50
  // items, e.g. article-style reading pages) also get real worker
  // parallelism: a fixed 256 swallows them into a single mega-chunk and the
  // "parallel" batch degenerates to one worker. Large stress batches clamp
  // to the measured optimum (~256, ~8 workers).
  const size_t chunk = [&] {
    const char* env = getenv("LYNX_FFRT_CHUNK");
    if (env != nullptr) {
      const int v = atoi(env);
      if (v >= 8) return static_cast<size_t>(v);
    }
    constexpr size_t kTargetWorkers = 8;
    constexpr size_t kMinChunk = 8;
    constexpr size_t kMaxChunk = 256;
    return std::min(std::max((count - warmup) / kTargetWorkers, kMinChunk),
                    kMaxChunk);
  }();
  std::vector<ffrt::task_handle> handles;
  handles.reserve((count - warmup + chunk - 1) / chunk);
  for (size_t begin = warmup; begin < count; begin += chunk) {
    const size_t end = std::min(begin + chunk, count);
    // submit_h gives handles so the wait below is scoped to this batch only
    // (a bare ffrt::wait would also block on unrelated ArkUI FFRT tasks).
    handles.push_back(ffrt::submit_h([=] {
      for (size_t idx = begin; idx < end; ++idx) {
        fn(ctx, idx);
      }
    }));
  }
  // FFRT_P1_region B/E: the submit -> wait window (parallel measure batch).
  if (lynx::base::IsFFRTProfilingEnabled())
    OH_HiTrace_StartTrace("FFRT_P1_region");
  for (const auto& handle : handles) {
    ffrt::wait({handle});
  }
  if (lynx::base::IsFFRTProfilingEnabled()) OH_HiTrace_FinishTrace();
  return true;
}

}  // namespace starlight
}  // namespace lynx

#else  // !LYNX_STARLIGHT_FFRT_PARALLEL

namespace lynx {
namespace starlight {

bool ParallelMeasureIndices(size_t /*count*/, size_t /*threshold*/,
                            void (* /*fn*/)(void*, size_t), void* /*ctx*/) {
  return false;
}

}  // namespace starlight
}  // namespace lynx

#endif  // LYNX_STARLIGHT_FFRT_PARALLEL
