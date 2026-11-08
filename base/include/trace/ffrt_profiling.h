// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef BASE_INCLUDE_TRACE_FFRT_PROFILING_H_
#define BASE_INCLUDE_TRACE_FFRT_PROFILING_H_

#include <atomic>

namespace lynx {
namespace base {

// ffrt-parallelization: runtime switch for profiling markers.
// Controlled via system property debug.lynx.ffrt_profiling (1=on, 0=off).
// Production default: off (zero overhead when disabled).
inline bool IsFFRTProfilingEnabled() {
  // Debug builds: default ON (needed for ffrt-parallelization measurement).
  // Production: set LYNX_FFRT_PROFILING=0 env var or system param to disable.
#ifndef NDEBUG
  return true;
#else
  static std::atomic<bool> enabled{false};
  static std::atomic<bool> initialized{false};
  if (!initialized.load(std::memory_order_relaxed)) {
    const char* env = getenv("LYNX_FFRT_PROFILING");
    if (env != nullptr && env[0] == '1') {
      enabled.store(true, std::memory_order_relaxed);
    }
    initialized.store(true, std::memory_order_relaxed);
  }
  return enabled.load(std::memory_order_relaxed);
#endif
}

}  // namespace base
}  // namespace lynx

#endif  // BASE_INCLUDE_TRACE_FFRT_PROFILING_H_
