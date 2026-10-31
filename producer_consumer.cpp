#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <optional>
#include <cassert>
#include <atomic>

template <typename T>
class BoundedBlockingQueue {
private:
    std::queue<T> queue_;
    const size_t max_capacity_;
    std::mutex mtx_;

    // ========================================================
    // TODO: Declare your synchronization primitives!
    // Hint: You need condition variable(s) to track:
    //       1. When the queue is NOT full (for producers)
    //       2. When the queue is NOT empty (for consumers)
    //       And a flag to signal shutdown.
    // ========================================================
    
    std::condition_variable cv_not_full_;
    std::condition_variable cv_not_empty_;
    bool is_shutdown_{false};

public:
    explicit BoundedBlockingQueue(size_t capacity) 
        : max_capacity_(capacity) {}

    // PUSH: Blocks if full. Returns false if queue is shut down.
    bool push(T item) {
        // ========================================================
        // TODO: Implement push from scratch!
        // Requirements:
        // 1. Acquire unique_lock on mtx_.
        // 2. Wait until queue_.size() < max_capacity_ OR is_shutdown_ is true.
        // 3. If shutdown, return false.
        // 4. Push item into queue_.
        // 5. Notify waiting consumers (cv_not_empty_).
        // ========================================================
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_full_.wait(lock,[&](){return !(queue_.size() == max_capacity_ && !is_shutdown_);});
        if(is_shutdown_) return false;
        queue_.push(item);
        cv_not_empty_.notify_one();
        return true;
    }

    // POP: Blocks if empty. Returns std::nullopt if queue is shut down and empty.
    std::optional<T> pop() {
        // ========================================================
        // TODO: Implement pop from scratch!
        // Requirements:
        // 1. Acquire unique_lock on mtx_.
        // 2. Wait until !queue_.empty() OR is_shutdown_ is true.
        // 3. If shutdown AND queue_.empty(), return std::nullopt.
        // 4. Extract front item, pop from queue_.
        // 5. Notify waiting producers (cv_not_full_).
        // 6. Return the extracted item.
        // ========================================================
        std::unique_lock<std::mutex> lock(mtx_);
        cv_not_empty_.wait(lock,[&](){return !(queue_.empty() && !is_shutdown_);});
        if (is_shutdown_ && queue_.empty()) return std::nullopt;
        auto it = queue_.front();
        queue_.pop();
        cv_not_full_.notify_one();
        return it;
    }

    // SHUTDOWN: Gracefully wakes up all waiting producers and consumers
    void shutdown() {
        // ========================================================
        // TODO: Implement shutdown from scratch!
        // Requirements:
        // 1. Acquire lock and set is_shutdown_ = true.
        // 2. Wake up ALL sleeping threads (both producers and consumers).
        // ========================================================
        std::unique_lock<std::mutex> lock(mtx_);
        is_shutdown_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }
};

int main() {
    std::cout << "--- Testing Bounded Blocking Queue ---\n";

    constexpr size_t QUEUE_CAPACITY = 5;
    constexpr int TOTAL_ITEMS = 1000;
    BoundedBlockingQueue<int> queue(QUEUE_CAPACITY);

    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::atomic<long long> consumer_sum{0};

    {
        std::vector<std::jthread> threads;

        // Spawn 3 Producer Threads
        for (int p = 0; p < 3; ++p) {
            threads.emplace_back([&]() {
                while (true) {
                    int val = items_produced.fetch_add(1, std::memory_order_relaxed);
                    if (val >= TOTAL_ITEMS) break;

                    if (!queue.push(val)) {
                        break; // Queue shut down
                    }
                }
            });
        }

        // Spawn 3 Consumer Threads
        for (int c = 0; c < 3; ++c) {
            threads.emplace_back([&]() {
                while (true) {
                    auto item = queue.pop();
                    if (!item.has_value()) {
                        break; // Queue empty and shut down
                    }
                    consumer_sum.fetch_add(*item, std::memory_order_relaxed);
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Give workers time to process all items
        while (items_consumed.load() < TOTAL_ITEMS) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Shut down the queue to unblock any idle waiting threads
        queue.shutdown();
    } // All threads join here

    long long expected_sum = (static_cast<long long>(TOTAL_ITEMS - 1) * TOTAL_ITEMS) / 2;

    std::cout << "Items Produced: " << items_consumed.load() << " / " << TOTAL_ITEMS << '\n';
    std::cout << "Actual Sum:     " << consumer_sum.load() << '\n';
    std::cout << "Expected Sum:   " << expected_sum << '\n';

    assert(items_consumed.load() == TOTAL_ITEMS);
    assert(consumer_sum.load() == expected_sum);

    std::cout << "SUCCESS: All assertions passed! No deadlocks, lost signals, or data races.\n";
    return 0;
}