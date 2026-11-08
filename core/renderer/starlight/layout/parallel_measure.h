// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CORE_RENDERER_STARLIGHT_LAYOUT_PARALLEL_MEASURE_H_
#define CORE_RENDERER_STARLIGHT_LAYOUT_PARALLEL_MEASURE_H_

#include <cstddef>

namespace lynx {
namespace starlight {

// Runs `fn(ctx, idx)` for every idx in [0, count) in parallel when the
// platform provides an implementation and the workload clears `threshold`.
// Returns false when it did not run (too few items, no platform runtime);
// the caller must then execute the serial loop unchanged.
//
// ffrt-parallelization iteration 3 (point P1): the FFRT implementation lives
// in parallel_measure.cc behind OS_HARMONY.
bool ParallelMeasureIndices(size_t count, size_t threshold,
                            void (*fn)(void*, size_t), void* ctx);

}  // namespace starlight
}  // namespace lynx

#endif  // CORE_RENDERER_STARLIGHT_LAYOUT_PARALLEL_MEASURE_H_
