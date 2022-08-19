#include <string>
#include <utility>
#include <type_traits>
#include <functional>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include "all_prop.cpp"

class simple_execution_context
{
    struct task_base
    {
        virtual void execute() noexcept = 0;
        task_base *next;
    };
    class schedule_sender
    {
        simple_execution_context &ctx;

    public:
        explicit schedule_sender(simple_execution_context &ctx) noexcept : ctx(ctx) {}
        template <typename Receiver>
        class operation_state final : private task_base
        {
            simple_execution_context &ctx;
            std::remove_cvref_t<Receiver> receiver;
            void execute() noexcept override
            {
                try
                {
                    std::execution::set_value(std::move(receiver));
                }
                catch (...)
                {
                    std::execution::set_error(std::move(receiver), std::current_exception());
                }
            }

        public:
            explicit operation_state(simple_execution_context &ctx, Receiver &&r) : ctx(ctx), receiver((Receiver &&) r) {}
            void start() noexcept & { ctx.enqueue(this); }
        };
        // Returns the operation-state object to the caller which is responsible for
        // ensuring it remains alive until the operation completes once start() is called.
     //   template <std::receiver_of Receiver>
     //   operation_state<Receiver> connect(Receiver &&r) { return operation_state<Receiver>{*this, (Receiver &&) r}; }
    };
    class scheduler
    {
        simple_execution_context &ctx;

    public:
        explicit scheduler(simple_execution_context &ctx) noexcept : ctx(ctx) {}
        schedule_sender schedule() const noexcept { return schedule_sender{ctx}; }
    };

public:
    scheduler get_scheduler() noexcept
    {
        return scheduler{*this};
    }
    // Processes all pending tasks until the queue is empty.
    void drain() noexcept
    {
        while (head != nullptr)
        {
            task_base *t = std::exchange(head, head->next);
            t->execute();
        }
    }
private:
    void enqueue(task_base *t) noexcept { t->next = std::exchange(head, t); }
    task_base *head = nullptr;
};