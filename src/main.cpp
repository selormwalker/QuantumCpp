#include <iostream>
#include <chrono>
#include "OrderBook.hpp"

int main() {
    std::cout << "QuantumCpp: Advanced High-Performance Engine" << std::endl;
    
    OrderBook ob;
    
    // Using Fixed-Point: 100.50 -> 10050
    auto start = std::chrono::high_resolution_clock::now();
    
    std::cout << "Seeding orders..." << std::endl;
    ob.addOrder(1, 10050, 10, Side::BUY);
    ob.addOrder(2, 10055, 5, Side::SELL);
    ob.addOrder(3, 10045, 20, Side::BUY);
    ob.addOrder(4, 10060, 15, Side::SELL);
    
    ob.display();

    std::cout << "Testing O(1) Cancellation..." << std::endl;
    ob.cancelOrder(4); // Cancel the 10060 Sell
    ob.display();

    std::cout << "Testing Precision Matching..." << std::endl;
    ob.addOrder(5, 10050, 15, Side::SELL); // Match with ID 1 and part of ID 3

    ob.display();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    std::cout << "Engine simulation finished in " << elapsed.count() << " ms" << std::endl;
    
    return 0;
}
