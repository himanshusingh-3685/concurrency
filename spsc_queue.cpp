#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <new>
#include <cassert>
#include <chrono>

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

private:
    static constexpr size_t Mask = Capacity - 1;

    // Buffer storage
    T buffer_[Capacity];

    // PRODUCER STATE
    alignas(std::hardware_destructive_interference_size) 
    std::atomic<size_t> write_idx_{0};
    size_t cached_read_idx_{0}; // Thread-local to producer

    // CONSUMER STATE
    alignas(std::hardware_destructive_interference_size) 
    std::atomic<size_t> read_idx_{0};
    size_t cached_write_idx_{0}; // Thread-local to consumer

public:
    SPSCQueue() = default;

    // PUSH: Single-Producer only
    // Returns false if buffer is full
    bool push(T val) {
        // ========================================================
        // TODO: Implement push()
        // 1. Check if buffer is full using cached_read_idx_:
        //    if (current_write - cached_read_idx_ == Capacity)
        //       refresh cached_read_idx_ = read_idx_.load(std::memory_order_acquire)
        //       if still full, return false.
        // 2. Write val into buffer_[current_write & Mask].
        // 3. Store new write_idx_ with std::memory_order_release.
        // 4. Return true.
        // ========================================================
        if(write_idx_ - cached_read_idx_ == Capacity)
        {
            cached_read_idx_ = read_idx_.load(std::memory_order_acquire);
            if(write_idx_ - cached_read_idx_ == Capacity) return false;
        }
        buffer_[write_idx_ & (Capacity - 1)] = std::move(val);
        write_idx_.fetch_add(1,std::memory_order_release);
        return true;
    }

    // POP: Single-Consumer only
    // Returns false if buffer is empty
    bool pop(T& out_val) {
        // ========================================================
        // TODO: Implement pop()
        // 1. Check if buffer is empty using cached_write_idx_:
        //    if (current_read == cached_write_idx_)
        //       refresh cached_write_idx_ = write_idx_.load(std::memory_order_acquire)
        //       if still empty, return false.
        // 2. Read val from buffer_[current_read & Mask].
        // 3. Store new read_idx_ with std::memory_order_release.
        // 4. Return true.
        // ========================================================
        if(read_idx_ == cached_write_idx_)
        {
            cached_write_idx_ = write_idx_.load(std::memory_order_acquire);
            if(cached_write_idx_ == read_idx_) return false;
        }
        out_val = std::move(buffer_[read_idx_ & (Capacity - 1)]);
        read_idx_.fetch_add(1,std::memory_order_release);
        return true;
    }
};

int main() {
    std::cout << "--- Starting SPSC Ring Buffer Benchmark ---\n";

    constexpr size_t QUEUE_CAPACITY = 1024;
    constexpr uint64_t TOTAL_ITEMS = 5'000'000;

    SPSCQueue<uint64_t, QUEUE_CAPACITY> queue;

    uint64_t produced_checksum = 0;
    uint64_t consumed_checksum = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Consumer Thread
    std::jthread consumer([&]() {
        uint64_t items_received = 0;
        uint64_t val = 0;

        while (items_received < TOTAL_ITEMS) {
            if (queue.pop(val)) {
                assert(val == items_received); // Verifies strict FIFO ordering
                consumed_checksum += val;
                items_received++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer Thread
    std::jthread producer([&]() {
        for (uint64_t i = 0; i < TOTAL_ITEMS; ++i) {
            produced_checksum += i;
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    // Join threads
    producer.join();
    consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "Processed: " << TOTAL_ITEMS << " messages in " << elapsed_ms << " ms\n";
    std::cout << "Throughput: " << (TOTAL_ITEMS * 1000.0) / (elapsed_ms * 1'000'000.0) << " million msgs/sec\n";
    assert(produced_checksum == consumed_checksum);
    std::cout << "SUCCESS: All messages received in strict FIFO order with zero data races!\n";

    return 0;
}