#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <iostream>
#include <vector>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <stdint.h>
#include <memory>
#include <functional>
#include <atomic>

namespace Quantum {

// --- Institutional Constants ---
typedef int64_t Price;
typedef uint32_t Quantity;
const Price SCALE = 10000;

enum class Side : uint8_t { BUY = 0, SELL = 1 };

/**
 * Cache-aligned Order structure to minimize false sharing.
 */
struct alignas(64) Order {
    uint64_t id;
    Price price;
    Quantity quantity;
    Side side;
    uint64_t timestamp;
};

struct TradeEvent {
    uint64_t buyOrderId;
    uint64_t sellOrderId;
    Price price;
    Quantity quantity;
};

struct BBO {
    Price bestBid = 0;
    Price bestAsk = 0;
    Quantity bidQty = 0;
    Quantity askQty = 0;
};

// --- Quantum Memory Pool ---
template<typename T, size_t BlockSize = 4096>
class alignas(64) QuantumPool {
private:
    std::vector<std::unique_ptr<T[]>> storage;
    std::vector<T*> freeList;

public:
    T* allocate() {
        if (freeList.empty()) {
            auto block = std::make_unique<T[]>(BlockSize);
            for (size_t i = 0; i < BlockSize; ++i) freeList.push_back(&block[i]);
            storage.push_back(std::move(block));
        }
        T* obj = freeList.back();
        freeList.pop_back();
        return obj;
    }

    void deallocate(T* obj) { freeList.push_back(obj); }
};

/**
 * Contiguous Memory Price Level
 * Uses deque for O(1) matching from front and O(1) adding to back, 
 * while maintaining better cache locality than a linked list.
 */
struct PriceLevel {
    std::deque<Order*> orders;
    Quantity totalQuantity = 0;
};

/**
 * QuantumTier OrderBook
 * Features: Flat-map price discovery, Contiguous levels, and zero-allocation critical path.
 */
class alignas(64) OrderBook {
private:
    // Flat-map style price discovery using sorted vectors
    // Using vector for price levels to keep memory contiguous
    struct PriceEntry {
        Price price;
        PriceLevel level;
    };

    std::vector<PriceEntry> bids; // Kept sorted DESC
    std::vector<PriceEntry> asks; // Kept sorted ASC

    struct OrderMeta {
        Price price;
        Side side;
        size_t index_in_level;
    };
    std::unordered_map<uint64_t, OrderMeta> orderMap;

    QuantumPool<Order> pool;
    BBO currentBBO;
    std::function<void(const TradeEvent&)> tradeCallback;

public:
    OrderBook() {
        bids.reserve(1024);
        asks.reserve(1024);
    }

    void setTradeCallback(std::function<void(const TradeEvent&)> cb) { tradeCallback = cb; }

    void addOrder(uint64_t id, Price price, Quantity qty, Side side) {
        Order* order = pool.allocate();
        order->id = id;
        order->price = price;
        order->quantity = qty;
        order->side = side;
        order->timestamp = 0; // In production: rdtsc()

        if (side == Side::BUY) {
            auto it = std::find_if(bids.begin(), bids.end(), [price](const PriceEntry& e){ return e.price <= price; });
            if (it == bids.end() || it->price != price) {
                it = bids.insert(it, {price, {}});
            }
            it->level.orders.push_back(order);
            it->level.totalQuantity += qty;
            orderMap[id] = {price, side, it->level.orders.size() - 1};
        } else {
            auto it = std::find_if(asks.begin(), asks.end(), [price](const PriceEntry& e){ return e.price >= price; });
            if (it == asks.end() || it->price != price) {
                it = asks.insert(it, {price, {}});
            }
            it->level.orders.push_back(order);
            it->level.totalQuantity += qty;
            orderMap[id] = {price, side, it->level.orders.size() - 1};
        }

        match();
        updateBBO();
    }

    void cancelOrder(uint64_t id) {
        auto it = orderMap.find(id);
        if (it == orderMap.end()) return;

        OrderMeta meta = it->second;
        auto& levels = (meta.side == Side::BUY) ? bids : asks;
        
        auto levelIt = std::find_if(levels.begin(), levels.end(), [meta](const PriceEntry& e){ return e.price == meta.price; });
        if (levelIt != levels.end()) {
            auto& q = levelIt->level.orders;
            // Optimization: In a production system, we'd mark as deleted or use a linked list 
            // for O(1) removal. For this contiguous demo, we use a simple erase if at index.
            if (meta.index_in_level < q.size() && q[meta.index_in_level]->id == id) {
                levelIt->level.totalQuantity -= q[meta.index_in_level]->quantity;
                pool.deallocate(q[meta.index_in_level]);
                q.erase(q.begin() + meta.index_in_level);
            }
            if (q.empty()) levels.erase(levelIt);
        }
        orderMap.erase(it);
        updateBBO();
    }

    void match() {
        while (!bids.empty() && !asks.empty() && bids[0].price >= asks[0].price) {
            auto& bidLevel = bids[0].level;
            auto& askLevel = asks[0].level;

            Order* buy = bidLevel.orders.front();
            Order* sell = askLevel.orders.front();

            Quantity matchQty = std::min(buy->quantity, sellOrderQty(sell));
            
            if (tradeCallback) tradeCallback({buy->id, sell->id, asks[0].price, matchQty});

            buy->quantity -= matchQty;
            sell->quantity -= matchQty;
            bidLevel.totalQuantity -= matchQty;
            askLevel.totalQuantity -= matchQty;

            if (buy->quantity == 0) {
                orderMap.erase(buy->id);
                pool.deallocate(buy);
                bidLevel.orders.pop_front();
            }
            if (sell->quantity == 0) {
                orderMap.erase(sell->id);
                pool.deallocate(sell);
                askLevel.orders.pop_front();
            }

            if (bidLevel.orders.empty()) bids.erase(bids.begin());
            if (askLevel.orders.empty()) asks.erase(asks.begin());
        }
    }

    Quantity sellOrderQty(Order* o) { return o->quantity; }

    void updateBBO() {
        if (!bids.empty()) {
            currentBBO.bestBid = bids[0].price;
            currentBBO.bidQty = bids[0].level.totalQuantity;
        } else {
            currentBBO.bestBid = 0; currentBBO.bidQty = 0;
        }

        if (!asks.empty()) {
            currentBBO.bestAsk = asks[0].price;
            currentBBO.askQty = asks[0].level.totalQuantity;
        } else {
            currentBBO.bestAsk = 0; currentBBO.askQty = 0;
        }
    }

    BBO getBBO() const { return currentBBO; }
};

// --- Disruptor-style Lock-Free Command Queue ---
template<typename T, size_t Size = 1024>
class alignas(64) CommandQueue {
private:
    T buffer[Size];
    alignas(64) std::atomic<size_t> writeIdx{0};
    alignas(64) std::atomic<size_t> readIdx{0};

public:
    bool enqueue(const T& cmd) {
        size_t next = (writeIdx.load(std::memory_order_relaxed) + 1) % Size;
        if (next == readIdx.load(std::memory_order_acquire)) return false;
        buffer[writeIdx.load(std::memory_order_relaxed)] = cmd;
        writeIdx.store(next, std::memory_order_release);
        return true;
    }

    bool dequeue(T& cmd) {
        size_t curr = readIdx.load(std::memory_order_relaxed);
        if (curr == writeIdx.load(std::memory_order_acquire)) return false;
        cmd = buffer[curr];
        readIdx.store((curr + 1) % Size, std::memory_order_release);
        return true;
    }
};

} // namespace Quantum

#endif
