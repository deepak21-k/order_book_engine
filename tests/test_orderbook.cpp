//  Lightweight test suite – no external framework needed.
//  Each test prints PASS / FAIL and returns non-zero on any failure.

#include "OrderBook.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <cmath>

static int passed = 0;
static int failed = 0;

#define EXPECT(cond, msg)                                          \
    do {                                                           \
        if (cond) {                                                \
            std::cout << "  [PASS] " << msg << "\n"; ++passed;    \
        } else {                                                   \
            std::cout << "  [FAIL] " << msg << "\n"; ++failed;    \
        }                                                          \
    } while(0)

//  Helpers

static std::ostringstream devNull;

Order makeOrder(const std::string& sym, Side side, double price, int qty) {
    Order o;
    o.id = 0; o.symbol = sym; o.side = side;
    o.price = price; o.quantity = qty; o.timestamp = 0;
    return o;
}

//  Test: basic match

void test_basic_match() {
    std::cout << "\n[TEST] Basic price-priority match\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,  100.0, 10));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 100.0, 10));

    EXPECT(trades.size() == 1,         "Exactly one trade executed");
    EXPECT(trades[0].quantity == 10,   "Trade qty = 10");
    EXPECT(std::fabs(trades[0].price - 100.0) < 1e-9, "Trade price = 100.0");
    EXPECT(ob.bestBid("TEST") == 0.0,  "Book empty after full fill (bids)");
    EXPECT(ob.bestAsk("TEST") == 0.0,  "Book empty after full fill (asks)");
}


//  Test: partial fill

void test_partial_fill() {
    std::cout << "\n[TEST] Partial fill\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,  100.0, 15));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 100.0,  8));

    EXPECT(trades.size() == 1,         "One trade");
    EXPECT(trades[0].quantity == 8,    "Matched qty = 8 (sell side)");
    EXPECT(ob.bestBid("TEST") == 100.0,"Bid still resting after partial fill");
    EXPECT(ob.bestAsk("TEST") == 0.0,  "Ask fully consumed");
}

//  Test: no match when spread exists


void test_no_cross() {
    std::cout << "\n[TEST] No cross – bid below ask\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,   99.0, 10));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 101.0, 10));

    EXPECT(trades.empty(),             "No trade when bid < ask");
    EXPECT(ob.bestBid("TEST") == 99.0, "Bid intact");
    EXPECT(ob.bestAsk("TEST") == 101.0,"Ask intact");
}

//  Test: price priority – best bid matched first

void test_price_priority() {
    std::cout << "\n[TEST] Price priority\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,  98.0, 5));
    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 5));  // better bid
    ob.addOrder(makeOrder("TEST", Side::BUY,  99.0, 5));

    // Sell at 98 – should match the 100 bid first
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 98.0, 5));

    EXPECT(trades.size() == 1,          "One trade");
    EXPECT(trades[0].price == 100.0,    "Matched at best bid (100.0)");
    EXPECT(ob.bestBid("TEST") == 99.0,  "Next best bid is 99.0");
}

//  Test: cancel order


void test_cancel() {
    std::cout << "\n[TEST] Cancel order\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 10));
    // id=1 was assigned to the first order
    bool ok = ob.cancelOrder(1);
    EXPECT(ok, "Cancel returns true for valid id");

    // Now add a crossing sell – cancelled bid should NOT match
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 100.0, 10));
    EXPECT(trades.empty(), "Cancelled order not matched");

    bool bad = ob.cancelOrder(999);
    EXPECT(!bad, "Cancel returns false for unknown id");
}

//  Test: modify order


void test_modify() {
    std::cout << "\n[TEST] Modify order qty\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 5));   // id=1
    ob.addOrder(makeOrder("TEST", Side::BUY,  99.0, 5));   // id=2
    uint64_t newId = ob.modifyOrder(1, 20);
    EXPECT(newId != 0, "Modify returns new order ID");

    // Now add a crossing sell for 20 units
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 99.0, 20));
    // The modified order re-entered the book; total bids ≥ 20
    int totalMatched = 0;
    for (auto& t : trades) totalMatched += t.quantity;
    EXPECT(totalMatched == 20, "Full 20 units matched after modify");
}

//  Test: multiple symbols independent

