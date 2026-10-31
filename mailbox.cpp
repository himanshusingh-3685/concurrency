#include <iostream>
#include <atomic>
#include <thread>
#include <string>
#include <cassert>
#include <chrono>

enum class MailboxState : int {
    Empty = 0,
    Writing = 1,
    Ready = 2
};

template <typename T>
class LockFreeMailbox {
private:
    T payload_{};
    std::atomic<MailboxState> state_{MailboxState::Empty};

    // Tracking counter using relaxed ordering
    std::atomic<uint64_t> messages_sent_{0};

public:
    // PRODUCER: Writes data and publishes to consumer
    // Requirements:
    // 1. Ensure state_ is Empty before writing (spin or check).
    // 2. Set state_ to Writing (can be relaxed or acquire).
    // 3. Write 'data' into payload_ (plain non-atomic write).
    // 4. Publish by setting state_ to Ready using std::memory_order_release!
    // 5. Increment messages_sent_ using std::memory_order_relaxed.
    void send(T data) {
        // ========================================================
        // TODO: Implement send() from scratch!
        // ========================================================
        while(state_.load(std::memory_order_acquire) != MailboxState::Empty)
        {
            std::this_thread::yield();
        }
        state_.store(MailboxState::Writing,std::memory_order_relaxed);
        payload_ = std::move(data);
        messages_sent_.fetch_add(1,std::memory_order_relaxed);
        state_.store(MailboxState::Ready,std::memory_order_release);
    }

    // CONSUMER: Polls and extracts data
    // Requirements:
    // 1. Read state_ using std::memory_order_acquire.
    // 2. If state_ != Ready, return false.
    // 3. Extract payload_ into out_data (plain non-atomic read).
    // 4. Reset state_ to Empty using std::memory_order_release.
    // 5. Return true.
    bool receive(T& out_data) {
        // ========================================================
        // TODO: Implement receive() from scratch!
        // ========================================================
        if(state_.load(std::memory_order_acquire) != MailboxState::Ready) return false;
        out_data = payload_;
        state_.store(MailboxState::Empty, std::memory_order_release);
        return true;
    }

    uint64_t get_total_sent() const {
        return messages_sent_.load(std::memory_order_relaxed);
    }
};

struct OrderPacket {
    int order_id;
    double price;
    char symbol[8];
};

int main() {
    std::cout << "--- Starting Lock-Free Mailbox (Acquire-Release Test) ---\n";

    LockFreeMailbox<OrderPacket> mailbox;
    constexpr int NUM_MESSAGES = 100'000;

    // Consumer Thread
    std::jthread consumer([&]() {
        int received_count = 0;
        OrderPacket packet;

        while (received_count < NUM_MESSAGES) {
            if (mailbox.receive(packet)) {
                // Invariant checks: payload MUST match what was sent without corruption
                assert(packet.order_id == received_count);
                assert(packet.price == received_count * 1.5);
                received_count++;
            } else {
                std::this_thread::yield();
            }
        }
        std::cout << "[Consumer] Finished verifying " << received_count << " packets cleanly.\n";
    });

    // Producer Thread
    std::jthread producer([&]() {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            OrderPacket packet{i, i * 1.5, "NVDA"};
            mailbox.send(packet);
        }
        std::cout << "[Producer] Finished sending all packets.\n";
    });

    // Threads auto-join here
    consumer.join();
    producer.join();

    assert(mailbox.get_total_sent() == NUM_MESSAGES);
    std::cout << "SUCCESS: All assertions passed with zero data races!\n";

    return 0;
}