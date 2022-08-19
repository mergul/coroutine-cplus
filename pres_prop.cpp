#include <functional>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <optional>
#include <exception>
#include <variant>
#include <iostream>
#include <mutex>
#include <utility>
#include <initializer_list>
#include <condition_variable>
#include <type_traits>
#include <cstddef>
#include <array>
#include <vector>
#include <tuple>
#include <atomic>


using namespace std;
template <class T>
struct _state
{
    std::mutex mtx;
    std::condition_variable cv;
    std::variant<std::monostate, std::exception_ptr, T> data;
};

template <class T>
struct _promise
{
    _state<T> *pst;
    template <int I>
    void _set(auto... xs)
    {
        auto lk = std::unique_lock{pst->mtx};
        pst->data.template emplace<I>(xs...);
        pst->cv.notify_one();
    }
    void set_value(auto... vs) { _set<2>(vs...); }
    void set_exception(auto e) { _set<1>(e); }
};

auto then(auto task, auto fun)
{
    return [=](auto p) {
        struct _promise
        {
            decltype(p) p_;
            decltype(fun) fun_;
            // void set_value(int vs) { p_.set_value((vs)); }
            void set_value(auto... vs) { p_.set_value(fun_(vs...)); }
            void set_exception(exception _e) { p_.set_exception(_e); }
        };
        task(_promise{p, fun});
    };
}

template <class T, class Task>
T sync_wait(Task task)
{ // Define some state:
    _state<T> state;
    // launch the operation:
    task(_promise<T>{&state});
    // wait for it to finish:
    {
        auto lk = std::unique_lock{state.mtx};
        state.cv.wait(lk, [&state] { return state.data.index() != 0; });
    }
    // throw or return the result:
    if (state.data.index() == 1)
        std::rethrow_exception(get<1>(state.data));
    return std::move(get<2>(state.data));
}

auto new_thread()
{
    return [](auto p) {
        std::thread t{[p = move(p)]() mutable {
            p.set_value();
        }};
        t.detach();
    };
}

auto async_algo(auto task)
{
    return then(task, []() { return 6 + rand(); });
}

int main()
{
    auto f = async_algo(new_thread());
    auto f2 = then(f, [](int i) { return i + rand(); });
    printf("%d\n", sync_wait<int>(f2));
}