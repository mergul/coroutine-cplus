#include <functional>
#include <type_traits>
#include <tuple>
#include <new>
#include <optional>
#include <exception>
#include <concepts>

extern "C" int puts(char const*) noexcept;
using std::invoke_result_t;

// C++20 concepts
template<class T, class... As>
concept constructible_from = std::is_constructible_v<T, As...>;

template<class T>
concept move_constructible = constructible_from<T, T>;

template<class T>
concept copy_constructible =
    move_constructible<T> &&
    constructible_from<T, T const&>;

template<class A, class B>
concept derived_from = std::is_base_of_v<B, A>;

template<class F, class... As>
concept invocable = requires {
    typename invoke_result_t<F, As...>;
};

// Generic sender/receiver customization points and concept
// definitions, simplified (not to spec).
inline namespace _receiver_cpos_ {
    inline constexpr struct _set_value_fn_ {
        template<class R, class... As>
            requires requires(R&& r, As&&... as) {
                ((R&&)r).set_value((As&&) as...);
            }
        void operator()(R&& r, As&&... as) const
            noexcept(noexcept(((R&&)r).set_value((As&&) as...))) {
            ((R&&)r).set_value((As&&) as...);
        }
    } set_value{};

    inline constexpr struct _set_error_fn_ {
        template<class R, class E>
            requires requires(R&& r, E&& e) {
                {((R&&)r).set_error((E&&) e)} noexcept;
            }
        void operator()(R&& r, E&& e) const noexcept {
            ((R&&)r).set_error((E&&) e);
        }
    } set_error{};

    inline constexpr struct _set_done_fn_ {
        template<class R>
            requires requires(R&& r) {
                {((R&&)r).set_done()} noexcept;
            }
        void operator()(R&& r) const noexcept {
            ((R&&)r).set_done();
        }
    } set_done{};
}

template<class R, class E = std::exception_ptr>
concept receiver =
    move_constructible<std::remove_cvref_t<R>> &&
    requires (R&& r, E&& e) {
        ::set_error((R&&) r, (E&&) e);
        ::set_done((R&&) r);
    };

template<class R,  class... As>
concept receiver_of =
    receiver<R> &&
    requires (R&& r, As&&... as) {
        ::set_value((R&&) r, (As&&) as...);
    };

template<class R, class... As>
inline constexpr bool is_nothrow_receiver_of_v =
    noexcept(::set_value(std::declval<R>(), std::declval<As>()...));

struct sender_base {};

template<class S>
struct sender_traits;

template<class S>
    requires derived_from<S, sender_base>
struct sender_traits<S> {};

template<class S>
concept sender =
    move_constructible<std::remove_cvref_t<S>> &&
    requires {
        // Opt-in to sender-ness by specializing sender_traits
        sizeof(sender_traits<std::remove_cvref_t<S>>);
    };

static_assert(!sender<int>);

inline namespace _sender_cpos_ {
    inline constexpr struct _start_fn_ {
        template<class O>
            requires requires(O&& o) {
                {((O&&)o).start()} noexcept;
            }
        void operator()(O&& o) const noexcept {
            ((O&&)o).start();
        }
    } start{};
}

template<class O>
concept operation_state =
    std::is_object_v<O> &&
    requires (O&& o) {
        ::start((O&&)o);
    };

inline namespace _sender_cpos_ {
    inline constexpr struct _connect_fn_ {
        template<sender S, receiver R>
            requires requires(S&& s, R&& r) {
                {((S&&)s).connect((R&&) r)} -> operation_state;
            }
        auto operator()(S&& s, R&& r) const
            noexcept(noexcept(((S&&)s).connect((R&&)r))) {
            return ((S&&)s).connect((R&&)r);
        }
    } connect{};
}

template<class S, class R>
concept sender_to =
    sender<S> &&
    receiver<R> &&
    requires(S&& s, R&& r) {
        ::connect((S&&)s, (R&&)r);
    };

template<class S, class R>
    requires sender_to<S, R>
using state_t =
    decltype(::connect(std::declval<S>(), std::declval<R>()));

/////////////////////////////////////////////////////////////////////////
// retry algorithm implementation begins here

// _conv needed so we can emplace construct non-movable types into
// a std::optional.
template<invocable F>
    requires std::is_nothrow_move_constructible_v<F>
struct _conv {
    F f_;
    explicit _conv(F f) noexcept : f_((F&&) f) {}
    operator invoke_result_t<F>() && {
        return ((F&&) f_)();
    }
};

// pass through set_value and set_error, but retry the operation
// from set_error.
template<class O, class R>
struct _retry_receiver {
    O* o_;
    explicit _retry_receiver(O* o): o_(o) {}
    template<class... As>
        requires receiver_of<R, As...>
    void set_value(As&&... as) &&
        noexcept(is_nothrow_receiver_of_v<R, As...>) {
        ::set_value(std::move(o_->r_), (As&&) as...);
    }
    void set_error(auto&&) && noexcept {
        o_->_retry(); // This causes the op to be retried
    }
    void set_done() && noexcept {
        ::set_done(std::move(o_->r_));
    }
};

template<sender S>
struct _retry_sender : sender_base {
    S s_;
    explicit _retry_sender(S s): s_((S&&) s) {}

    // Hold the nested operation state in an optional so we can
    // re-construct and re-start it when the operation fails.
    template<receiver R>
    struct _op {
        S s_;
        R r_;
        std::optional<state_t<S&, _retry_receiver<_op, R>>> o_;

