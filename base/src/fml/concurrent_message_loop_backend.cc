// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "base/include/fml/concurrent_message_loop_backend.h"

#include <memory>

#include "base/include/fml/thread.h"
#if defined(OS_HARMONY)
#include "base/src/fml/platform/harmony/concurrent_loop_backend_ffrt.h"
#endif
#include "base/src/fml/concurrent_loop_backend_std.h"

// Phase 1 / Task 5 / Phase 2 / Task 10: factory dispatches to a
// platform-specific backend. Future phases will extend the branches:
//   - OS_HARMONY        -> ConcurrentLoopBackendFFRT (Phase 2 / Task 9-10)
//   - OS_IOS / OS_OSX   -> ConcurrentLoopBackendGCD  (planned)
//   - else              -> ConcurrentLoopBackendStd   (current path)
// The shape (`#if defined(OS_*)` ladder + early returns) is preserved so
// the gating switch is a one-line edit when a new backend lands.

namespace lynx {
namespace fml {

std::unique_ptr<ConcurrentLoopBackend> CreateConcurrentLoopBackend(
    const std::string& name_prefix, Thread::ThreadPriority priority,
    size_t worker_count, Thread::ThreadConfigSetter setter) {
#if defined(OS_HARMONY)
  return std::make_unique<ConcurrentLoopBackendFFRT>(
      name_prefix, priority, worker_count, std::move(setter));
#elif defined(OS_IOS) || defined(OS_OSX)
  // Placeholder until BackendGCD lands.
  return std::make_unique<ConcurrentLoopBackendStd>(
      name_prefix, priority, worker_count, std::move(setter));
#else
  return std::make_unique<ConcurrentLoopBackendStd>(
      name_prefix, priority, worker_count, std::move(setter));
#endif
}

}  // namespace fml
}  // namespace lynx
