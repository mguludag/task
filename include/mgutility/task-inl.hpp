#ifndef __MGUTILITY_TASK_INL_H__
#define __MGUTILITY_TASK_INL_H__

#include <atomic>
#include <exception>
#include <mutex>
#include <system_error>

namespace mgutility
{

template <typename T, typename Tag, typename Lockable>
struct task<T, Tag, Lockable>::task_impl
{
    using function_type = std::function<T()>;
    using promise_type = std::promise<T>;
    using future_type = std::shared_future<T>;

    explicit task_impl(const function_type& f) noexcept : func_(f), future_(promise_.get_future().share()) {}

    task_impl(const task_impl&) = delete;
    auto operator=(const task_impl&) -> task_impl& = delete;
    task_impl(task_impl&&) = default;
    auto operator=(task_impl&&) -> task_impl& = default;
    ~task_impl() = default;

    auto invoke() -> void
    {
        if (!is_pending())
            return;
        try
        {
            is_running_flag_ = true;
            std::lock_guard<lockable_type> guard(func_mutex_);
            task<T, Tag, Lockable>::run_impl(func_, promise_);
        }
        catch (...)
        {
            promise_.set_exception(std::current_exception());
        }
        clear_func_and_run_continuation();
    }

    auto get_future() -> future_type& { return future_; }
    auto is_running() const -> bool { return is_running_flag_.load(); }
    auto is_pending() const -> bool
    {
        if (is_running())
            return false;
        std::lock_guard<lockable_type> guard(func_mutex_);
        return func_ != nullptr;
    }
    auto cancel() -> bool
    {
        if (!is_pending())
            return false;
        reset();
        promise_.set_exception(std::make_exception_ptr(
            std::system_error{std::make_error_code(std::errc::operation_canceled), func_.target_type().name()}));
        clear_func_and_run_continuation();
        return true;
    }
    auto reset() -> void
    {
        is_running_flag_ = false;
        std::lock_guard<lockable_type> guard(func_mutex_);
        func_ = nullptr;
    }
    auto set_continuation(const std::function<void()>& continuation_func) -> void { continuation_ = continuation_func; }

private:
    auto clear_func_and_run_continuation() -> void
    {
        reset();

        std::lock_guard<lockable_type> guard(continuation_mutex_);
        if (continuation_)
        {
            continuation_();
            continuation_ = nullptr;
        }
    }

    std::atomic_bool is_running_flag_{false};
    mutable lockable_type func_mutex_;
    function_type func_;
    promise_type promise_;
    future_type future_;
    mutable lockable_type continuation_mutex_;
    std::function<void()> continuation_;
};

// --- task<T, Tag, Lockable> member functions ---

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::task() = default;

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::task(const function_type& func)
    : cancel_at_exit_{true}, impl_(std::make_shared<task_impl>(func))
{
}

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::task(const task& other) noexcept : impl_(other.impl_)
{
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::operator=(const task& other) noexcept -> task&
{
    if (this != &other)
        impl_ = other.impl_;
    return *this;
}

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::task(task&& other) noexcept : impl_(std::move(other.impl_))
{
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::operator=(task&& other) noexcept -> task&
{
    if (this != &other)
        impl_ = std::move(other.impl_);
    return *this;
}

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::~task()
{
    if (cancel_at_exit_)
        cancel();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::cancel() const -> bool
{
    return impl_ && impl_->cancel();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::get_future() const -> future_type
{
    if (!impl_)
        throw std::runtime_error("Task is not initialized.");
    return impl_->get_future();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::wait() const -> void
{
    get_future().wait();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::get() const -> T
{
    return get_future().get();
}

template <typename T, typename Tag, typename Lockable>
template <typename Rep, typename Period>
auto task<T, Tag, Lockable>::wait_for(const std::chrono::duration<Rep, Period>& duration) const -> std::future_status
{
    return get_future().wait_for(duration);
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::is_pending() const -> bool
{
    return impl_ && impl_->is_pending();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::is_valid() const -> bool
{
    return impl_ != nullptr;
}

// --- invoker ---

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::invoker::invoker(task& t) noexcept : impl_(t.impl_)
{
}

template <typename T, typename Tag, typename Lockable>
task<T, Tag, Lockable>::invoker::invoker(std::shared_ptr<task_impl> impl) noexcept : impl_(std::move(impl))
{
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::invoker::operator()() -> void
{
    if (impl_)
        impl_->invoke();
}

template <typename T, typename Tag, typename Lockable>
auto task<T, Tag, Lockable>::get_invoker() -> invoker
{
    return invoker{*this};
}

// --- then/then_impl ---

template <typename T, typename Tag, typename Lockable>
template <typename Func, typename U>
auto task<T, Tag, Lockable>::then(Func continuation)
    -> detail::enable_if_t<!detail::is_void<U>::value, task<detail::invoke_result_t<Func, std::shared_future<T>>>>
{
    using future_type = std::shared_future<T>;
    using return_type = detail::invoke_result_t<Func, future_type>;
    return then_impl<Func, future_type, return_type>(std::move(continuation));
}

template <typename T, typename Tag, typename Lockable>
template <typename Func, typename U>
auto task<T, Tag, Lockable>::then(Func continuation)
    -> detail::enable_if_t<detail::is_void<U>::value, task<detail::invoke_result_t<Func, std::shared_future<void>>>>
{
    using future_type = std::shared_future<void>;
    using return_type = detail::invoke_result_t<Func, future_type>;
    return then_impl<Func, future_type, return_type>(std::move(continuation));
}

// Helper for pre-C++17: tag dispatch for void/non-void return
template <typename ReturnType, typename Func, typename FutureType>
static typename std::enable_if<!detail::is_void<ReturnType>::value, ReturnType>::type
call_continuation(Func& continuation, FutureType& prev_future)
{
    return continuation(prev_future);
}

template <typename ReturnType, typename Func, typename FutureType>
static typename std::enable_if<detail::is_void<ReturnType>::value, void>::type
call_continuation(Func& continuation, FutureType& prev_future)
{
    continuation(prev_future);
}

template <typename T, typename Tag, typename Lockable>
template <typename Func, typename FutureType, typename ReturnType>
auto task<T, Tag, Lockable>::then_impl(Func continuation) -> task<ReturnType>
{
    using next_task_t = task<ReturnType>;

    if (!impl_ || !get_future().valid())
        throw std::runtime_error("Invalid task");

    auto prev_future = this->get_future();

    next_task_t next_task([prev_future, continuation]() mutable -> ReturnType
                          { return call_continuation<ReturnType>(continuation, prev_future); });

    auto next_invoker = next_task.get_invoker();

    impl_->set_continuation([next_invoker]() mutable -> void { next_invoker(); });

    if (!impl_->is_pending() && get_future().valid())
        next_task.impl_->invoke();

    return next_task;
}

template <typename T, typename Tag, typename Lockable>
template <typename U>
void task<T, Tag, Lockable>::run_impl(std::function<U()>& func, std::promise<U>& promise)
{
    if (!func)
        return;
    promise.set_value(func());
}

template <typename T, typename Tag, typename Lockable>
void task<T, Tag, Lockable>::run_impl(std::function<void()>& func, std::promise<void>& promise)
{
    if (!func)
        return;
    func();
    promise.set_value();
}

} // namespace mgutility
#endif // __MGUTILITY_TASK_INL_H__