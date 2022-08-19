#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

class ThreadPool final
{
public:

    explicit ThreadPool(std::size_t nthreads = std::thread::hardware_concurrency()):
        m_enabled(true),
        m_pool(nthreads)
    {
        run();
    }

    ~ThreadPool()
    {
        stop();
    }

    ThreadPool(ThreadPool const&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<class TaskT>
    auto enqueue(TaskT task) -> std::future<decltype(task())>
    {
        using ReturnT = decltype(task());
        auto promise = std::make_shared<std::promise<ReturnT>>();
        auto result = promise->get_future();

        auto t = [p = std::move(promise), t = std::move(task)] () mutable { execute(*p, t); };

        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_tasks.push(std::move(t));
        }

        m_cv.notify_one();

        return result;
    }

private:

    std::mutex m_mu;
    std::condition_variable m_cv;

    bool m_enabled;
    std::vector<std::thread> m_pool;
    std::queue<std::function<void()>> m_tasks;

    template<class ResultT, class TaskT>
    static void execute(std::promise<ResultT>& p, TaskT& task)
    {
        p.set_value(task()); // syntax doesn't work with void ResultT :(
    }

    template<class TaskT>
    static void execute(std::promise<void>& p, TaskT& task)
    {
        task();
        p.set_value();
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_enabled = false;
        }

        m_cv.notify_all();

        for (auto& t : m_pool)
            t.join();
    }

    void run()
    {
        auto f = [this] ()
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock{ m_mu };
                m_cv.wait(lock, [&] () { return !m_enabled || !m_tasks.empty(); });

                if (!m_enabled)
                    break;

                assert(!m_tasks.empty());

                auto task = std::move(m_tasks.front());
                m_tasks.pop();

                lock.unlock();
                task();
            }
        };

        for (auto& t : m_pool)
            t = std::thread(f);
    }
};
// Create some work to test the Thread Pool
void spitId()
{
    std::cout << "thread #" << std::this_thread::get_id() << '\n';
}

void sayAndNoReturn()
{
    auto tid = std::this_thread::get_id();
    std::cout << "thread #" << tid << "says " << " and returns... ";
    std::cout << typeid(tid).name() << '\n';    // std::thread::id
}

char sayWhat(int arg)
{
    auto tid = std::this_thread::get_id();
    std::stringstream sid;
    sid << tid;
    int id = std::stoi(sid.str());
    std::cout << "\nthread #" << id << " says " << arg << " and returns... ";
    if (id > 7000)
        return 'X';
    return 'a';
}

struct Member
{
    int i_ = 4;
    void sayCheese(int i)
    {
        std::cout << "CHEESEE!" << '\n';
        std::cout << i + i_ << '\n';
    }
};

int vv() { puts("nothing"); return 0; }
int vs(const std::string& str) { puts(str.c_str()); return 0; }

int main()
{
    ThreadPool threadPool{ std::thread::hardware_concurrency() };

    threadPool.enqueue(spitId);
    threadPool.enqueue(&spitId);
    threadPool.enqueue(sayAndNoReturn);

    auto f1 = threadPool.enqueue([]() -> int
    {
        std::cout << "lambda 1\n";
        return 1;
    });

    // auto sayWhatRet = threadPool.enqueue([]()
    // {
    //     return sayWhat(10);
    // });
    // std::cout << sayWhatRet.get() << '\n';

    Member member{ 1 };
    threadPool.enqueue(std::bind(&Member::sayCheese, member, 100));

    std::cout << f1.get() << '\n';



    auto f2 = threadPool.enqueue([]()
    {
        std::cout << "lambda 2\n";
        return 2;
    });
    // auto f3 = threadPool.enqueue([]()
    // {
    //     return sayWhat(100);
    // });

    //threadPool.enqueue(std::function<void(int)>{Member{}.sayCheese(100)});

    std::cout << "f1 type = " << typeid(f2).name() << '\n';

    std::cout << f2.get() << '\n';
  //  std::cout << f3.get() << '\n';

    return EXIT_SUCCESS;
}