void test_multi_symbol() {
    std::cout << "\n[TEST] Multi-symbol independence\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("AAPL", Side::BUY,  150.0, 10));
    ob.addOrder(makeOrder("GOOG", Side::BUY,  200.0, 10));

    // A sell on AAPL should not affect GOOG
    auto trades = ob.addOrder(makeOrder("AAPL", Side::SELL, 150.0, 10));
    EXPECT(trades.size() == 1,           "AAPL trade executes");
    EXPECT(ob.bestBid("GOOG") == 200.0,  "GOOG book unaffected");
}

//  Test: bestBid / bestAsk after cancel

void test_best_price_after_cancel() {
    std::cout << "\n[TEST] bestBid / bestAsk after cancel\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 10)); // ID 1
    ob.addOrder(makeOrder("TEST", Side::BUY,  99.0, 10)); // ID 2
    ob.addOrder(makeOrder("TEST", Side::SELL, 101.0, 10)); // ID 3
    ob.addOrder(makeOrder("TEST", Side::SELL, 102.0, 10)); // ID 4

    EXPECT(ob.bestBid("TEST") == 100.0, "Initial best bid is 100.0");
    EXPECT(ob.bestAsk("TEST") == 101.0, "Initial best ask is 101.0");

    ob.cancelOrder(1); // Cancel best bid
    EXPECT(ob.bestBid("TEST") == 99.0, "Best bid updates to 99.0 after cancel");

    ob.cancelOrder(3); // Cancel best ask
    EXPECT(ob.bestAsk("TEST") == 102.0, "Best ask updates to 102.0 after cancel");
    
    ob.cancelOrder(2); // Cancel remaining bid
    EXPECT(ob.bestBid("TEST") == 0.0, "Best bid is 0.0 when all bids cancelled");
}

//  Test: printBook after cancel

void test_print_book_after_cancel() {
    std::cout << "\n[TEST] printBook depth after cancel\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 10)); // ID 1
    ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 15)); // ID 2
    
    std::streambuf* oldCoutStreamBuf = std::cout.rdbuf();
    std::ostringstream strCout;
    std::cout.rdbuf(strCout.rdbuf());
    
    ob.printBook("TEST");
    
    std::string output = strCout.str();
    bool hasQty25 = output.find("    25") != std::string::npos;
    
    strCout.str("");
    strCout.clear();
    
    ob.cancelOrder(1);
    ob.printBook("TEST");
    output = strCout.str();
    bool hasQty15 = output.find("    15") != std::string::npos;
    bool hasQty25AfterCancel = output.find("    25") != std::string::npos;
    
    std::cout.rdbuf(oldCoutStreamBuf);

    EXPECT(hasQty25, "Initial printBook shows combined qty of 25");
    EXPECT(hasQty15 && !hasQty25AfterCancel, "printBook shows qty 15 after cancelling 10");
}

//  Test: input validation

void test_input_validation() {
    std::cout << "\n[TEST] Input validation\n";
    devNull.str("");
    OrderBook ob(devNull);

    auto t1 = ob.addOrder(makeOrder("TEST", Side::BUY, 0.0, 10));
    auto t2 = ob.addOrder(makeOrder("TEST", Side::BUY, -10.0, 10));
    auto t3 = ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, 0));
    auto t4 = ob.addOrder(makeOrder("TEST", Side::BUY, 100.0, -5));
    auto t5 = ob.addOrder(makeOrder("TEST", Side::BUY, std::nan(""), 10));
    auto t6 = ob.addOrder(makeOrder("TEST", Side::BUY, INFINITY, 10));

    EXPECT(ob.bestBid("TEST") == 0.0, "Invalid orders are rejected and not added to the book");
}

//  main


int main() {
    std::cout << "═══════════════════════════════════════\n";
    std::cout << "  Order Book Engine — Test Suite\n";
    std::cout << "═══════════════════════════════════════\n";

    test_basic_match();
    test_partial_fill();
    test_no_cross();
    test_price_priority();
    test_cancel();
    test_modify();
    test_multi_symbol();
    test_best_price_after_cancel();
    test_print_book_after_cancel();
    test_input_validation();

    std::cout << "\n───────────────────────────────────────\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "───────────────────────────────────────\n";

    return failed == 0 ? 0 : 1;
}
