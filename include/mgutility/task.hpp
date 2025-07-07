/*
 * MIT License
 *
 * Copyright (c) 2024 mguludag
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __MGUTILITY_TASK_H__
#define __MGUTILITY_TASK_H__

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>

namespace mgutility {

namespace detail {
#ifdef _MSC_VER
#ifndef MGUTILITY_CPLUSPLUS
#define MGUTILITY_CPLUSPLUS _MSVC_LANG
#endif
#else
#ifndef MGUTILITY_CPLUSPLUS
#define MGUTILITY_CPLUSPLUS __cplusplus
#endif
#endif

template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;

#if MGUTILITY_CPLUSPLUS < 201703L
template <typename F, typename... Args>
using invoke_result_t = typename std::result_of<F(Args...)>::type;
#else
template <typename F, typename... Args>
using invoke_result_t = typename std::invoke_result<F, Args...>::type;
#endif

template <typename T> using is_void = std::is_same<T, void>;
} // namespace detail

/**
 * @brief A generic, composable asynchronous task abstraction.
 *
 * The `task<T>` class provides a mechanism for representing asynchronous
 * computations, similar to `std::future` and `std::promise`, but with
 * additional support for continuations (via `then()`), cancellation, and
 * chaining.
 *
 * @tparam T   The result type produced by the task.
 * @tparam Tag An optional tag type for distinguishing tasks (default: void).
 *
 * ## Concepts
 * - **Continuation**: You can attach further work to a task using `then()`,
 * which creates a new task that will execute after the current one completes.
 * - **Cancellation**: Tasks can be cancelled before execution, propagating an
 * exception.
 * - **Invoker**: The `invoker` type allows explicit invocation of the task's
 * function.
 * - **Shared Future**: Internally, tasks use `std::shared_future` to allow
 * multiple continuations to observe the result.
 *
 * ## Example
 * @code
 * task<int> t([] { return 42; });
 * auto t2 = t.then([](std::shared_future<int> f) { return f.get() + 1; });
 * t.get_invoker()(); // Start the chain
 * int result = t2.get();
 * @endcode
 *
 * ## Usage with Thread Pools (e.g., asio::thread_pool)
 * You can schedule task invocations on a thread pool such as
 * `asio::thread_pool`:
 * @code
 * #include <asio/thread_pool.hpp>
 * #include <asio/post.hpp>
 *
 * asio::thread_pool pool(4);
 * mgutility::task<int> t([] { return 42; });
 * auto t2 = t.then([](std::shared_future<int> f) { return f.get() + 1; });
 * asio::post(pool, t.get_invoker());
 * pool.join();
 * int result = t2.get();
 * @endcode
 */
template <typename T, typename Tag = void, typename Lockable = std::mutex>
class task {
  struct task_impl;
  template <typename T_, typename Tag_, typename Lockable_> friend class task;

public:
  /// The result type of the task.
  using value_type = T;
  /// The tag type for distinguishing tasks.
  using tag_type = Tag;
  /// The lockable type used for synchronization (default: std::mutex).
  using lockable_type = Lockable;
  /// The function type executed by the task.
  using function_type = std::function<T()>;
  /// The shared future type for result access.
  using future_type = std::shared_future<T>;

  /**
   * @brief Default-construct an empty task.
   *
   * The task will be invalid until assigned or constructed with a function.
   */
  task();

  /**
   * @brief Construct a task from a function.
   * @param func The function to execute asynchronously.
   *
   * The function will be executed when the task is invoked.
   */
  explicit task(const function_type &func);

  /**
   * @brief Copy-construct a task (shares the same state).
   * @param other The task to copy from.
   */
  task(const task &other) noexcept;

  /**
   * @brief Copy-assign a task (shares the same state).
   * @param other The task to copy from.
   * @return Reference to this task.
   */
  auto operator=(const task &other) noexcept -> task &;

  /**
   * @brief Move-construct a task.
   * @param other The task to move from.
   */
  task(task &&other) noexcept;

  /**
   * @brief Move-assign a task.
   * @param other The task to move from.
   * @return Reference to this task.
   */
  auto operator=(task &&other) noexcept -> task &;

  /**
   * @brief Destructor. Cancels the task if not yet executed.
   */
  ~task() noexcept;

  /**
   * @brief Cancel the task if it is pending.
   * @return true if the task was cancelled, false otherwise.
   *
   * If the task is pending, it will be cancelled and any waiting continuations
   * will receive an exception.
   */
  auto cancel() const -> bool;

  /**
   * @brief Get the shared future associated with the task.
   * @throws std::runtime_error if the task is not initialized.
   * @return The shared future for the task's result.
   */
  auto get_future() const -> future_type;

  /**
   * @brief Wait for the task to complete.
   */
  auto wait() const -> void;

