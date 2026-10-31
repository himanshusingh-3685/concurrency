#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <optional>
#include <cassert>

template <typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };

    struct TaggedPointer {
        Node* ptr{nullptr};
        uint64_t tag{0};
    };

    // 128-bit atomic tagged head pointer
    alignas(16) std::atomic<TaggedPointer> head_{TaggedPointer{nullptr, 0}};

public:
    LockFreeStack() = default;

    // Destructor to drain and free all allocated nodes
    ~LockFreeStack() {
        while (pop().has_value()) {}
    }

    // PUSH: Allocates node, links next, swings head atomically
    // Requirements:
    // 1. Allocate new Node(val).
    // 2. Load current head_ into a TaggedPointer.
    // 3. Set new_node->next = current_head.ptr.
    // 4. Try CAS with desired = { new_node, current_head.tag + 1 }.
    // 5. Use std::memory_order_release on success, std::memory_order_relaxed on failure.
    void push(T val) {
        // ========================================================
        // TODO: Implement push() from scratch!
        // ========================================================
        Node* new_node_ = new Node(val);
        TaggedPointer current_head_ = head_;
        new_node_->next = current_head_.ptr;
        uint64_t tag_ = current_head_.tag;
        TaggedPointer desired = {new_node_,tag_+1};
        while(!head_.compare_exchange_weak(current_head_,desired,std::memory_order_release,std::memory_order_relaxed))
        {
            new_node_->next = current_head_.ptr;
            desired.tag = current_head_.tag+1;
            desired.ptr = new_node_;
        }
    }

    // POP: Extracts data, swings head to next, deletes old node
    // Requirements:
    // 1. Load current head_ into a TaggedPointer using std::memory_order_acquire.
    // 2. If current.ptr == nullptr, stack is empty -> return std::nullopt.
    // 3. Set desired = { current.ptr->next, current.tag + 1 }.
    // 4. Try CAS. If it succeeds, extract data, delete current.ptr, and return data.
    // 5. If CAS fails, loop and retry.
    std::optional<T> pop() {
        // ========================================================
        // TODO: Implement pop() from scratch!
        // ========================================================
        TaggedPointer current_head_ = head_;
        while (current_head_.ptr != NULL)
        {
            TaggedPointer desired = {current_head_.ptr->next, current_head_.tag + 1};
            if(head_.compare_exchange_strong(current_head_,desired,std::memory_order_acquire,std::memory_order_relaxed))
            {
                T data_ = current_head_.ptr->data;
                delete current_head_.ptr;
                return data_;
            }
        }
        return std::nullopt;
    }
};

int main() {
    std::cout << "--- Testing ABA-Safe Tagged-Pointer Lock-Free Stack ---\n";

    LockFreeStack<int> stack;
    constexpr int NUM_THREADS = 4;
    constexpr int ITEMS_PER_THREAD = 25'000;

    std::atomic<int> push_checksum{0};
    std::atomic<int> pop_checksum{0};

    // Concurrent Pushers
    {
        std::vector<std::jthread> pushers;
        for (int t = 0; t < NUM_THREADS; ++t) {
            pushers.emplace_back([&, t]() {
                for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                    int val = t * 100'000 + i;
                    stack.push(val);
                    push_checksum.fetch_add(val, std::memory_order_relaxed);
                }
            });
        }
    } // All pushers finish

    // Concurrent Poppers
    {
        std::vector<std::jthread> poppers;
        for (int t = 0; t < NUM_THREADS; ++t) {
            poppers.emplace_back([&]() {
                for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                    while (true) {
                        auto val = stack.pop();
                        if (val.has_value()) {
                            pop_checksum.fetch_add(*val, std::memory_order_relaxed);
                            break;
                        }
                        std::this_thread::yield();
                    }
                }
            });
        }
    } // All poppers finish

    std::cout << "Push Checksum: " << push_checksum.load() << '\n';
    std::cout << "Pop Checksum:  " << pop_checksum.load() << '\n';

    assert(push_checksum.load() == pop_checksum.load());
    std::cout << "SUCCESS: All elements pushed and popped with zero ABA corruption!\n";

    return 0;
}