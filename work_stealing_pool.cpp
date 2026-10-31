#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <random>
#include <cassert>
#include <chrono>

using Task = std::function<void()>;

// Thread-safe double-ended queue for each worker
class WorkDeque {
private:
    mutable std::mutex mtx_;
    std::deque<Task> tasks_;

public:
    // OWNER: Pushes to bottom (back)
    void push(Task task) {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks_.push_back(std::move(task));
    }

    // OWNER: Pops from bottom (back - LIFO for cache locality)
    bool pop(Task& out_task) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tasks_.empty()) return false;
        out_task = std::move(tasks_.back());
        tasks_.pop_back();
        return true;
    }

    // THIEF: Steals from top (front - FIFO for coarse granularity)
    bool steal(Task& out_task) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tasks_.empty()) return false;
        out_task = std::move(tasks_.front());
        tasks_.pop_front();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return tasks_.empty();
    }
};

class WorkStealingPool {
private:
    const size_t num_threads_;
    std::vector<std::unique_ptr<WorkDeque>> deques_;
    std::vector<std::jthread> workers_;
    
    std::atomic<bool> stop_{false};
    std::atomic<size_t> submit_idx_{0};

    // Notification for parked workers
    std::mutex cv_mtx_;
    std::condition_variable cv_;

    // Thread-local ID to identify which deque belongs to the current worker
    static inline thread_local int worker_id_{-1};

public:
    explicit WorkStealingPool(size_t num_threads) 
        : num_threads_(num_threads) {
        
        for (size_t i = 0; i < num_threads_; ++i) {
            deques_.push_back(std::make_unique<WorkDeque>());
        }

        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back([this, i]() {
                worker_id_ = static_cast<int>(i);
                worker_routine(i);
            });
        }
    }

    ~WorkStealingPool() {
        stop_.store(true, std::memory_order_release);
        cv_.notify_all();
        // workers_ jthreads automatically join on destruction
    }

    // Submits a task to the pool:
    // If called by an existing worker, pushes to their own local deque.
    // If called by an outside thread (like main), round-robins across deques.
    void submit(Task task) {
        // ========================================================
        // TODO: Implement submit()
        // 1. If worker_id_ != -1, push to deques_[worker_id_].
        // 2. Otherwise, use submit_idx_.fetch_add(1) % num_threads_
        //    to distribute to a worker's deque.
        // 3. Notify one sleeping worker via cv_.notify_one().
        // ========================================================
        if(worker_id_ != -1) deques_[worker_id_]->push(std::move(task));
        else deques_[submit_idx_.fetch_add(1)%num_threads_]->push(std::move(task));
        cv_.notify_one();
    }

private:
    void worker_routine(size_t my_id) {
        // Setup random victim selector for stealing
        std::mt19937 rng(static_cast<unsigned>(my_id + 100));
        std::uniform_int_distribution<size_t> dist(0, num_threads_ - 1);

        while (!stop_.load(std::memory_order_relaxed)) {
            Task task;

            // ====================================================
            // STEP 1: Try local execution (Pop from own deque)
            // ====================================================
            if (deques_[my_id]->pop(task)) {
                task();
                continue;
            }

            // ====================================================
            // STEP 2: Work-Stealing
            // Pick random victims and try to steal() from their front.
            // ====================================================
            bool stolen = false;
            for (size_t attempt = 0; attempt < num_threads_; ++attempt) {
                size_t victim = dist(rng);
                if (victim != my_id && deques_[victim]->steal(task)) {
                    stolen = true;
                    break;
                }
            }

            if (stolen) {
                task();
                continue;
            }

            // ====================================================
            // STEP 3: Multi-Stage Backoff / Park
            // No tasks found locally or stolen.
            // Wait on cv_ until new tasks arrive or stop_ is true.
            // ====================================================
            std::unique_lock<std::mutex> lock(cv_mtx_);
            cv_.wait_for(lock, std::chrono::milliseconds(2), [this, my_id]() {
                return stop_.load(std::memory_order_relaxed) || !deques_[my_id]->empty();
            });
        }
    }
};

int main() {
    std::cout << "--- Starting Work-Stealing Thread Pool Benchmark ---\n";

    constexpr size_t THREADS = 4;
    constexpr int TOTAL_TASKS = 40'000;

    std::atomic<int> completed_tasks{0};

    {
        WorkStealingPool pool(THREADS);

        // Submit tasks: some create child tasks to stress dynamic load balancing
        for (int i = 0; i < TOTAL_TASKS; ++i) {
            pool.submit([&pool, &completed_tasks, i]() {
                completed_tasks.fetch_add(1, std::memory_order_relaxed);

                // Dynamically spawn child subtasks from within a running worker
                if (i % 10 == 0) {
                    pool.submit([&completed_tasks]() {
                        completed_tasks.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            });
        }

        // Wait until all tasks (initial + dynamically spawned) finish
        while (completed_tasks.load(std::memory_order_relaxed) < TOTAL_TASKS + (TOTAL_TASKS / 10)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } // Pool destructor runs here, stops threads and joins cleanly

    std::cout << "Total Tasks Processed: " << completed_tasks.load() << '\n';
    assert(completed_tasks.load() == TOTAL_TASKS + (TOTAL_TASKS / 10));
    std::cout << "SUCCESS: Work-stealing pool executed all tasks with zero deadlocks!\n";

    return 0;
}