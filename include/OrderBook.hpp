#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdint.h>
#include <memory>
#include <functional>

// --- Types & Constants ---
typedef int64_t Price;
typedef uint32_t Quantity;
const Price SCALE = 10000; // 4 decimal places precision

enum class Side { BUY, SELL };

struct Order {
    uint64_t id;
    Price price;
    Quantity quantity;
    Side side;
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

// --- Memory Pool (Simplified for HFT) ---
template<typename T, size_t BlockSize = 1024>
class ObjectPool {
private:
    std::vector<std::unique_ptr<T[]>> blocks;
    std::vector<T*> freeList;

public:
    T* allocate() {
        if (freeList.empty()) {
            blocks.push_back(std::make_unique<T[]>(BlockSize));
            for (size_t i = 0; i < BlockSize; ++i) {
                freeList.push_back(&blocks.back()[i]);
            }
        }
        T* obj = freeList.back();
        freeList.pop_back();
        return obj;
    }

    void deallocate(T* obj) {
        freeList.push_back(obj);
    }
};

class OrderBook {
private:
    // Core Data Structures
    std::map<Price, std::list<Order*>, std::greater<Price>> bids; 
    std::map<Price, std::list<Order*>> asks;                     
    
    struct OrderInfo {
        Price price;
        Side side;
        std::list<Order*>::iterator it;
    };
    std::unordered_map<uint64_t, OrderInfo> orderMap;
    
    // Performance Components
    ObjectPool<Order> orderPool;
    BBO currentBBO;
    
    // Callbacks for Event-Driven Architecture
    std::function<void(const TradeEvent&)> onTrade;

public:
    void setTradeCallback(std::function<void(const TradeEvent&)> cb) { onTrade = cb; }

    void addOrder(uint64_t id, Price price, Quantity quantity, Side side) {
        // Use Memory Pool to avoid heap allocation cost
        Order* order = orderPool.allocate();
        order->id = id;
        order->price = price;
        order->quantity = quantity;
        order->side = side;

        if (side == Side::BUY) {
            auto& list = bids[price];
            list.push_back(order);
            orderMap[id] = {price, side, --list.end()};
        } else {
            auto& list = asks[price];
            list.push_back(order);
            orderMap[id] = {price, side, --list.end()};
        }
        
        matchOrders();
        updateBBO();
    }

    void cancelOrder(uint64_t id) {
        auto it = orderMap.find(id);
        if (it == orderMap.end()) return;

        OrderInfo info = it->second;
        Order* orderPtr = *info.it;

        if (info.side == Side::BUY) {
            bids[info.price].erase(info.it);
            if (bids[info.price].empty()) bids.erase(info.price);
        } else {
            asks[info.price].erase(info.it);
            if (asks[info.price].empty()) asks.erase(info.price);
        }
        
        orderPool.deallocate(orderPtr); // Return to pool
        orderMap.erase(it);
        updateBBO();
    }

    void matchOrders() {
        while (!bids.empty() && !asks.empty()) {
            auto bestBidIt = bids.begin();
            auto bestAskIt = asks.begin();
            
            if (bestBidIt->first >= bestAskIt->first) {
                auto& bestBidLevel = bestBidIt->second;
                auto& bestAskLevel = bestAskIt->second;

                Order* buyOrder = bestBidLevel.front();
                Order* sellOrder = bestAskLevel.front();

                Quantity matchQty = std::min(buyOrder->quantity, sellOrder->quantity);
                
                // Trigger Trade Event
                if (onTrade) {
                    onTrade({buyOrder->id, sellOrder->id, bestAskIt->first, matchQty});
                }

                buyOrder->quantity -= matchQty;
                sellOrder->quantity -= matchQty;

                if (buyOrder->quantity == 0) {
                    orderMap.erase(buyOrder->id);
                    orderPool.deallocate(buyOrder);
                    bestBidLevel.pop_front();
                    if (bestBidLevel.empty()) bids.erase(bestBidIt);
                }
                if (sellOrder->quantity == 0) {
                    orderMap.erase(sellOrder->id);
                    orderPool.deallocate(sellOrder);
                    bestAskLevel.pop_front();
                    if (bestAskLevel.empty()) asks.erase(bestAskIt);
                }
            } else {
                break;
            }
        }
    }

    void updateBBO() {
        if (!bids.empty()) {
            currentBBO.bestBid = bids.begin()->first;
            currentBBO.bidQty = 0;
            for (auto o : bids.begin()->second) currentBBO.bidQty += o->quantity;
        } else {
            currentBBO.bestBid = 0;
            currentBBO.bidQty = 0;
        }

        if (!asks.empty()) {
            currentBBO.bestAsk = asks.begin()->first;
            currentBBO.askQty = 0;
            for (auto o : asks.begin()->second) currentBBO.askQty += o->quantity;
        } else {
            currentBBO.bestAsk = 0;
            currentBBO.askQty = 0;
        }
    }

    BBO getBBO() const { return currentBBO; }

    void display() {
        std::cout << "\n--- Quantum Order Book (BBO: " 
                  << (float)currentBBO.bestBid/SCALE << " @ " << (float)currentBBO.bestAsk/SCALE << ") ---" << std::endl;
        std::cout << "  ASK Depth: " << asks.size() << " levels | BID Depth: " << bids.size() << " levels" << std::endl;
        std::cout << "-----------------------------------------------------\n" << std::endl;
    }
};

#endif
