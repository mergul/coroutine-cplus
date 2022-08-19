/**********************************************************
 *
 * As in P0443, oneway execute is a function that takes
 * a nullary callable, creates an execution agent to execute
 * it, and returns void. What is new is the ability to pass
 * an enriched callable to execute that, in addition to being
 * a nullary callable, also has .error() and .done() members.
 * The concept that enriched callables model is named "Callback"
 * and it subsumes Invocable.
 *
 * concept Invocable<C, An...> { // (already standard)
 *   void operator()(An...);
 * }
 *
 * concept CallbackSignal<C, E = std::exception_ptr> {
 *   void done() noexcept;
 *   void error(E) noexcept;
 * }
 *
 * concept Callback<C, An...> {
 *   requires Invocable<C, An...> && CallbackSignal<C>;
 * }
 *
 * concept Executor< E, C [=void(*)()] > {
 *   requires Invocable< C >;
 *   void execute(C);
 * }
 *
 * Authors of oneway executors can accept and execute simple
 * callables and use an implementation-defined scheme to deal
 * with errors. If passed a Callback, an executor should send
 * errors to the Callback's .error() channel. This is made
 * simple with a invoke_callback helper that invokes the
 * callable inline, routing errors to the error channel if the
 * callable is in fact a Callback.
 *
 * Here, for example, is the inline_executor:
 * 
 *   struct inline_executor {
 *     template<Invocable C>
 *     void execute(C c) {
 *       std::tbd::invoke_callback(std::move(c));
 *     }
 *   };
 *
 * With Callbacks instead of raw Invocables, we can implement
 * Sender/Receiver in a way that is compatible with oneway
 * executors. It is possible to define the submit() and
 * schedule() customization points such that a oneway executor,
 * when used with a Callback, satisfies the requirements of
 * Sender and even Scheduler. I show the code below the break.
 *
 * The final section demonstrates how Sender/Callback/Scheduler
 * would be defined in the presence of Executor/Callback
 * 
 **********************************************************/

#include <string>
#include <utility>
#include <type_traits>
#include <functional>
#include <exception>
#include <stdexcept>
#include <concepts>

namespace std {
template<class A, class B>
concept Same = __is_same(A, B) && __is_same(B, A);

template<class C, class... An>
concept Invocable = requires (C&& c, An&&... an) {
  std::invoke((C&&) c, (An&&) an...);
};

namespace tbd {
// A CallbackSignal is something with .done() and
// .error() member functions.
template<class T, class E = exception_ptr>
concept CallbackSignal =
  requires (T&& t, E&& e) {
    {((T&&) t).done()} noexcept;
    {((T&&) t).error(((E&&) e))} noexcept;
  };

// A Callback is an Invocable that is also a
// CallbackSignal.
template<class T, class... An>
concept Callback =
  Invocable<T, An...> &&
  CallbackSignal<T>;

// A Executor is something with an .execute()
// member function that takes a nullary callable.
template<class T, class C = void(*)()>
concept Executor =
  Invocable<C> &&
  requires (T&& t, C&& c) {
    ((T&&) t).execute((C&&) c);
  };

// Definition in the stdlib somewhere:
extern runtime_error const invocation_error_; // = "invocation error";

// All exceptions from invocations of Callbacks are
// wrapped in callback_invocation_error:
struct callback_invocation_error : runtime_error, nested_exception {
  callback_invocation_error() noexcept
    : runtime_error(invocation_error_)
    , nested_exception()
  {}
};

// execute the callable immediately in the current context
// and send any errors to the handler.
template<CallbackSignal F, class... Args, Invocable<Args...> C>
constexpr void try_invoke_callback(F&& f, C&& c, Args&&... args) noexcept
try {
  invoke((C&&) c, (Args&&) args...);
} catch (...) {
  ((F&&) f).error(make_exception_ptr(callback_invocation_error{}));
}

// execute the callable immediately in the current context
// and send any errors to the handler (if any).
template<class... Args, Invocable<Args...> C>
constexpr void invoke_callback(C&& c, Args&&... args)
noexcept(is_nothrow_invocable_v<C, Args...> || CallbackSignal<C>) {
  if constexpr (CallbackSignal<C>)
    try_invoke_callback((C&&) c, (C&&) c, (Args&&) args...);
  else
    invoke((C&&) c, (Args&&) args...);
}

template<Invocable Fn, Invocable<invoke_result_t<Fn>> C>
requires CallbackSignal<C> 
auto compose_callback(C&& c, Fn&& fn) {
  struct __local {
    remove_cvref_t<C> c_;
    remove_cvref_t<Fn> fn_;
    void operator()() && {
      invoke((C&&) c_, invoke((Fn&&) fn_));
    }
    void done() && noexcept {
      ((C&&) c_).done();
    }
    void error(exception_ptr ep) && noexcept {
      ((C&&) c_).error(move(ep));
    }
  };
  return __local{(C&&) c, (Fn&&) fn};
}

inline constexpr struct __execute_fn {
  // Pass the callable and the handler to the executor to create an
  // execution agent for execution.
  template<Invocable C, Executor<C> E>
  constexpr void operator()(E&& e, C&& c) const
      noexcept(noexcept(((E&&) e).execute((C&&) c))) {
    ((E&&) e).execute((C&&) c);
  }
} const execute{};

struct inline_executor {
  template<Invocable C>
  void execute(C c) {
    invoke_callback(move(c));
  }
};

template<Executor Ex, Invocable SF,
  class State = decay_t<invoke_result_t<SF&>>,
  Invocable<size_t, State&> Op, Invocable<State> C>
void bulk_execute(Ex ex, Op op, size_t n, SF sf, C c) {
  if constexpr (requires {ex.bulk_execute(op,n,sf,c);})
    return ex.bulk_execute(op, n, sf, c);
  else
    ex.execute(compose_callback(move(c), [=] {
        auto shared = sf();
        for(size_t i = 0; i < n; ++i)
            op(i, shared);
        return shared;
      }));
}

}} // namespace std::tbd

