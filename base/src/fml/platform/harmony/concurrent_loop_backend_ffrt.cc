// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "base/src/fml/platform/harmony/concurrent_loop_backend_ffrt.h"

#include <memory>
#include <utility>

// The @ppd/ffrt C++ wrappers transitively include job_ring.h, which has
// an unused variable (us) that triggers -Werror. Suppress just that warning
// around the third-party include; our own code keeps full warnings.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "ffrt/ffrt.h"  // @ppd/ffrt 1.1.8 — C++ wrappers (header-only)
#pragma GCC diagnostic pop

namespace lynx {
namespace fml {

namespace {
// HIGH → qos_user_initiated (the highest user-initiated QoS class in
// this FFRT SDK; the C++ enum mirrors the C ffrt_qos_user_initiated).
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
  // queue_attr chain: max_concurrency → qos → thread_mode. thread_mode(true)
  // makes each FFRT task run on its own OS thread so the per-task
  // thread_local (g_current_worker) is observable. The chain result is
  // bound directly to ffrt::queue's const queue_attr& parameter — storing
  // it in a local would require a copy (queue_attr deletes its copy ctor).
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
  // Wrap the MoveOnly closure in a shared_ptr so the lambda capturing it
  // is CopyConstructible. ffrt::queue::submit wraps the callable in a
  // std::function internally, which requires the callable to be copyable.
  auto shared_task = std::make_shared<base::closure>(std::move(task));
  // Set task-level sentinel around the user closure. The ffrt::queue
  // thread_mode(true) ensures this lambda runs on a single OS thread,
  // so the thread_local is reliably observable.
  auto wrapped = [this, shared_task]() {
    g_current_worker = this;
    (*shared_task)();
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
