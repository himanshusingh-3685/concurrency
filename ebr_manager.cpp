#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <cassert>
#include <chrono>

constexpr size_t NUM_EPOCHS = 3;

struct TestNode {
    int id;
    std::atomic<bool> is_deleted{false};
    TestNode(int val) : id(val) {}
};

class SimpleEBR {
private:
    static constexpr uint64_t INACTIVE_EPOCH = std::numeric_limits<uint64_t>::max();

    struct alignas(64) ThreadRegistration {
        std::atomic<uint64_t> local_epoch{INACTIVE_EPOCH};
    };

    std::atomic<uint64_t> global_epoch_{0};
    std::vector<std::unique_ptr<ThreadRegistration>> registry_;
    
    // 3 bins to store retired nodes per epoch
    std::vector<TestNode*> retired_bins_[NUM_EPOCHS];
    std::mutex retire_mtx_;

    static inline thread_local size_t my_thread_id_{std::numeric_limits<size_t>::max()};

public:
    explicit SimpleEBR(size_t max_threads) {
        for (size_t i = 0; i < max_threads; ++i) {
            registry_.push_back(std::make_unique<ThreadRegistration>());
        }
    }

    ~SimpleEBR() {
        // Final cleanup of remaining nodes across all bins
        for (size_t e = 0; e < NUM_EPOCHS; ++e) {
            for (auto* node : retired_bins_[e]) {
                delete node;
            }
            retired_bins_[e].clear();
        }
    }

    void register_thread(size_t id) {
        my_thread_id_ = id;
    }

    // ========================================================
    // 1. ENTER READ: Announce current epoch
    // Requirements:
    // - Load global_epoch_ using std::memory_order_relaxed.
    // - Store this value into registry_[my_thread_id_]->local_epoch
    //   using std::memory_order_release (or seq_cst).
    // ========================================================
    void enter_read() {
        // TODO: Implement enter_read
        uint64_t badge_ = global_epoch_.load(std::memory_order_relaxed);
        registry_[my_thread_id_]->local_epoch.store(badge_,std::memory_order_release);
    }

    // ========================================================
    // 2. EXIT READ: Announce inactive state
    // Requirements:
    // - Set local_epoch to INACTIVE_EPOCH using std::memory_order_release.
    // ========================================================
    void exit_read() {
        // TODO: Implement exit_read
        registry_[my_thread_id_]->local_epoch.store(INACTIVE_EPOCH,std::memory_order_release);
    }

    // ========================================================
    // 3. RETIRE: Buffer node into current epoch bin
    // Requirements:
    // - Protect with retire_mtx_
    // - Push node into retired_bins_[global_epoch_ % NUM_EPOCHS]
    // ========================================================
    void retire(TestNode* node) {
        // TODO: Implement retire
        std::lock_guard<std::mutex> lock(retire_mtx_);
        retired_bins_[global_epoch_ % NUM_EPOCHS].push_back(node);
    }

    // ========================================================
    // 4. ADVANCE & RECLAIM:
    // Requirements:
    // - Check if ALL active threads have caught up to global_epoch_.
    //   If any active thread has local_epoch < current_epoch, we cannot advance.
    // - If all caught up:
    //   1. Advance global_epoch_ by 1.
    //   2. Empty and delete all nodes in retired_bins_[(new_epoch - 2) % NUM_EPOCHS]
    //      (or (current_epoch + 2) % 3, which is the oldest bin).
    // ========================================================
    void try_advance_and_reclaim() {
        std::lock_guard<std::mutex> lock(retire_mtx_);
        uint64_t current_epoch = global_epoch_.load(std::memory_order_relaxed);

        // Check whether any thread is still lagging behind in an older epoch
        for (const auto& reg : registry_) {
            uint64_t e = reg->local_epoch.load(std::memory_order_acquire);
            if (e != INACTIVE_EPOCH && e < current_epoch) {
                return; // A thread is still in an older epoch: cannot advance!
            }
        }

        // All threads are at least in current_epoch or inactive!
        uint64_t next_epoch = current_epoch + 1;
        global_epoch_.store(next_epoch, std::memory_order_relaxed);

        // Safely reclaim nodes from two epochs ago
        size_t reclaim_bin_idx = (next_epoch + 1) % NUM_EPOCHS; 
        for (auto* node : retired_bins_[reclaim_bin_idx]) {
            node->is_deleted.store(true, std::memory_order_relaxed);
            delete node;
        }
        retired_bins_[reclaim_bin_idx].clear();
    }
};

int main() {
    std::cout << "--- Starting Epoch-Based Reclamation Verification ---\n";

    constexpr size_t THREADS = 4;
    SimpleEBR ebr(THREADS);

    // Register main thread
    ebr.register_thread(0);

    // Step 1: Thread 0 enters reading at Epoch 0
    ebr.enter_read();

    // Step 2: Retire a node during Epoch 0
    TestNode* n1 = new TestNode(101);
    ebr.retire(n1);

    // Cannot reclaim n1 yet because Thread 0 is active in Epoch 0
    ebr.try_advance_and_reclaim();
    assert(n1->is_deleted.load() == false);

    // Step 3: Thread 0 exits read
    ebr.exit_read();

    // Advance to Epoch 1
    ebr.try_advance_and_reclaim();

    // Advance to Epoch 2 (n1 was from Epoch 0, now 2 epochs behind: safe to delete!)
    ebr.try_advance_and_reclaim();
    assert(n1->is_deleted.load() == true);

    std::cout << "SUCCESS: Node from Epoch 0 safely deferred and reclaimed without premature free!\n";
    return 0;
}