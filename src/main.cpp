#include <iostream>
#include <chrono>
#include "OrderBook.hpp"

// Performance-grade Trade Logger Callback
void onTradeExecuted(const TradeEvent& event) {
    std::cout << "[TRADE EXECUTION] Buy Order #" << event.buyOrderId 
              << " matched with Sell Order #" << event.sellOrderId 
              << " | " << event.quantity << " units @ " << (float)event.price/SCALE << std::endl;
}

int main() {
    std::cout << "QuantumCpp: HFT Performance Session Started" << std::endl;
    
    OrderBook ob;
    ob.setTradeCallback(onTradeExecuted);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulating institutional-grade order flow
    std::cout << "Seeding liquidity (Using Memory Pool)..." << std::endl;
    ob.addOrder(1, 1005000, 50, Side::BUY);   // $100.50
    ob.addOrder(2, 1005500, 10, Side::SELL);  // $100.55
    ob.addOrder(3, 1004500, 100, Side::BUY);  // $100.45
    ob.addOrder(4, 1006000, 30, Side::SELL);  // $100.60
    
    ob.display();

    std::cout << "Triggering massive trade event..." << std::endl;
    ob.addOrder(5, 1005000, 80, Side::SELL); // Should fully match ID 1 and partially ID 3

    BBO snapshot = ob.getBBO();
    std::cout << "\nPost-Trade BBO Snapshot:" << std::endl;
    std::cout << "Best Bid: " << (float)snapshot.bestBid/SCALE << " (Qty: " << snapshot.bidQty << ")" << std::endl;
    std::cout << "Best Ask: " << (float)snapshot.bestAsk/SCALE << " (Qty: " << snapshot.askQty << ")" << std::endl;

    ob.display();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    std::cout << "HFT Simulation finished in " << elapsed.count() << " ms" << std::endl;
    
    return 0;
}