        _op(S s, R r): s_((S&&)s), r_((R&&)r), o_{_connect()} {}
        _op(_op&&) = delete;

        auto _connect() noexcept {
            return _conv{[this] {
                return ::connect(s_, _retry_receiver<_op, R>{this});
            }};
        }
        void _retry() noexcept try {
            o_.emplace(_connect()); // potentially throwing
            ::start(std::move(*o_));
        } catch(...) {
            ::set_error((R&&) r_, std::current_exception());
        }
        void start() && noexcept {
            ::start(std::move(*o_));
        }
    };

    template<receiver R>
        requires sender_to<S&, _retry_receiver<_op<R>, R>>
    auto connect(R r) && -> _op<R> {
        return _op<R>{(S&&) s_, (R&&) r};
    }
};

template<sender S>
sender auto retry(S s) {
    return _retry_sender{(S&&)s};
}

// retry algorithm implementation ends here
/////////////////////////////////////////////////////////////////////////

inline constexpr struct _sink {
    void set_value(auto&&...) const noexcept {}
    [[noreturn]] void set_error(auto&&) const noexcept {
        std::terminate();
    }
    [[noreturn]] void set_done() const noexcept {
        std::terminate();
    }
} sink{};

// Here is a test sender that fails the first three times it is
// started and then succeeds on the fourth try.
struct fail_3 : sender_base {
    int count_ = 0;

    template<receiver_of R>
    struct _op {
        int const count_;
        R r_;
        _op(int count, R r): count_(count), r_((R&&) r) {}
        _op(_op&&) = delete;
        void start() && noexcept try {
            if(count_ > 3) {
                ::puts("success");
                ::set_value((R&&) r_, count_);
            } else {
                ::puts("error");
                ::set_error((R&&) r_, 42);
            }
        } catch(...) {
            ::set_error((R&&) r_, std::current_exception());
        }
    };

    template<receiver_of R>
        requires receiver<R, int>
    auto connect(R r) & -> _op<R> {
        return _op<R>{++count_, (R&&) r};
    }
};

int main() {
    fail_3 s;
    ::start(::connect(retry(s), sink));
}







// exposition only:
template<class T>
inline constexpr bool is-nothrow-move-or-copy-constructible =
  is_nothrow_move_constructible<T> ||
   copy_constructible<T>;

template<class T, class E = exception_ptr>
concept receiver =
  move_constructible<remove_cvref_t<T>> &&
  (is-nothrow-move-or-copy-constructible<remove_cvref_t<T>>) &&
  requires(T&& t, E&& e) {
    { execution::set_done((T&&) t) } noexcept;
    { execution::set_error((T&&) t, (E&&) e) } noexcept;
  };

  template<class T, class... An>
concept receiver_of =
  receiver<T> &&
  requires(T&& t, An&&... an) {
    execution::set_value((T&&) t, (An&&) an...);
  };

  template<class S, class R>
concept sender-to-impl =
  requires(S&& s, R&& r) {
    execution::submit((S&&) s, (R&&) r);
  };

template<class S>
concept sender =
  move_constructible<remove_cvref_t<S>> &&
  sender-to-impl<S, sink_receiver>;

template<class S, class R>
concept sender_to =
  sender<S> &&
  receiver<R> &&
  sender-to-impl<S, R>;

template<class S>
concept scheduler =
  copy_constructible<remove_cvref_t<S>> &&
  equality_comparable<remove_cvref_t<S>> &&
  requires(E&& e) {
    execution::schedule((S&&)s);
  }; // && sender<invoke_result_t<execution::schedule, S>>



template<template<template<class...> class Tuple, template<class...> class Variant> class>
struct has-value-types; // exposition only

template<template<class...> class Variant>
struct has-error-types; // exposition only

template<class S>
comcept has-sender-types = // exposition only
  requires {
    typename has-value-types<S::template value_types>;
    typename has-error-types<S::template error_types>;
    typename bool_constant<S::sends_done>;
  };

template<class S>
concept typed_sender =
  sender<S> &&
  has-sender-types<sender_traits<S>>;


  template<class E, class F>
concept executor-of-impl =
  invocable<F> &&
  is_nothrow_copy_constructible_v<E> &&
  is_nothrow_destructible_v<E> &&
  equality_comparable<E> &&
  requires(const E& e, F&& f) {
    execution::execute(e, (F&&)f);
  };

template<class E>
concept executor = executor-of-impl<E, execution::invocable_archetype>;

template<class E, class F>
concept executor_of = executor-of-impl<E, F>;

class sink_receiver {
    public:
      void set_value(auto&&...) {}
      [[noreturn]] void set_error(auto&&) noexcept {
        std::terminate();
      }
      void set_done() noexcept {}
    };


class static_thread_pool
{
  public:
    using scheduler_type = see-below;
    using executor_type = see-below;
    
    // construction/destruction
    explicit static_thread_pool(std::size_t num_threads);
    
    // nocopy
    static_thread_pool(const static_thread_pool&) = delete;
    static_thread_pool& operator=(const static_thread_pool&) = delete;

    // stop accepting incoming work and wait for work to drain
    ~static_thread_pool();

    // attach current thread to the thread pools list of worker threads
    void attach();

    // signal all work to complete
    void stop();

    // wait for all threads in the thread pool to complete
    void wait();

    // placeholder for a general approach to getting schedulers from 
    // standard contexts.
    scheduler_type scheduler() noexcept;

    // placeholder for a general approach to getting executors from 
    // standard contexts.
    executor_type executor() noexcept;
};