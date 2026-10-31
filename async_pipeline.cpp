#include <iostream>
#include <future>
#include <thread>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <chrono>

// TASK 1: Worker function using std::promise
// Requirements:
// 1. If 'number' < 0, throw std::invalid_argument("Negative input not allowed")
//    Catch the exception and send it using prom.set_exception(std::current_exception()).
// 2. Otherwise, compute the square root (std::sqrt(number)) and send via prom.set_value(result).
void compute_square_root(double number, std::promise<double> prom) {
    // TODO: Write your implementation from scratch!
    try
    {
        if(number < 0) throw std::invalid_argument("Negative input not allowed");
    }
    catch(const std::exception& e)
    {
        prom.set_exception(std::current_exception());
        return;
    }
    
    double result = (std::sqrt(number));
    prom.set_value(result);
}

// TASK 2: Normal function to be packaged
int sum_range(int start, int end) {
    int sum = 0;
    for (int i = start; i <= end; ++i) {
        sum += i;
    }
    return sum;
}

int main() {
    std::cout << "--- Starting Module 6 Async Verification ---\n";

    // -------------------------------------------------------------
    // Test Case 1: Successful std::promise computation
    // -------------------------------------------------------------
    {
        std::promise<double> prom;
        std::future<double> fut = prom.get_future();

        std::jthread worker(compute_square_root, 64.0, std::move(prom));

        double result = fut.get();
        std::cout << "[Test 1] sqrt(64) = " << result << '\n';
        assert(result == 8.0);
    }

    // -------------------------------------------------------------
    // Test Case 2: Exception propagation through future
    // -------------------------------------------------------------
    {
        std::promise<double> prom;
        std::future<double> fut = prom.get_future();

        std::jthread worker(compute_square_root, -9.0, std::move(prom));

        bool exception_caught = false;
        try {
            double result = fut.get(); // Should rethrow invalid_argument!
        } catch (const std::invalid_argument& e) {
            std::cout << "[Test 2] Caught expected exception: " << e.what() << '\n';
            exception_caught = true;
        }
        assert(exception_caught == true);
    }

    // -------------------------------------------------------------
    // Test Case 3: Using std::packaged_task
    // Requirements:
    // 1. Wrap 'sum_range' into a std::packaged_task<int(int, int)>.
    // 2. Extract its future.
    // 3. Move the task into a std::jthread, passing arguments (1, 100).
    // 4. Retrieve and verify the result (sum of 1..100 = 5050).
    // -------------------------------------------------------------
    {
        // TODO: Complete Test Case 3!
        std::packaged_task<int(int,int)> task(sum_range);
        std::future<int> fut = task.get_future();

        std::jthread worker(std::move(task),1,100);

        int result = fut.get();
        std::cout<< "[Test 3] sum of 1..100 = "<<result<<std::endl;
        assert(result == 5050);
    }

    std::cout << "SUCCESS: All async and future assertions passed cleanly!\n";
    return 0;
}