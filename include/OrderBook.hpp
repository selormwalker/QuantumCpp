#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdint.h>

enum class Side { BUY, SELL };

struct Order {
    uint64_t id;
    double price;
    uint32_t quantity;
    Side side;
};

class OrderBook {
private:
    // Prices mapped to lists of orders (FIFO)
    std::map<double, std::list<Order>, std::greater<double>> bids; // Highest price first
    std::map<double, std::list<Order>> asks;                     // Lowest price first
    
    // Fast lookup for order cancellation (O(1))
    std::unordered_map<uint64_t, std::list<Order>::iterator> orderMap;

public:
    void addOrder(const Order& order) {
        if (order.side == Side::BUY) {
            bids[order.price].push_back(order);
        } else {
            asks[order.price].push_back(order);
        }
        std::cout << "Order added: " << (order.side == Side::BUY ? "BUY" : "SELL") 
                  << " " << order.quantity << " @ " << order.price << std::endl;
        
        matchOrders();
    }

    void matchOrders() {
        while (!bids.empty() && !asks.empty()) {
            auto& bestBidLevel = bids.begin()->second;
            auto& bestAskLevel = asks.begin()->second;
            
            double bidPrice = bids.begin()->first;
            double askPrice = asks.begin()->first;

            if (bidPrice >= askPrice) {
                Order& buyOrder = bestBidLevel.front();
                Order& sellOrder = bestAskLevel.front();

                uint32_t matchQty = std::min(buyOrder.quantity, sellOrder.quantity);
                
                std::cout << "MATCH: " << matchQty << " units @ " << askPrice << std::endl;

                buyOrder.quantity -= matchQty;
                sellOrder.quantity -= matchQty;

                if (buyOrder.quantity == 0) {
                    bestBidLevel.pop_front();
                    if (bestBidLevel.empty()) bids.erase(bids.begin());
                }
                if (sellOrder.quantity == 0) {
                    bestAskLevel.pop_front();
                    if (bestAskLevel.empty()) asks.erase(asks.begin());
                }
            } else {
                break; // No match possible
            }
        }
    }

    void display() {
        std::cout << "\n--- Order Book ---" << std::endl;
        std::cout << "ASKS:" << std::endl;
        for (auto const& [price, orders] : asks) {
            uint32_t totalQty = 0;
            for (auto const& o : orders) totalQty += o.quantity;
            std::cout << "  " << price << " : " << totalQty << std::endl;
        }
        std::cout << "BIDS:" << std::endl;
        for (auto const& [price, orders] : bids) {
            uint32_t totalQty = 0;
            for (auto const& o : orders) totalQty += o.quantity;
            std::cout << "  " << price << " : " << totalQty << std::endl;
        }
        std::cout << "------------------\n" << std::endl;
    }
};

#endif