//
// ^^^^^ Executor machinery ends here     ^^^^^
// vvvvv Sender/Scheduler machinery below vvvvv
//

// A helper concept used to constrain part of the implementation
// of submit.
template<class T, class C>
concept _SenderTo =
  std::tbd::CallbackSignal<C> &&
  (requires (T&& t, C&& c) { {((T&&) t).submit(((C&&) c))} noexcept; } ||
   requires (T&& t, C& c) { ((T&&) t).submit(c); });

// The implementation of the submit customization point.
inline constexpr struct __submit_fn {
  inline static constexpr auto __with_subex = [](auto e, auto c) {
    return std::tbd::compose_callback(std::move(c), [=]{return e;});
  };
  using __with_subex_fn = decltype(__with_subex);
  template<class E, class C>
  using __with_subex_t = std::invoke_result_t<__with_subex_fn, E, C>;

  // Handle s.submit(c):
  template<class C, _SenderTo<C> S>
  void operator()(S s, C c) const noexcept {
    if constexpr (noexcept(std::move(s).submit(std::move(c))))
      std::move(s).submit(std::move(c));
    else
      std::tbd::try_invoke_callback(std::move(c), [&]{std::move(s).submit(c);});
  }
  // For use when passed a Executor and a nullary
  // Callback.
  template<std::tbd::Callback C, std::tbd::Executor<C> E>
  void operator()(E e, C c) const noexcept requires (!_SenderTo<E, C>) {
    std::move(e).execute(std::move(c));
  }
  // For use when passed a Executor and a Callback that
  // is expecting to be passed a sub-executor.
  template<class E, std::tbd::Callback<E> C>
    requires std::tbd::Executor<E, __with_subex_t<E, C>>
  void operator()(E e, C c) const noexcept requires (!_SenderTo<E, C>) {
    auto c2 = __with_subex(e, std::move(c));
    ((E&&) e).execute(std::move(c2));
  }
} const submit {};

// A multi-type concept for constraining compatible Senders
// and Callbacks. 
template<class S, class C>
concept SenderTo =
  requires (S&& s, C&& c) {
    submit((S&&) s, (C&&) c);
  };

// Below are some nasty implementation details for the
// Scheduler concept, which is inherently self-recursive.
// Basically a Scheduler is a type with a .schedule()
// member that returns a Sender of a Scheduler. We hackishly
// test that constraint with an archetypical Callback that
// requires its argument to be a either a Scheduler, or the
// type we started with. (That terminates the recursion for
// sane definitions of Schedulers.)
template<class, class>
inline constexpr bool __is_scheduler_or_v = false;
template<class S, class T>
concept _IsSchedulerOr = __is_scheduler_or_v<S, T>;
template<class T>
struct __scheduler_callback {
  template<_IsSchedulerOr<T> S>
  void operator()(S) const {}
  void done() const noexcept {}
  void error(std::exception_ptr) const noexcept {}
};

// With this definition of schedule(), a type with .schedule()
// is a Scheduler, but so too is a Executor.
struct __schedule_fn {
  template<class S>
    requires requires(S s) { s.schedule(); }
  auto operator()(S s) const noexcept {
    return s.schedule();
  }
  template<std::tbd::Executor E>
  auto operator()(E e) const noexcept {
    return e;
  }
};
inline constexpr __schedule_fn const schedule {};

