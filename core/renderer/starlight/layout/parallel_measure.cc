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
  // workers. Text measurement cold-starts the Skia font manager (loading
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
  static const size_t kChunkSize = [] {
    const char* env = getenv("LYNX_FFRT_CHUNK");
    if (env != nullptr) {
      const int v = atoi(env);
      if (v >= 8) return static_cast<size_t>(v);
    }
    return static_cast<size_t>(256);  // default: ~5 workers (measured optimum)
  }();
  std::vector<ffrt::task_handle> handles;
  handles.reserve((count - warmup + kChunkSize - 1) / kChunkSize);
  for (size_t begin = warmup; begin < count; begin += kChunkSize) {
    const size_t end = std::min(begin + kChunkSize, count);
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
