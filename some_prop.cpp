#include <string>
#include <utility>
#include <type_traits>
#include <functional>
#include <exception>
#include <stdexcept>
#include <coroutine>


class simple_execution_context {
    class awaiter {
        friend simple_execution_context;
        simple_execution_context& ctx;
        awaiter* next = nullptr;
        std::coroutine_handle<> continuation;
        
        public:
        explicit awaiter(simple_execution_context& ctx) noexcept : ctx(ctx) {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::continuation_handle<> h) noexcept {
                continuation = h;
                ctx.enqueue(this);
            }
            void await_resume() noexcept {}};
            class schedule_awaitable {
                simple_execution_context& ctx;
                public:
                explicit schedule_awaitable(simple_execution_context& ctx) noexcept : ctx(ctx) {}
                // Return an instance of the operation-state from'operator co_await()'
                // This is will be placed as a local variable within the awaiting coroutine's
                // coroutine-frame and means that we don't need a separate heap-allocation.
                awaiter operator co_await() const noexcept { return awaiter{ctx};}
            };
            class scheduler {
                simple_execution_context& ctx;
                public:explicit scheduler(simple_execution_context& ctx) noexcept : ctx(ctx) {}
                schedule_awaitable schedule() const noexcept { return schedule_awaitable{ctx}; }
            };
            public:scheduler get_scheduler() noexcept { return scheduler{*this}; }
            // Processes all pending awaiters until the queue is empty.
            void drain() noexcept {
                while (head != nullptr) {
                    awaiter* a = std::exchange(head, head->next);
                    a->execute();
                }
            }
            private:void enqueue(awaiter* a) noexcept {
                a->next = std::exchange(head, a);
            }
            awaiter* head = nullptr;
};