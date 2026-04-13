#include "OrderBook.h"

#include <iostream>
#include <fstream>
#include <iomanip>

int main(int argc, char* argv[]) {

    // Trade log goes to both stdout and order_output.txt
    std::ofstream tradeFile("order_output.txt");
    if (!tradeFile.is_open()) {
        std::cerr << "[ERROR] Could not open order_output.txt for writing.\n";
        return 1;
    }

    // Tee stream: log to file; we'll echo to cout separately
    OrderBook ob(tradeFile);

    const std::string TICKER = "AAPL";

    // ── 1. Load from files if provided, else use hardcoded demo 
    bool usedFiles = false;
    if (argc == 3) {
        try {
            ob.loadFromFile(argv[1], TICKER, Side::BUY,  10);
            ob.loadFromFile(argv[2], TICKER, Side::SELL, 10);
            usedFiles = true;
        } catch (const std::exception& e) {
            std::cerr << "[WARN] File load failed: " << e.what()
                      << " — falling back to demo orders.\n";
        }
    }

    if (!usedFiles) {
        std::cout << "\n[INFO] Running with built-in demo orders.\n";
        std::cout << "[INFO] Usage: ./order_book_engine buy_orders.txt sell_orders.txt\n\n";

        // Demo orders – same scenario as original, but now uses proper engine
        auto trades = ob.addOrder({0, TICKER, Side::BUY,  150.00, 10, 0});
        trades      = ob.addOrder({0, TICKER, Side::BUY,  148.50, 15, 0});
        trades      = ob.addOrder({0, TICKER, Side::BUY,  149.00,  8, 0});
        trades      = ob.addOrder({0, TICKER, Side::SELL, 151.00, 12, 0});
        trades      = ob.addOrder({0, TICKER, Side::SELL, 149.50,  5, 0});

        // This sell crosses the best bid (150.00 >= 149.50? yes) → triggers trade
        std::cout << "\n── Adding SELL @ 149.00 (crosses best bid @ 150.00) ──\n";
        trades = ob.addOrder({0, TICKER, Side::SELL, 149.00, 6, 0});
        for (auto& t : trades) {
            std::cout << "  ✔ TRADE: " << t.quantity << " @ " << t.price << "\n";
        }

        // Demonstrate cancel
        std::cout << "\n── Cancelling order id=2 (BUY @ 148.50) ──\n";
        bool cancelled = ob.cancelOrder(2);
        std::cout << "  Cancel result: " << (cancelled ? "OK" : "FAILED") << "\n";

        // Demonstrate modify
        std::cout << "\n── Modifying order id=3 (BUY @ 149.00) qty → 20 ──\n";
        bool modified = ob.modifyOrder(3, 20);
        std::cout << "  Modify result: " << (modified ? "OK" : "FAILED") << "\n";
    }

    // ── 2. Print final order book state 
    ob.printBook(TICKER);

    // ── 3. Print spread summary 
    double bid = ob.bestBid(TICKER);
    double ask = ob.bestAsk(TICKER);
    std::cout << "\nBest Bid : " << std::fixed << std::setprecision(4) << bid << "\n";
    std::cout << "Best Ask : " << std::fixed << std::setprecision(4) << ask << "\n";
    if (bid > 0 && ask > 0)
        std::cout << "Spread   : " << std::fixed << std::setprecision(4) << (ask - bid) << "\n";

    std::cout << "\n[INFO] Trade log written to order_output.txt\n";
    tradeFile.close();
    return 0;
}
