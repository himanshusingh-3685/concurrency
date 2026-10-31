#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <array>
#include <chrono>
#include <atomic>
#include <cassert>

constexpr int NUM_PHILOSOPHERS = 5;
constexpr int MEALS_PER_PHILOSOPHER = 500;

std::array<std::mutex, NUM_PHILOSOPHERS> forks;

// Counter to track total successful meals eaten
std::atomic<int> total_meals_eaten{0};

// Array to track meals eaten per philosopher
std::array<int, NUM_PHILOSOPHERS> meals_per_person{};

void philosopher_routine(int id) {
    int left_fork = id;
    int right_fork = (id + 1) % NUM_PHILOSOPHERS;

    int first = std::min(left_fork, right_fork);
    int second = std::max(left_fork, right_fork);

    for (int meal = 0; meal < MEALS_PER_PHILOSOPHER; ++meal) {
        
        // ========================================================
        // TODO: Write your synchronization logic from scratch!
        // 
        // Requirements:
        // 1. Acquire both 'left_fork' and 'right_fork' without deadlocking.
        // 2. Remember: DO NOT use std::scoped_lock(fork1, fork2).
        //    Implement manual lock ordering (Resource Hierarchy)
        //    or an explicit back-off strategy.
        // 3. Increment meals_per_person[id] while holding the forks.
        // 4. Release both forks cleanly.
        // ========================================================


        std::unique_lock<std::mutex> lock2(forks[second]);
        std::unique_lock<std::mutex> lock1(forks[first]);



        // Simulate a tiny eating duration (keep short to run fast)
        total_meals_eaten.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    std::cout << "--- Starting Dining Philosophers (Deadlock Test) ---\n";

    {
        std::vector<std::jthread> philosophers;
        philosophers.reserve(NUM_PHILOSOPHERS);

        // Spawn 5 philosophers concurrently
        for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
            philosophers.emplace_back(philosopher_routine, i);
        }
    } // All threads join here automatically

    std::cout << "All philosophers have finished dining!\n";
    std::cout << "Total meals expected: " << (NUM_PHILOSOPHERS * MEALS_PER_PHILOSOPHER) << '\n';
    std::cout << "Total meals eaten:    " << total_meals_eaten.load() << '\n';

    assert(total_meals_eaten.load() == NUM_PHILOSOPHERS * MEALS_PER_PHILOSOPHER);
    std::cout << "SUCCESS: All assertions passed, no deadlocks detected!\n";

    return 0;
}