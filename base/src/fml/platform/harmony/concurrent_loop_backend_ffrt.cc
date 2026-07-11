// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "base/src/fml/platform/harmony/concurrent_loop_backend_ffrt.h"

#include <utility>

#include "ffrt/ffrt.h"  // @ppd/ffrt 1.1.8 — C++ wrappers (header-only)

namespace lynx {
namespace fml {

thread_local ConcurrentLoopBackendFFRT*
    ConcurrentLoopBackendFFRT::g_current_worker = nullptr;

ConcurrentLoopBackendFFRT::ConcurrentLoopBackendFFRT(
    const std::string& name_prefix, Thread::ThreadPriority priority,
    size_t worker_count, Thread::ThreadConfigSetter setter)
    : worker_count_(worker_count), setter_(std::move(setter)) {
  // queue_attr chain: max_concurrency → qos → thread_mode.
  // thread_mode(true) makes each FFRT task run on its own OS thread so
  // the per-task thread_local (g_current_worker) is observable.
  auto attr = ffrt::queue_attr()
                  .max_concurrency(static_cast<int>(worker_count))
                  .qos(MapQos(priority))
                  .thread_mode(true);

  queue_ = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
                                         name_prefix.c_str(), attr);
}

ConcurrentLoopBackendFFRT::~ConcurrentLoopBackendFFRT() = default;

// static
ffrt::qos ConcurrentLoopBackendFFRT::MapQos(Thread::ThreadPriority p) {
  switch (p) {
    // HIGH → qos_user_interactive ("UI 响应" 档). The enum header annotates
    // it as @since 23, but the FFRT C interface does not validate the enum
    // value against the API level, so the integer value 5 is usable on
    // all targets (Harmony 6.0+ runtime).
    case Thread::ThreadPriority::HIGH:
      return ffrt::qos_user_interactive;
    case Thread::ThreadPriority::LOW:
    case Thread::ThreadPriority::BACKGROUND:
      return ffrt::qos_background;
    case Thread::ThreadPriority::NORMAL:
    default:
      return ffrt::qos_default;
  }
}

void ConcurrentLoopBackendFFRT::PostTask(base::closure task) {
  // Set task-level sentinel around the user closure. The ffrt::queue
  // thread_mode(true) ensures this lambda runs on a single OS thread,
  // so the thread_local is reliably observable.
  auto wrapped = [this, t = std::move(task)]() mutable {
    g_current_worker = this;
    t();
    g_current_worker = nullptr;
  };
  // ffrt::queue::submit takes std::function&& and converts internally
  // via create_function_wrapper; fire-and-forget (returns void, no
  // task_handle to cancel).
  queue_->submit(std::move(wrapped));
}

bool ConcurrentLoopBackendFFRT::RunsTasksOnCurrentThreadWorker() const {
  return g_current_worker == this;
}

void ConcurrentLoopBackendFFRT::Terminate() {
  // Reset the unique_ptr to invoke ffrt::queue's destructor
  // (= ffrt_queue_destroy). ffrt_queue_destroy waits for in-flight
  // tasks to finish before returning, so this is a synchronous join.
  queue_.reset();
}

}  // namespace fml
}  // namespace lynx