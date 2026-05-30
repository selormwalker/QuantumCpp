#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdint.h>

// Using fixed-point arithmetic for price to avoid double precision issues
// 100.50 becomes 10050 (scaled by 100)
typedef int64_t Price;
typedef uint32_t Quantity;

enum class Side { BUY, SELL };

struct Order {
    uint64_t id;
    Price price;
    Quantity quantity;
    Side side;
};

class OrderBook {
private:
    // Prices mapped to lists of orders (FIFO)
    std::map<Price, std::list<Order>, std::greater<Price>> bids; 
    std::map<Price, std::list<Order>> asks;                     
    
    // Fast lookup for order cancellation (O(1))
    // Stores a pair of (Price, iterator to the order in the list)
    struct OrderInfo {
        Price price;
        Side side;
        std::list<Order>::iterator it;
    };
    std::unordered_map<uint64_t, OrderInfo> orderMap;

public:
    void addOrder(uint64_t id, Price price, Quantity quantity, Side side) {
        Order order = {id, price, quantity, side};
        
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
    }

    void cancelOrder(uint64_t id) {
        auto it = orderMap.find(id);
        if (it == orderMap.end()) return;

        OrderInfo info = it->second;
        if (info.side == Side::BUY) {
            bids[info.price].erase(info.it);
            if (bids[info.price].empty()) bids.erase(info.price);
        } else {
            asks[info.price].erase(info.it);
            if (asks[info.price].empty()) asks.erase(info.price);
        }
        
        orderMap.erase(it);
        std::cout << "Order cancelled: ID " << id << std::endl;
    }

    void matchOrders() {
        while (!bids.empty() && !asks.empty()) {
            auto bestBidIt = bids.begin();
            auto bestAskIt = asks.begin();
            
            if (bestBidIt->first >= bestAskIt->first) {
                auto& bestBidLevel = bestBidIt->second;
                auto& bestAskLevel = bestAskIt->second;

                Order& buyOrder = bestBidLevel.front();
                Order& sellOrder = bestAskLevel.front();

                Quantity matchQty = std::min(buyOrder.quantity, sellOrder.quantity);
                
                std::cout << "MATCH: " << matchQty << " units @ " << bestAskIt->first << std::endl;

                buyOrder.quantity -= matchQty;
                sellOrder.quantity -= matchQty;

                if (buyOrder.quantity == 0) {
                    orderMap.erase(buyOrder.id);
                    bestBidLevel.pop_front();
                    if (bestBidLevel.empty()) bids.erase(bestBidIt);
                }
                if (sellOrder.quantity == 0) {
                    orderMap.erase(sellOrder.id);
                    bestAskLevel.pop_front();
                    if (bestAskLevel.empty()) asks.erase(bestAskIt);
                }
            } else {
                break;
            }
        }
    }

    void display() {
        std::cout << "\n--- Order Book (Fixed-Point) ---" << std::endl;
        std::cout << "ASKS:" << std::endl;
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            uint32_t totalQty = 0;
            for (auto const& o : it->second) totalQty += o.quantity;
            std::cout << "  " << it->first << " : " << totalQty << std::endl;
        }
        std::cout << "BIDS:" << std::endl;
        for (auto const& [price, orders] : bids) {
            uint32_t totalQty = 0;
            for (auto const& o : orders) totalQty += o.quantity;
            std::cout << "  " << price << " : " << totalQty << std::endl;
        }
        std::cout << "--------------------------------\n" << std::endl;
    }
};

#endif
