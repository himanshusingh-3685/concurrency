#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <cassert>
#include <numeric>

/*
Thread-safety under high contention (multiple worker threads hitting the same accounts).

Deadlock-free multi-account transfers (using std::scoped_lock to avoid deadlocks when accounts transfer back and forth).

Self-transfer protection (preventing undefined behavior if someone transfers to themselves).

Fine-grained locking (simulating an expensive statement/audit log generation without holding the account lock during the slow part).

Invariant verification (asserting that total money across all accounts remains 100% constant after thousands of concurrent operations).
*/

class Account {
private:
    int id_;
    int balance_;
    mutable std::mutex mtx_;

public:
    Account(int id, int initial_balance) 
        : id_(id), balance_(initial_balance) {}

    int get_id() const { return id_; }

    // Read balance safely using scoped_lock
    int get_balance() const {
        std::scoped_lock lock(mtx_);
        return balance_;
    }

    // 1. Task: Complete safe deposit
    void deposit(int amount) {
        std::scoped_lock lock(mtx_);
        balance_ += amount;
    }

    // 2. Task: Complete withdraw
    // Return true if successful, false if insufficient funds
    bool withdraw(int amount) {
        std::scoped_lock lock(mtx_);
        if (balance_ >= amount) {
            balance_ -= amount;
            return true;
        }
        return false;
    }

    // 3. Task: Deadlock-Free Transfer
    // MUST:
    // - Check for self-transfer (this == &target)
    // - Use std::scoped_lock to lock both mtx_ and target.mtx_ simultaneously
    // - Deduct from this and add to target if funds are available
    bool transfer_to(Account& target, int amount) {
        // TODO: Fill this in!
        if(this == &target) return true;
        std::scoped_lock lock(this->mtx_, target.mtx_);
        if(this->balance_ < amount) return false;
        this->balance_ -= amount;
        target.balance_ += amount;
        return true; 
    }

    // 4. Task: Fine-Grained Locking / Expensive Task
    // This simulates creating an encrypted account receipt.
    // MUST:
    // - Lock mtx_ to read balance and id_
    // - Unlock early (or exit scope) before doing the 50ms expensive computation!
    void generate_statement_summary() const {
        int current_balance = 0;
        int acc_id = 0;

        // Step A: Grab lock, copy data out, release lock immediately
        {
            std::scoped_lock lock(mtx_);
            current_balance = balance_;
            acc_id = id_;
        } // lock is freed here!

        // Step B: Simulate expensive CPU work (takes time, but NO lock held!)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "[Audit] Account " << acc_id 
                  << " snapshot balance: " << current_balance << '\n';
    }
};

int main() {
    std::cout << "--- Starting Concurrent Ledger Verification ---\n";

    constexpr int INITIAL_BALANCE = 10'000;
    Account acc1(1, INITIAL_BALANCE);
    Account acc2(2, INITIAL_BALANCE);
    Account acc3(3, INITIAL_BALANCE);

    const int total_system_money = INITIAL_BALANCE * 3;

    {
        // Vector of modern C++20 jthreads
        std::vector<std::jthread> threads;

        // Thread 1 & 2: Rapid circular transfers between Acc1 and Acc2 (Deadlock trap!)
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                acc1.transfer_to(acc2, 10);
            }
        });

        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                acc2.transfer_to(acc1, 10);
            }
        });

        // Thread 3 & 4: Multi-account crisscross transfers
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                acc2.transfer_to(acc3, 15);
            }
        });

        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                acc3.transfer_to(acc1, 15);
            }
        });

        // Thread 5: Random self-transfers to test edge-case safety
        threads.emplace_back([&]() {
            for (int i = 0; i < 100; ++i) {
                acc1.transfer_to(acc1, 50); // Should safely do nothing
            }
        });
    } // All threads join here automatically

    int final_total = acc1.get_balance() + acc2.get_balance() + acc3.get_balance();

    std::cout << "Account 1 Final: " << acc1.get_balance() << '\n';
    std::cout << "Account 2 Final: " << acc2.get_balance() << '\n';
    std::cout << "Account 3 Final: " << acc3.get_balance() << '\n';
    std::cout << "Expected Total:  " << total_system_money << '\n';
    std::cout << "Actual Total:    " << final_total << '\n';

    // Invariant check: No money created or lost!
    assert(final_total == total_system_money);
    std::cout << "SUCCESS: All assertions passed, no race conditions or deadlocks!\n";

    return 0;
}