template<class T>
concept Scheduler = requires (T t) {
  {schedule(t)} -> SenderTo<__scheduler_callback<T>>;
};

template<class S, class T>
  requires std::Same<S, T> || Scheduler<S>
inline constexpr bool __is_scheduler_or_v<S, T> = true;

//
// ^^^^^ Sender/Scheduler machinery ends here ^^^^^
// vvvvv Test code below            vvvvv
//

template<class Fn>
struct terminate_on_error : Fn {
  constexpr terminate_on_error(Fn fn)
    : Fn(std::move(fn)) {}
  void done() const noexcept
  {}
  template <class E>
  [[noreturn]] void error(E&&) const noexcept {
    std::terminate();
  }
};

// Show how an Executor works with execute with either
// a raw Invocable or a Callback
void foo0() {
  std::tbd::inline_executor ie;

  std::tbd::execute(ie, []() noexcept {});
  std::tbd::execute(ie, terminate_on_error{[]() noexcept {}});
}

struct inline_scheduler {
  struct inline_executor {
    template<std::Invocable C>
    void execute(C c) && noexcept {
      std::tbd::invoke_callback(std::move(c));
    }
  };

  std::tbd::inline_executor schedule() {
    return {};
  }
};
static_assert(Scheduler<std::tbd::inline_executor>);
static_assert(Scheduler<inline_scheduler>);

// Show how execute works with Schedulers and raw
// Invocables and also Callbacks.
void foo1() {
  inline_scheduler is;

  std::tbd::execute(schedule(is), 
                    []() noexcept {});
  std::tbd::execute(schedule(is),
                    terminate_on_error{[]() noexcept {}});
}

template<class... An>
struct inline_callback {
  void done() const noexcept {}
  void operator()(An...) const noexcept {}
  [[noreturn]] void error(std::exception_ptr) const noexcept {
    std::terminate();
  }
};

template<class... An>
struct inline_sender {
  std::tuple<An...> an_;

  template<std::tbd::Callback<An&...> C>
  void submit(C&& c) {
    std::apply([&](An&... an) {
                 std::invoke((C&&) c, an...);
               },
               an_);
  }
};

// Show how submit works with a Sender and a Callback 
void foo2() {
  inline_sender<int, float> ie{std::make_tuple(42, 42.0)};

  submit(ie, inline_callback<int, float>{});
}

struct full_inline_scheduler {
  struct inline_sender {
    template<std::Invocable C>
    void execute(C c) && noexcept(
        noexcept(std::tbd::invoke_callback((C&&) c))) {
      std::tbd::invoke_callback((C&&) c);
    }
    template<std::tbd::Callback<full_inline_scheduler> C>
    void submit(C c) && noexcept(
        noexcept(((C&&) c)(full_inline_scheduler{}))) {
      std::invoke((C&&) c, full_inline_scheduler{});
    }
  };

  inline_sender schedule() {
    return {};
  }
};

// Show how a Scheduler can work with execute, and how an
// Executor can work with submit:
void foo3() {
  full_inline_scheduler is;

  std::tbd::execute(schedule(is), inline_callback{});
  submit(schedule(is), inline_callback<full_inline_scheduler>{});

  submit(std::tbd::inline_executor{},
         terminate_on_error{[](){}});
  submit(std::tbd::inline_executor{},
         terminate_on_error{[](std::tbd::inline_executor){}});
}

// An example of using bulk_execute:
void foo4() {
    std::tbd::inline_executor ie;

    using std::string;
    using namespace std::literals;
    std::tbd::bulk_execute(ie,                               // Executor
                           [](int, string&)noexcept{},       // Bulk operation
                           100u,                             // Iterations
                           []()noexcept{ return "state"s; }, // State factory
                           [](string&&)noexcept{});          // Continuation
}

template<typename Func, typename Inner>
struct transform_sender {
    Inner inner_;
    Func func_;
    template<typename Receiver>
    struct transform_receiver {
        Func func_;
        Receiver receiver_;
        template<typename... Values>
        void set_value(Values&&... values) {
            receiver_.set_value(std::invoke(func_, (Values&&)values...));
        }
        template<typename Error>
        void set_error(Error&& error) {
            receiver_.set_error((Error&&)error);
        }
        void set_done() {
            receiver_.set_done();
        }
    };
    template<typename Receiver>
    void submit(Receiver r) {
        // Here we delegate responsibility for storing the receiver,'r'
        // and a copy of'func_'to the implementation of inner_.submit() which
        // is required to store the transform_receiver we pass to it.
        inner_.submit(transform_receiver<Receiver>{ func_, std::move(r)});
    }
};
