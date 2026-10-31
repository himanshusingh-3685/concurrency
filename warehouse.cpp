#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <latch>
#include <barrier>
#include <semaphore>
#include <atomic>
#include <cassert>

constexpr int NUM_ROBOTS = 4;
constexpr int CHARGING_DOCKS = 2;

// 1. Latch for system startup (4 robots must report ready)
std::latch startup_latch(NUM_ROBOTS);

// 2. Semaphore: Exactly 2 charging docks available
std::counting_semaphore<CHARGING_DOCKS> dock_semaphore(CHARGING_DOCKS);

// Callback to announce shift completion
auto on_shift_done = []() noexcept {
    std::cout << "[Supervisor] Shift complete! All robots synced at checkpoint.\n";
};

// 3. Barrier: 4 robots sync at each shift boundary
std::barrier shift_barrier(NUM_ROBOTS, on_shift_done);

// Tracking counters for verification
std::atomic<int> concurrent_chargers{0};
std::atomic<int> max_observed_chargers{0};
std::atomic<int> total_shifts_completed{0};

void robot_worker(int robot_id) {
    // ========================================================
    // STEP 1: Startup Calibration
    // - Simulate calibration (e.g., 20ms sleep)
    // - Signal the startup latch that this robot is calibrated
    // ========================================================
    // TODO: Write your code for Step 1
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    startup_latch.count_down();

    

    // ========================================================
    // STEP 2: Shift 1 - Scan Inventory
    // - Simulate scanning work (10ms)
    // - Wait at the shift_barrier until ALL 4 robots finish Shift 1
    // ========================================================
    // TODO: Write your code for Step 2
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    shift_barrier.arrive_and_wait();
    total_shifts_completed++;

    

    // ========================================================
    // STEP 3: Shift 2 - Move Boxes
    // - Simulate moving boxes (10ms)
    // - Wait at the shift_barrier until ALL 4 robots finish Shift 2
    // ========================================================
    // TODO: Write your code for Step 3
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    shift_barrier.arrive_and_wait();
    total_shifts_completed++;
    

    // ========================================================
    // STEP 4: Battery Charging
    // - Acquire a charging dock from dock_semaphore
    // - Track charging concurrency (already written for you)
    // - Sleep 30ms to charge
    // - Release the charging dock permit back to dock_semaphore
    // ========================================================
    // TODO: Write the acquire and release around the charging block:
    
    dock_semaphore.acquire();
    // --> ACQUIRE DOCK HERE <--
    {
        int current = ++concurrent_chargers;
        int prev_max = max_observed_chargers.load();
        while (current > prev_max && !max_observed_chargers.compare_exchange_weak(prev_max, current)) {}
        
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        
        --concurrent_chargers;
    }
    // --> RELEASE DOCK HERE <--
    dock_semaphore.release();
}

int main() {
    std::cout << "--- Starting Autonomous Warehouse Simulation ---\n";

    std::vector<std::jthread> robots;
    robots.reserve(NUM_ROBOTS);

    for (int i = 0; i < NUM_ROBOTS; ++i) {
        robots.emplace_back(robot_worker, i);
    }

    // Main thread waits until ALL 4 robots have calibrated
    std::cout << "[Main] Waiting for all robots to complete calibration...\n";
    // TODO: Wait on startup_latch here
    startup_latch.wait();
    

    std::cout << "[Main] All robots calibrated! Floor operations are LIVE.\n";

    // Wait for all robots to complete all shifts and charging
    robots.clear(); // Destructs and joins all jthreads

    std::cout << "Simulation finished!\n";
    std::cout << "Max concurrent chargers observed: " << max_observed_chargers.load() << " / " << CHARGING_DOCKS << '\n';

    // Assert that we never exceeded the 2 available charging docks
    assert(max_observed_chargers.load() <= CHARGING_DOCKS);
    std::cout << "SUCCESS: All synchronization invariants verified cleanly!\n";

    return 0;
}