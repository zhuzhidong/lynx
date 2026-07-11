#include "base/include/fml/concurrent_message_loop_backend.h"
#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

namespace lynx {
namespace fml {
namespace {

// Test fake backend: each PostTask spawns a fresh thread, sets the
// per-task thread_local g_current to this, runs the task, clears it.
class FakeBackend : public ConcurrentLoopBackend {
 public:
  explicit FakeBackend(size_t worker_count) : worker_count_(worker_count) {}

  void PostTask(base::closure task) override {
    threads_.emplace_back([this, t = std::move(task)]() {
      g_current = this;
      t();
      g_current = nullptr;
    });
  }
  bool RunsTasksOnCurrentThreadWorker() const override {
    return g_current == this;
  }
  size_t GetWorkerCount() const override { return worker_count_; }
  void Terminate() override {
    for (auto& t : threads_) {
      if (t.joinable()) t.join();
    }
    threads_.clear();
  }

  static thread_local ConcurrentLoopBackend* g_current;

 private:
  size_t worker_count_;
  std::vector<std::thread> threads_;
};
thread_local ConcurrentLoopBackend* FakeBackend::g_current = nullptr;

// C1: 非 shutdown 期 PostTask 必最终执行一次且仅一次
TEST(ConcurrentLoopBackendContract, C1TaskIsExecuted) {
  auto backend = std::make_unique<FakeBackend>(2);
  std::atomic<int> count{0};
  base::closure task = [&] { count.fetch_add(1); };
  backend->PostTask(task);
  backend->Terminate();
  EXPECT_EQ(count.load(), 1);
}

// C3: 任务执行期间 RunsTasksOnCurrentThreadWorker() == true；非任务上下文 == false
TEST(ConcurrentLoopBackendContract, C3WorkerSelfIdentifiesInsideTask) {
  auto backend = std::make_unique<FakeBackend>(1);
  std::atomic<bool> inside{false};
  std::atomic<bool> flag_inside{false};
  base::closure task = [&] {
    inside.store(backend->RunsTasksOnCurrentThreadWorker());
    flag_inside.store(FakeBackend::g_current == backend.get());
  };
  backend->PostTask(task);
  backend->Terminate();
  EXPECT_TRUE(inside.load());
  EXPECT_TRUE(flag_inside.load());
  // 主线程不在该 backend 任务上下文
  EXPECT_FALSE(backend->RunsTasksOnCurrentThreadWorker());
}

}  // namespace
}  // namespace fml
}  // namespace lynx
