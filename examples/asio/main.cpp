#include "mgutility/task.hpp"

#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <iostream>

using mgutility::task;

int main() {

  asio::thread_pool pool(1);
  std::cout << "Starting ASIO thread pool with 1 thread...\n";

  // Example 1: Simple task
  task<void> t1([] { std::cout << "[Task1] Hello from task 1!\n"; });
  asio::post(pool, t1.get_invoker());

  // Example 2: Chained tasks
  auto t2 = mgutility::make_task([] {
    std::cout << "[Task2] First part done, scheduling second part...\n";
    return 42;
  });

  auto t2b = t2.then([](std::shared_future<int> f) {
    std::cout << "[Task2] Second part received value: " << f.get() << "\n";
  });

  asio::post(pool, t2.get_invoker());

  // Example 3: Cancelled task
  auto t3 =
      task<void>([] { std::cout << "[Task3] You should not see this!\n"; });
  asio::post(pool, t3.get_invoker());

  // Cancel t3 immediately (before it runs)
  std::cout << "[Task3] Cancelling task3 before it runs.\n";
  if (t3.cancel()) {
    std::cout << "[Task3] Task cancelled successfully.\n";
  } else {
    std::cout << "[Task3] Task was not pending or already finished, nothing to "
                 "cancel.\n";
  }

  // Example 4: Multiple tasks
  for (int i = 0; i < 3; ++i) {
    auto t = mgutility::make_task(
        [i] { std::cout << "[Task4] Task #" << i << " done!\n"; });
    asio::post(pool, t.get_invoker());
  }

  // Example 5: Task with a return value
  auto t5 = mgutility::make_task([] {
    std::cout << "[Task5] Performing some computation...\n";
    return 3.14;
  });

  auto t5b = t5.then([](std::shared_future<double> f) {
    std::cout << "[Task5] Computation result: " << f.get() << "\n";
  });

  asio::post(pool, t5.get_invoker());

  pool.wait();
  pool.join();
  std::cout << "All tasks processed.\n";
  return 0;
}
