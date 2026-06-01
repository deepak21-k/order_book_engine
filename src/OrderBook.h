#pragma once

#include <map>
#include <queue>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <functional>

//  Order side enum  (safer than raw strings)

enum class Side { BUY, SELL };

std::string sideToStr(Side s);

//  Price type (fixed-point integer cents)
using Price = int64_t;
Price parsePrice(const std::string& str);
std::string priceToString(Price p);

//  Order struct

struct Order {
    uint64_t    id;        // unique order id
    std::string symbol;
    Side        side;
    Price       price;
    int         quantity;
    uint64_t    timestamp; // insertion sequence (for FIFO at same price)
};


//  Trade struct  (result of a match)

struct Trade {
    std::string symbol;
    Price       price;
    int         quantity;
    uint64_t    buyOrderId;
    uint64_t    sellOrderId;
};

//  OrderBook class
//
//  Data structure:
//    BUY  side : std::map<price, queue<Order>, greater<>>
//                  → best bid (highest price) at begin()
//    SELL side : std::map<price, queue<Order>>
//                  → best ask (lowest price)  at begin()
//
//  Complexity:
//    addOrder   – O(log N)   insert into map
//    cancelOrder– O(log N)   lookup + mark cancelled
//    matchOrders– O(k log N) where k = trades executed

class OrderBook {
public:
    explicit OrderBook(std::ostream& tradeLog);

    // Place a new limit order; returns matched trades (if any)
    std::vector<Trade> addOrder(Order order);

    // Cancel an outstanding order by id; returns true if found
    bool cancelOrder(uint64_t orderId);

    // Modify qty of an outstanding order.
    // Behavior: Cancels the original order and re-inserts a new one. This ensures that the modified order gets a new time priority (loses original time priority).
    // Returns the new order ID on success, or 0 on failure.
    uint64_t modifyOrder(uint64_t orderId, int newQty);

    // Print current state of the book to stdout
    void printBook(const std::string& symbol) const;

    // Best bid / ask accessors (returns 0 if side is empty)
    Price bestBid(const std::string& symbol) const;
    Price bestAsk(const std::string& symbol) const;

    // Load orders from a file (one price per line, random qty assigned)
    void loadFromFile(const std::string& filename,
                      const std::string& symbol,
                      Side side,
                      int defaultQty = 10);

private:
    // price → FIFO queue of orders at that price level
    using BuyLevels  = std::map<Price, std::queue<Order>, std::greater<Price>>;
    using SellLevels = std::map<Price, std::queue<Order>>;

    struct SymbolBook {
        BuyLevels  bids;
        SellLevels asks;
    };

    std::map<std::string, SymbolBook> books_;

    // id → pointer metadata for fast cancel lookup
    struct OrderMeta {
        std::string symbol;
        Side        side;
        Price       price;
        bool        active;
        int         remainingQty;
    };
    std::map<uint64_t, OrderMeta> orderIndex_;

    uint64_t nextId_   = 1;
    uint64_t sequence_ = 0;   // monotonic timestamp

    std::ostream& tradeLog_;  // output stream for trade records

    // Internal helpers
    std::vector<Trade> matchOrders(const std::string& symbol);
    void logTrade(const Trade& t);
};
