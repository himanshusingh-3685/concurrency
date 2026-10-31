#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <cassert>

class Spinlock {
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        // ========================================================
        // TODO: Implement lock() from scratch!
        // Hint: Keep spinning while flag_.test_and_set() returns true.
        // Bonus: Call std::this_thread::yield() or _mm_pause() 
        //        inside the loop to be friendly to the CPU pipeline.
        // ========================================================
        while(flag_.test_and_set())
        {
            std::this_thread::yield();
        }
    }

    void unlock() {
        // ========================================================
        // TODO: Implement unlock() from scratch!
        // ========================================================
        flag_.clear();
    }
};

constexpr int NUM_THREADS = 4;
constexpr int ITERATIONS = 100'000;
int shared_counter = 0; // Protected solely by Spinlock
Spinlock spinlock;

void worker() {
    for (int i = 0; i < ITERATIONS; ++i) {
        spinlock.lock();
        shared_counter++; // Critical section
        spinlock.unlock();
    }
}

int main() {
    std::cout << "--- Testing Custom Spinlock ---\n";

    {
        std::vector<std::jthread> threads;
        threads.reserve(NUM_THREADS);
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(worker);
        }
    } // All threads join here

    std::cout << "Expected Counter: " << NUM_THREADS * ITERATIONS << '\n';
    std::cout << "Actual Counter:   " << shared_counter << '\n';

    assert(shared_counter == NUM_THREADS * ITERATIONS);
    std::cout << "SUCCESS: Spinlock mutual exclusion verified cleanly!\n";

    return 0;
}