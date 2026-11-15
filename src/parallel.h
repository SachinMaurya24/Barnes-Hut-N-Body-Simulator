#ifndef BARNES_HUT_PARALLEL_H
#define BARNES_HUT_PARALLEL_H

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

inline std::size_t default_thread_count() {
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    return hardware_threads == 0 ? 1 : static_cast<std::size_t>(hardware_threads);
}

class ThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Function>
    void parallel_for(
        std::size_t item_count,
        std::size_t requested_threads,
        Function function) {
        constexpr std::size_t chunk_size = 16;
        if (item_count == 0) {
            return;
        }

        const std::size_t thread_count = requested_threads == 0
            ? default_thread_count() : requested_threads;
        const std::size_t chunk_count = (item_count + chunk_size - 1) / chunk_size;
        const std::size_t worker_count = std::min(thread_count, chunk_count);

        if (worker_count <= 1) {
            for (std::size_t index = 0; index < item_count; ++index) {
                function(index);
            }
            return;
        }

        const std::size_t helper_count = worker_count - 1;
        ensure_threads(helper_count);

        std::atomic<std::size_t> next_index{0};
        const auto run_chunks = [&]() {
            while (true) {
                const std::size_t chunk_start = next_index.fetch_add(chunk_size, std::memory_order_relaxed);
                if (chunk_start >= item_count) {
                    break;
                }
                const std::size_t chunk_end = std::min(item_count, chunk_start + chunk_size);
                for (std::size_t index = chunk_start; index < chunk_end; ++index) {
                    function(index);
                }
            }
        };

        std::latch done_latch(static_cast<std::ptrdiff_t>(helper_count));
        const std::function<void()> task_func = run_chunks;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            current_task_ = &task_func;
            current_latch_ = &done_latch;
            assigned_workers_ = helper_count;
            ++task_id_;
            cv_workers_.notify_all();
        }

        run_chunks();
        done_latch.wait();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            current_task_ = nullptr;
            current_latch_ = nullptr;
            assigned_workers_ = 0;
        }
    }

private:
    ThreadPool() = default;

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
            ++task_id_;
            cv_workers_.notify_all();
        }
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ensure_threads(std::size_t count) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (workers_.size() < count) {
            const std::size_t worker_id = workers_.size();
            workers_.emplace_back([this, worker_id]() {
                std::uint64_t last_task_id = 0;
                while (true) {
                    const std::function<void()>* task = nullptr;
                    std::latch* latch = nullptr;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_workers_.wait(lock, [&]() {
                            return stop_ || (task_id_ != last_task_id && worker_id < assigned_workers_);
                        });

                        if (stop_) {
                            return;
                        }
                        last_task_id = task_id_;
                        task = current_task_;
                        latch = current_latch_;
                    }

                    if (task && *task) {
                        (*task)();
                    }
                    if (latch) {
                        latch->count_down();
                    }
                }
            });
        }
    }

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable cv_workers_;
    const std::function<void()>* current_task_{nullptr};
    std::latch* current_latch_{nullptr};
    std::size_t assigned_workers_{0};
    std::uint64_t task_id_{0};
    bool stop_{false};
};

template <typename Function>
inline void parallel_for_indices(
    std::size_t item_count,
    std::size_t requested_thread_count,
    Function function) {
    ThreadPool::instance().parallel_for(item_count, requested_thread_count, function);
}

#endif

