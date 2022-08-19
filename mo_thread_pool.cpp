#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <iostream>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <chrono>


template <typename... Ts>
void display(Ts... ts)
{
    static std::mutex m;
    auto lock = std::lock_guard{m};
    std::cout << "this_thread: " << std::this_thread::get_id() << ": ";
    ((std::cout << ts << " "), ...);
    std::cout << std::endl;
}

#define FWD(x) std::forward<decltype(x)>(x)

template <typename T>
class Mutex
{
public:
    template <typename F>
    decltype(auto) with_lock(F &&f)
    {
        std::lock_guard lock{m_mutex};
        return std::invoke(FWD(f), m_value);
    }
    template <typename F>
    decltype(auto) with_lock(F &&f) const
    {
        std::shared_lock lock{m_mutex};
        return std::invoke(FWD(f), m_value);
    }

protected:
    T m_value;
    mutable std::shared_mutex m_mutex;
};

template <typename T>
class ConditionVariable : public Mutex<T>
{
public:
    void notifyOne() { m_cond.notify_one(); }
    void notifyAll() { m_cond.notify_all(); }

    template <typename F>
    void wait(F f)
    {
        auto lock = std::unique_lock{this->m_mutex};
        m_cond.wait(lock, [&, this] { return std::invoke(f, this->m_value); });
    }

    template <typename F>
    void wait(F f, std::stop_token st)
    {
        auto lock = std::unique_lock{this->m_mutex};
        m_cond.wait(lock, st, [&, this] { return std::invoke(f, this->m_value); });
    }

private:
    std::condition_variable_any m_cond;
};

template <typename T>
class ThreadSafeQueue
{
public:
    void push(T t)
    {
        m_queue.with_lock([&](auto &queue) { queue.push(std::move(t)); });
        m_queue.notifyOne();
    }

    std::optional<T> pop()
    {
        std::optional<T> x;
        m_queue.with_lock([&](auto &queue) {
            if (!queue.empty())
            {
                x.emplace(std::move(queue.front()));
                queue.pop();
            }
        });
        return x;
    }

    void waitForAnElement(std::stop_token st)
    {
        auto hasElement = [](const auto &x) { return !x.empty(); };
        m_queue.wait(hasElement, st);
    }

private:
    ConditionVariable<std::queue<T>> m_queue;
};

struct Awaiter
{
public:
    Awaiter() {}

    ~Awaiter()
    {
        if (m_handle)
            m_handle.destroy();
    }

    template <typename... Args>
    Awaiter(std::coroutine_handle<Args...> handle) : m_handle{handle} {}

    Awaiter(const Awaiter &) = delete;
    Awaiter(Awaiter &&a) : m_handle{a.m_handle} { a.m_handle = nullptr; }

    void resume()
    {
        m_handle();
        m_handle = nullptr;
    }

private:
    std::coroutine_handle<> m_handle = nullptr;
};

class Thread
{
    struct Awaitable
    {
        Thread &thread;
        bool await_ready() { return false; }

        void await_suspend(std::coroutine_handle<> handle)
        {
            thread.addAwaiter({handle});
        }

        void await_resume() {}
    };

public:
    Thread()
    {
        m_thread = std::jthread([this](std::stop_token st) { run(st); });
    }

    void addAwaiter(Awaiter &&awaiter) { m_awaiters.push(std::move(awaiter)); }

    auto id() { return m_thread.get_id(); }

    Awaitable operator co_await() { return {*this}; }

private:
    void run(std::stop_token st)
    {
        while (!st.stop_requested())
        {
            m_awaiters.waitForAnElement(st);

            auto awaiter = m_awaiters.pop();

            if (awaiter)
                awaiter->resume();
        }
    }

  private:
    ThreadSafeQueue<Awaiter> m_awaiters;
    std::jthread m_thread;
};

struct task
{
    struct promise_type
    {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() noexcept {}

        ~promise_type() {}
    };

    ~task() {}
};

std::atomic_bool done = false;

task f(Thread &thread1, Thread &thread2)
{
    co_await thread1;
    display("This is the thread1");

    co_await thread2;
    display("This is the thread2");
    done = true;
}

int main()
{
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    Thread a, b;
    display("This is the main thread");
    f(a, b);
    while (!done)
        ;
    auto end = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast < std::chrono::milliseconds > (end - start).count();
    std::cout << "\nTotal Time Taken = " << diff << " MilliSeconds" << std::endl;

    return 0;
}
