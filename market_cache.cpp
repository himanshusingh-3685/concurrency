#include <iostream>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <optional>
#include <cassert>

class MarketDataCache {
private:
    mutable std::shared_mutex rw_mtx_;
    std::unordered_map<std::string, double> cache_;

public:
    MarketDataCache() = default;

    // READ: Allows multiple concurrent readers
    std::optional<double> get_price(const std::string& symbol) const {
        // ========================================================
        // TODO: Implement get_price using std::shared_lock
        // ========================================================
        std::shared_lock<std::shared_mutex> lock(rw_mtx_);
        auto it = cache_.find(symbol);
        if(it != cache_.end()) return it->second;
        return std::nullopt;
    }

    // WRITE: Requires exclusive access
    void update_price(const std::string& symbol, double price) {
        // ========================================================
        // TODO: Implement update_price using std::unique_lock
        // ========================================================
        std::unique_lock<std::shared_mutex> lock(rw_mtx_);
        std::string key_ = symbol;
        cache_[symbol] = price;
    }

    // READ-THEN-WRITE: Safe transition
    double get_or_set_default(const std::string& symbol, double default_price) {
        // ========================================================
        // TODO: Implement the drop -> lock exclusive -> recheck pattern!
        // 1. Shared lock -> check if exists -> return if found.
        // 2. Release shared lock.
        // 3. Unique lock.
        // 4. Re-check if inserted by another thread.
        // 5. Insert default_price and return.
        // ========================================================
        {
            std::shared_lock<std::shared_mutex> lock(rw_mtx_);
            auto it = cache_.find(symbol);
            if(it != cache_.end()) return it->second;
        }

        {
            std::unique_lock<std::shared_mutex> lock(rw_mtx_);
            auto it = cache_.find(symbol);
            if(it != cache_.end()) return it->second;
            std::string key_ = symbol;
            cache_[key_] = default_price;
            return default_price;
        }
        return 0.0;
    }
};

int main() {
    std::cout << "--- Testing Thread-Safe Market Data Cache ---\n";

    MarketDataCache market;
    market.update_price("NVDA", 120.0);
    market.update_price("AAPL", 220.0);
    market.update_price("QCOM", 165.0);

    constexpr int NUM_READERS = 6;
    constexpr int NUM_WRITERS = 2;
    constexpr int READ_ITERATIONS = 50'000;
    constexpr int WRITE_ITERATIONS = 5'000;

    std::vector<std::jthread> threads;

    // Launch Readers
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([&market]() {
            for (int j = 0; j < READ_ITERATIONS; ++j) {
                auto p = market.get_price("NVDA");
                if (p.has_value()) {
                    assert(*p >= 100.0);
                }
            }
        });
    }

    // Launch Writers
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([&market, i]() {
            for (int j = 0; j < WRITE_ITERATIONS; ++j) {
                market.update_price("NVDA", 120.0 + (j % 10));
                market.get_or_set_default("TSLA", 250.0);
            }
        });
    }

    // Join all threads
    threads.clear();

    std::cout << "Final NVDA Price: " << *market.get_price("NVDA") << '\n';
    std::cout << "Final TSLA Price: " << *market.get_price("TSLA") << '\n';
    assert(market.get_price("TSLA").has_value());

    std::cout << "SUCCESS: All concurrent reader/writer operations completed without data races!\n";
    return 0;
}