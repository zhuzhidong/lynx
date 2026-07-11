// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef BASE_SRC_FML_PLATFORM_HARMONY_CONCURRENT_LOOP_BACKEND_FFRT_H_
#define BASE_SRC_FML_PLATFORM_HARMONY_CONCURRENT_LOOP_BACKEND_FFRT_H_

#include <cstddef>
#include <memory>
#include <string>

#include "base/include/fml/concurrent_message_loop_backend.h"
#include "base/include/fml/thread.h"

// Forward-declare ffrt::queue so this header does not need to pull in the
// FFRT C++ API headers. The concrete type and its RAII semantics are
// resolved in the .cc, which is the only translation unit that includes
// `<ffrt/ffrt.h>`. std::unique_ptr<T> with an incomplete T is allowed as
// long as the destructor is defined where T is complete.
namespace ffrt {
class queue;
}  // namespace ffrt

namespace lynx {
namespace fml {

// BackendFFRT: implements ConcurrentLoopBackend using FFRT (Harmony's
// task-based concurrency framework). Uses the FFRT C++ API
// (ffrt::queue, ffrt::queue_attr) — see .cc for the ffrt includes.
// ConcurrentLoopBackend::RunsTasksOnCurrentThreadWorker is implemented
// via a per-task thread_local (g_current_worker); the FFRT queue is
// configured with thread_mode(true) to ensure each task runs on its
// own OS thread context so the thread_local is valid.
class ConcurrentLoopBackendFFRT final : public ConcurrentLoopBackend {
 public:
  ConcurrentLoopBackendFFRT(const std::string& name_prefix,
                            Thread::ThreadPriority priority,
                            size_t worker_count,
                            Thread::ThreadConfigSetter setter = nullptr);
  ~ConcurrentLoopBackendFFRT() override;

  void PostTask(base::closure task) override;
  bool RunsTasksOnCurrentThreadWorker() const override;
  size_t GetWorkerCount() const override { return worker_count_; }
  void Terminate() override;

 private:
  // Pimpl: ffrt::queue is forward-declared via the unique_ptr<ffrt::queue>
  // type. The actual definition (and ffrt headers) is in the .cc.
  std::unique_ptr<ffrt::queue> queue_;
  size_t worker_count_;
  // Stored for parity with BackendStd. May be applied in T9/T10 to
  // configure per-task thread attributes; ignored while ffrt::queue_attr
  // is constructed inline in the .cc.
  Thread::ThreadConfigSetter setter_;

  static thread_local ConcurrentLoopBackendFFRT* g_current_worker;
};

}  // namespace fml
}  // namespace lynx

#endif  // BASE_SRC_FML_PLATFORM_HARMONY_CONCURRENT_LOOP_BACKEND_FFRT_H_