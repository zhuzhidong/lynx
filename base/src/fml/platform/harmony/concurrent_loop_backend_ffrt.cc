// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "base/src/fml/platform/harmony/concurrent_loop_backend_ffrt.h"

#include <memory>
#include <utility>

// @ppd/ffrt transitively pulls job_ring.h, which has an unused variable
// that triggers -Werror. Suppress just around the third-party include.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "ffrt/ffrt.h"  // @ppd/ffrt 1.1.8 — C++ wrappers (header-only)
#pragma GCC diagnostic pop

namespace lynx {
namespace fml {

namespace {
ffrt::qos MapQos(Thread::ThreadPriority p) {
  switch (p) {
    case Thread::ThreadPriority::HIGH:
      return ffrt::qos_user_initiated;
    case Thread::ThreadPriority::LOW:
    case Thread::ThreadPriority::BACKGROUND:
      return ffrt::qos_background;
    case Thread::ThreadPriority::NORMAL:
    default:
      return ffrt::qos_default;
  }
}
}  // namespace

thread_local ConcurrentLoopBackendFFRT*
    ConcurrentLoopBackendFFRT::g_current_worker = nullptr;

ConcurrentLoopBackendFFRT::ConcurrentLoopBackendFFRT(
    const std::string& name_prefix, Thread::ThreadPriority priority,
    size_t worker_count, Thread::ThreadConfigSetter setter)
    : worker_count_(worker_count), setter_(std::move(setter)) {
  // thread_mode(true) gives each task its own OS thread so the per-task
  // thread_local (g_current_worker) is observable.
  queue_ = std::make_unique<ffrt::queue>(
      ffrt::queue_concurrent,
      name_prefix.c_str(),
      ffrt::queue_attr()
          .max_concurrency(static_cast<int>(worker_count))
          .qos(MapQos(priority))
          .thread_mode(true));
}

ConcurrentLoopBackendFFRT::~ConcurrentLoopBackendFFRT() = default;

void ConcurrentLoopBackendFFRT::PostTask(base::closure task) {
  // ffrt::queue::submit needs a CopyConstructible callable, but
  // base::closure is move-only, so wrap it in a shared_ptr.
  auto shared_task = std::make_shared<base::closure>(std::move(task));
  auto wrapped = [this, shared_task]() {
    g_current_worker = this;
    (*shared_task)();
    g_current_worker = nullptr;
  };
  queue_->submit(std::move(wrapped));
}

bool ConcurrentLoopBackendFFRT::RunsTasksOnCurrentThreadWorker() const {
  return g_current_worker == this;
}

void ConcurrentLoopBackendFFRT::Terminate() {
  // ffrt_queue_destroy waits for in-flight tasks before returning.
  queue_.reset();
}

}  // namespace fml
}  // namespace lynx