  /**
   * @brief Get the result of the task (waits if necessary).
   * @return The result value.
   */
  auto get() const -> T;

  /**
   * @brief Wait for the task to complete for a given duration.
   * @param duration The maximum duration to wait.
   * @return The status of the wait operation.
   */
  template <typename Rep, typename Period>
  auto wait_for(const std::chrono::duration<Rep, Period> &duration) const
      -> std::future_status;

  /**
   * @brief Check if the task is pending execution.
   * @return true if pending, false otherwise.
   */
  auto is_pending() const -> bool;

  /**
   * @brief Check if the task is valid (initialized).
   * @return true if valid, false otherwise.
   */
  auto is_valid() const -> bool;

  /**
   * @brief Invoker for a task, allowing explicit invocation.
   *
   * The invoker holds a reference to the task's implementation and can be
   * called to execute the task's function. This is useful for explicit control
   * over when the task starts.
   */
  struct invoker {
    /**
     * @brief Construct an invoker from a task.
     * @param t The task to invoke.
     */
    explicit invoker(task &t) noexcept;
    /**
     * @brief Construct an invoker from a shared implementation.
     * @param impl The shared implementation pointer.
     */
    explicit invoker(std::shared_ptr<task_impl> impl) noexcept;
    /**
     * @brief Invoke the task's function if it is pending.
     */
    auto operator()() const -> void;

  private:
    std::shared_ptr<task_impl> impl_;
  };

  /**
   * @brief Get an invoker for this task.
   * @return An invoker object.
   */
  auto get_invoker() const -> invoker;

  /**
   * @brief Attach a continuation to this task (for non-void result).
   *
   * The continuation receives the shared future of this task and returns a new
   * value. The returned task represents the continuation.
   *
   * @tparam Func The continuation function type.
   * @return A new task representing the continuation.
   */
  template <typename Func, typename U = T>
  auto then(Func continuation) const -> detail::enable_if_t<
      !detail::is_void<U>::value,
      task<detail::invoke_result_t<Func, std::shared_future<T>>>>;

  /**
   * @brief Attach a continuation to this task (for void result).
   *
   * The continuation receives the shared future of this task and returns a new
   * value. The returned task represents the continuation.
   *
   * @tparam Func The continuation function type.
   * @return A new task representing the continuation.
   */
  template <typename Func, typename U = T>
  auto then(Func continuation) const -> detail::enable_if_t<
      detail::is_void<U>::value,
      task<detail::invoke_result_t<Func, std::shared_future<void>>>>;

private:
  /// Internal implementation for then().
  template <typename Func, typename FutureType, typename ReturnType>
  auto then_impl(Func continuation) const -> task<ReturnType>;

  /// Helper to run a function and set the promise (non-void).
  static void run_impl(std::function<void()> &func,
                       std::promise<void> &promise);
  template <typename U>
  static void run_impl(std::function<U()> &func, std::promise<U> &promise);

  const bool cancel_at_exit_{
      true}; ///< Whether to cancel the task on destruction.
  std::shared_ptr<task_impl> impl_; ///< Shared implementation pointer.
};

/**
 * @brief Create a task from a callable function.
 *
 * This function wraps a callable into a `task` object, allowing it to be
 * executed asynchronously. The task can be invoked using an invoker or
 * scheduled on an executor.
 *
 * @tparam Tag An optional tag type for the task (default: void).
 * @tparam Lockable The lockable type used for synchronization (default:
 * std::mutex).
 * @tparam F The callable type (function, lambda, etc.).
 * @param func The callable to wrap.
 * @return A task object representing the callable.
 */
template <typename Tag = void, typename Lockable = std::mutex, typename F>
auto make_task(F &&func) -> task<detail::invoke_result_t<F>, Tag, Lockable> {
  return task<detail::invoke_result_t<F>, Tag, Lockable>(std::forward<F>(func));
}

#if MGUTILITY_CPLUSPLUS >= 201703L
/**
 * @brief Create a task from a callable function with optional tag and lockable.
 *
 * This function is a convenience overload that allows specifying a tag and
 * lockable type for the task.
 *
 * @tparam Tag An optional tag type for the task (default: void).
 * @tparam Lockable The lockable type used for synchronization (default:
 * std::mutex).
 * @tparam F The callable type (function, lambda, etc.).
 * @param func The callable to wrap.
 * @return A task object representing the callable.
 */
template <typename Tag = void, typename Lockable = std::mutex, typename F>
task(F &&func) -> task<detail::invoke_result_t<F>, Tag, Lockable>;
#endif // MGUTILITY_CPLUSPLUS >= 201703L

} // namespace mgutility

#include "task-inl.hpp"
#endif // __MGUTILITY_TASK_H__