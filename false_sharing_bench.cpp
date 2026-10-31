#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <new>
#include <cstdint>

constexpr int NUM_THREADS = 4;
constexpr uint64_t ITERATIONS = 50'000'000;

// ========================================================
// TASK 1: Define BadWorker
// - Contains a single uint64_t counter initialized to 0.
// - DO NOT apply any alignment (let it default to 8 bytes).
// ========================================================
struct BadWorker {
    // TODO: Define members
    uint64_t counter = 0;
};

// ========================================================
// TASK 2: Define GoodWorker
// - Contains a single uint64_t counter initialized to 0.
// - Use alignas(std::hardware_destructive_interference_size) 
//   to force each instance onto its own 64-byte cache line.
// ========================================================
struct alignas(std::hardware_destructive_interference_size) GoodWorker {
    // TODO: Define members with alignment
    uint64_t counter = 0;
};

// Worker function: tightly increments its assigned worker's counter
template <typename T>
void run_worker(T* array, int thread_id) {
    for (uint64_t i = 0; i < ITERATIONS; ++i) {
        array[thread_id].counter++;
    }
}

template <typename T>
long long measure_benchmark(const std::string& label) {
    // Allocate array of 4 workers
    std::vector<T> workers(NUM_THREADS);

    auto start_time = std::chrono::high_resolution_clock::now();

    {
        std::vector<std::jthread> threads;
        threads.reserve(NUM_THREADS);

        // Spawn 4 threads, each touching their own index
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(run_worker<T>, workers.data(), i);
        }
    } // All threads join here

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "[" << label << "] Time: " << duration_ms << " ms | sizeof struct: " << sizeof(T) << " bytes\n";
    return duration_ms;
}

int main() {
    std::cout << "--- False Sharing Performance Benchmark ---\n";
    std::cout << "Target hardware cache line size: " 
              << std::hardware_destructive_interference_size << " bytes\n\n";

    long long bad_time = measure_benchmark<BadWorker>("False Sharing (Bad)");
    long long good_time = measure_benchmark<GoodWorker>("Cache Aligned  (Good)");

    std::cout << "\nSpeedup: " << static_cast<double>(bad_time) / good_time << "x faster!\n";

    return 0;
}