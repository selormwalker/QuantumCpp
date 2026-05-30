#include <iostream>
#include <chrono>
#include "OrderBook.hpp"

int main() {
    std::cout << "QuantumCpp: High-Performance Order Book Initialized" << std::endl;
    
    OrderBook ob;
    
    // Simulating high-frequency order flow
    auto start = std::chrono::high_resolution_clock::now();
    
    ob.addOrder({1, 100.50, 10, Side::BUY});
    ob.addOrder({2, 100.55, 5, Side::SELL});
    ob.addOrder({3, 100.45, 20, Side::BUY});
    ob.addOrder({4, 100.60, 15, Side::SELL});
    
    std::cout << "\nInitial State:" << std::endl;
    ob.display();

    // Triggering a match
    std::cout << "Adding matching order..." << std::endl;
    ob.addOrder({5, 100.50, 12, Side::SELL}); // Should match with ID 1 and partially with ID 3

    std::cout << "\nFinal State:" << std::endl;
    ob.display();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    
    std::cout << "Simulation finished in " << elapsed.count() << " ms" << std::endl;
    
    return 0;
}
