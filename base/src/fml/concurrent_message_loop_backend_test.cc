#include "base/include/fml/concurrent_message_loop.h"
#include "base/include/fml/concurrent_message_loop_backend.h"
#include "gtest/gtest.h"

#include <atomic>
#include <memory>
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
// 注：base::closure 是 move-only；构造时必须接受 rvalue（lambda 字面量是 prvalue）。
TEST(ConcurrentLoopBackendContract, C1TaskIsExecuted) {
  auto backend = std::make_unique<FakeBackend>(2);
  std::atomic<int> count{0};
  backend->PostTask([&] { count.fetch_add(1); });
  backend->Terminate();
  EXPECT_EQ(count.load(), 1);
}

// C3: 任务执行期间 RunsTasksOnCurrentThreadWorker() == true；非任务上下文 == false
TEST(ConcurrentLoopBackendContract, C3WorkerSelfIdentifiesInsideTask) {
  auto backend = std::make_unique<FakeBackend>(1);
  std::atomic<bool> inside{false};
  std::atomic<bool> flag_inside{false};
  backend->PostTask([&] {
    inside.store(backend->RunsTasksOnCurrentThreadWorker());
    flag_inside.store(FakeBackend::g_current == backend.get());
  });
  backend->Terminate();
  EXPECT_TRUE(inside.load());
  EXPECT_TRUE(flag_inside.load());
  // 主线程不在该 backend 任务上下文
  EXPECT_FALSE(backend->RunsTasksOnCurrentThreadWorker());
}

// C2: facade 在 shutdown 后 PostTask 必须在调用方线程同步执行。
//
// 合同要点：
//   1) PostTask 调用返回时，task 必已经执行过一次（同步语义）。
//   2) task 在调用方线程运行（不是任何 worker 线程）。
//   3) 调用方线程不是 backend 的 worker 线程（自我识别为 false）。
//
// 注：本测试通过 facade 的公开 API 验证整体行为（facade.C2 ∪ backend.C2 兜底），
// 不严格区分"是 facade 还是 backend 提供的同步兜底"——但因为 facade.PostTask
// 先于 backend.PostTask 触发（facade 检查 shutdown_ 优先），只要本测试通过，
// facade 合同就被覆盖。Backend 自身是否带 C2 兜底是其内部实现细节。
TEST(ConcurrentLoopBackendFacadeContract,
     C2ShutdownFallbackRunsOnCaller) {
  auto loop = std::make_shared<ConcurrentMessageLoop>("c2-test",
                                                   Thread::ThreadPriority::NORMAL,
                                                   1);
  loop->Terminate();

  std::atomic<bool> ran{false};
  std::atomic<std::thread::id> ran_on_id{};
  const std::thread::id caller_id = std::this_thread::get_id();

  loop->PostTask([&] {
    ran.store(true);
    ran_on_id.store(std::this_thread::get_id());
  });

  // 同步语义：PostTask 返回时 task 必已执行
  EXPECT_TRUE(ran.load());
  // 必在调用方线程跑
  EXPECT_EQ(ran_on_id.load(), caller_id);
  // 主线程不在 backend worker 上下文
  EXPECT_FALSE(loop->RunsTasksOnCurrentThreadWorker());
}

}  // namespace
}  // namespace fml
}  // namespace lynx
