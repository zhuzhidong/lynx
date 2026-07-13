// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef BASE_INCLUDE_FML_CONCURRENT_MESSAGE_LOOP_BACKEND_H_
#define BASE_INCLUDE_FML_CONCURRENT_MESSAGE_LOOP_BACKEND_H_

#include <memory>
#include <string>

#include "base/include/base_export.h"
#include "base/include/closure.h"
#include "base/include/fml/thread.h"

namespace lynx {
namespace fml {

class ConcurrentLoopBackend {
 public:
  virtual ~ConcurrentLoopBackend() = default;

  // 在后端执行器上异步执行 task。task 必非空，由 facade 保证 shutdown 后不再调用。
  virtual void PostTask(base::closure task) = 0;

  // 当前线程是否正运行本后端的一个任务。
  virtual bool RunsTasksOnCurrentThreadWorker() const = 0;

  virtual size_t GetWorkerCount() const = 0;

  // 停止接收新任务，等待已在执行的任务结束后回收资源。
  virtual void Terminate() = 0;
};

// 编译期工厂：按平台宏选后端。priority/worker_count 由 facade 构造时传入。
// `setter` 是 per-worker-thread 的线程配置回调（设置名称/优先级等）。
// 传 nullptr 表示使用平台默认选择（iOS/Android: PlatformThreadPriority::Setter;
// 其余: Thread::SetCurrentThreadName），保留未提供 setter 时的历史行为。
BASE_EXPORT std::unique_ptr<ConcurrentLoopBackend> CreateConcurrentLoopBackend(
    const std::string& name_prefix,
    Thread::ThreadPriority priority,
    size_t worker_count,
    Thread::ThreadConfigSetter setter = nullptr);

}  // namespace fml
}  // namespace lynx

#endif  // BASE_INCLUDE_FML_CONCURRENT_MESSAGE_LOOP_BACKEND_H_
