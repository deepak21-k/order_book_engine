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

Order makeOrder(const std::string& sym, Side side, Price price, int qty) {
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

    ob.addOrder(makeOrder("TEST", Side::BUY,  10000, 10));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 10000, 10));

    EXPECT(trades.size() == 1,         "Exactly one trade executed");
    EXPECT(trades[0].quantity == 10,   "Trade qty = 10");
    EXPECT(trades[0].price == 10000,   "Trade price = 10000");
    EXPECT(ob.bestBid("TEST") == 0,    "Book empty after full fill (bids)");
    EXPECT(ob.bestAsk("TEST") == 0,    "Book empty after full fill (asks)");
}


//  Test: partial fill

void test_partial_fill() {
    std::cout << "\n[TEST] Partial fill\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,  10000, 15));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 10000,  8));

    EXPECT(trades.size() == 1,         "One trade");
    EXPECT(trades[0].quantity == 8,    "Matched qty = 8 (sell side)");
    EXPECT(ob.bestBid("TEST") == 10000,"Bid still resting after partial fill");
    EXPECT(ob.bestAsk("TEST") == 0,    "Ask fully consumed");
}

//  Test: no match when spread exists


void test_no_cross() {
    std::cout << "\n[TEST] No cross – bid below ask\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,   9900, 10));
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 10100, 10));

    EXPECT(trades.empty(),             "No trade when bid < ask");
    EXPECT(ob.bestBid("TEST") == 9900, "Bid intact");
    EXPECT(ob.bestAsk("TEST") == 10100,"Ask intact");
}

//  Test: price priority – best bid matched first

void test_price_priority() {
    std::cout << "\n[TEST] Price priority\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY,  9800, 5));
    ob.addOrder(makeOrder("TEST", Side::BUY, 10000, 5));  // better bid
    ob.addOrder(makeOrder("TEST", Side::BUY,  9900, 5));

    // Sell at 98 – should match the 100 bid first
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 9800, 5));

    EXPECT(trades.size() == 1,          "One trade");
    EXPECT(trades[0].price == 10000,    "Matched at best bid (10000)");
    EXPECT(ob.bestBid("TEST") == 9900,  "Next best bid is 9900");
}

//  Test: cancel order


void test_cancel() {
    std::cout << "\n[TEST] Cancel order\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 10000, 10));
    // id=1 was assigned to the first order
    bool ok = ob.cancelOrder(1);
    EXPECT(ok, "Cancel returns true for valid id");

    // Now add a crossing sell – cancelled bid should NOT match
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 10000, 10));
    EXPECT(trades.empty(), "Cancelled order not matched");

    bool bad = ob.cancelOrder(999);
    EXPECT(!bad, "Cancel returns false for unknown id");
}

//  Test: modify order


void test_modify() {
    std::cout << "\n[TEST] Modify order qty\n";
    devNull.str("");
    OrderBook ob(devNull);

    ob.addOrder(makeOrder("TEST", Side::BUY, 10000, 5));   // id=1
    ob.addOrder(makeOrder("TEST", Side::BUY,  9900, 5));   // id=2
    bool ok = ob.modifyOrder(1, 20);
    EXPECT(ok, "Modify returns true");

    // Now add a crossing sell for 20 units
    auto trades = ob.addOrder(makeOrder("TEST", Side::SELL, 9900, 20));
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

    ob.addOrder(makeOrder("AAPL", Side::BUY,  15000, 10));
    ob.addOrder(makeOrder("GOOG", Side::BUY,  20000, 10));

    // A sell on AAPL should not affect GOOG
    auto trades = ob.addOrder(makeOrder("AAPL", Side::SELL, 15000, 10));
    EXPECT(trades.size() == 1,           "AAPL trade executes");
    EXPECT(ob.bestBid("GOOG") == 20000,  "GOOG book unaffected");
}

//  Test: precision parsing

void test_precision_cases() {
    std::cout << "\n[TEST] Precision parsing\n";
    EXPECT(parsePrice("0.10") + parsePrice("0.20") == parsePrice("0.30"), "0.10 + 0.20 == 0.30 in cents");
    EXPECT(parsePrice("100") == parsePrice("100.00"), "100 == 100.00");
    EXPECT(parsePrice("99.99") == 9999, "99.99 parses to 9999");
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
    test_precision_cases();

    std::cout << "\n───────────────────────────────────────\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "───────────────────────────────────────\n";

    return failed == 0 ? 0 : 1;
}
