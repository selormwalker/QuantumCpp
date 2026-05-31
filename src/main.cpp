#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include "OrderBook.hpp"

// Example Command structure for the lock-free queue
struct Command {
    enum Type { ADD, CANCEL } type;
    uint64_t id;
    Quantum::Price price;
    Quantum::Quantity quantity;
    Quantum::Side side;
};

int main() {
    std::cout << "QuantumCpp v2.0: Ultra-Low Latency Trading Core" << std::endl;

    Quantum::OrderBook lob;
    Quantum::CommandQueue<Command> queue;

    lob.setTradeCallback([](const Quantum::TradeEvent& t) {
        std::cout << "[TRADE] " << t.quantity << " @ " << (float)t.price/Quantum::SCALE 
                  << " (B:" << t.buyOrderId << " S:" << t.sellOrderId << ")" << std::endl;
    });

    // --- High-Performance Ingestion Thread ---
    std::thread engine_thread([&]() {
        Command cmd;
        while (true) {
            if (queue.dequeue(cmd)) {
                if (cmd.type == Command::ADD) {
                    lob.add_order(cmd.id, cmd.price, cmd.quantity, cmd.side);
                } else if (cmd.type == Command::CANCEL) {
                    lob.cancel_order(cmd.id);
                }
            } else {
                // Yield or busy-wait for sub-microsecond response
                std::this_thread::yield();
            }
            
            // Exit condition for demo
            if (cmd.id == 999) break;
        }
    });

    // --- Order Producer ---
    auto start = std::chrono::high_resolution_clock::now();

    // Ingest Institutional Liquidity
    queue.enqueue({Command::ADD, 1, 50000 * Quantum::SCALE, 100, Quantum::Side::SELL});
    queue.enqueue({Command::ADD, 2, 50010 * Quantum::SCALE, 100, Quantum::Side::SELL});
    
    // Aggressive Buy Order
    queue.enqueue({Command::ADD, 3, 50005 * Quantum::SCALE, 50, Quantum::Side::BUY});

    // Stop Engine
    queue.enqueue({Command::ADD, 999, 0, 0, Quantum::Side::BUY});

    engine_thread.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;

    std::cout << "\nQuantum Ingestion & Matching complete in " << elapsed.count() << " us." << std::endl;
    lob.display();

    return 0;
}
