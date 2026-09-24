#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>

namespace Minimum {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
        if (threads < 2) threads = 2;
        if (threads > 8) threads = 8;
        m_stop = false;
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                        if (m_stop && m_tasks.empty()) return;
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& w : m_workers) {
            if (w.joinable()) w.join();
        }
    }

    template <class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_tasks.emplace(std::forward<F>(f));
        }
        m_cv.notify_one();
    }

    void parallelFor(size_t begin, size_t end, size_t grain, std::function<void(size_t, size_t)> fn) {
        if (end <= begin) return;
        if (end - begin < grain || m_workers.size() < 2) {
            fn(begin, end);
            return;
        }
        size_t n = m_workers.size();
        size_t total = end - begin;
        size_t chunk = (total + n - 1) / n;
        std::atomic<int> remaining{0};
        std::mutex doneMutex;
        std::condition_variable doneCv;

        for (size_t t = 0; t < n; ++t) {
            size_t a = begin + t * chunk;
            size_t b = std::min(a + chunk, end);
            if (a >= b) break;
            remaining.fetch_add(1);
            enqueue([&, a, b] {
                fn(a, b);
                if (remaining.fetch_sub(1) == 1) {
                    std::lock_guard<std::mutex> lock(doneMutex);
                    doneCv.notify_one();
                }
            });
        }
        std::unique_lock<std::mutex> lock(doneMutex);
        doneCv.wait(lock, [&] { return remaining.load() == 0; });
    }

    size_t size() const { return m_workers.size(); }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop;
};

inline ThreadPool& pool() {
    static ThreadPool p;
    return p;
}

inline void warmPool() {
    (void)pool().size();
}

} // namespace Minimum
