#include "OrderBook.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

//  Helpers

std::string sideToStr(Side s) {
    return s == Side::BUY ? "BUY" : "SELL";
}

Price parsePrice(const std::string& str) {
    return static_cast<Price>(std::round(std::stod(str) * 100.0));
}

std::string priceToString(Price p) {
    std::ostringstream oss;
    oss << (p / 100) << "." << std::setfill('0') << std::setw(2) << (p % 100);
    return oss.str();
}


//  Constructor

OrderBook::OrderBook(std::ostream& tradeLog)
    : tradeLog_(tradeLog) {}

//  addOrder

std::vector<Trade> OrderBook::addOrder(Order order) {
    // Input validation: reject invalid price or quantity
    if (order.price <= 0 || order.quantity <= 0) {
        return {};
    }

    // price-time priority: map guarantees price priority, and the monotonic sequence/queue guarantees time priority (FIFO).
    order.id        = nextId_++;
    order.timestamp = sequence_++;

    auto& book = books_[order.symbol];

    // Register in index for cancel/modify support
    orderIndex_[order.id] = {
        order.symbol, order.side, order.price, true, order.quantity
    };

    if (order.side == Side::BUY) {
        book.bids[order.price].push(order);
    } else {
        book.asks[order.price].push(order);
    }

    return matchOrders(order.symbol);
}

//  cancelOrder

bool OrderBook::cancelOrder(uint64_t orderId) {
    auto it = orderIndex_.find(orderId);
    if (it == orderIndex_.end() || !it->second.active)
        return false;

    // lazy cancellation: Mark inactive – the order remains in the queue but will be skipped
    // when it reaches the front during matching. This avoids O(N) search and erase.
    it->second.active = false;
    return true;
}


//  modifyOrder
//  Behavior: cancels the original order and adds a replacement,
//  giving it a new time priority.

uint64_t OrderBook::modifyOrder(uint64_t orderId, int newQty) {
    auto it = orderIndex_.find(orderId);
    if (it == orderIndex_.end() || !it->second.active || newQty <= 0)
        return 0;

    auto& meta = it->second;

    // Build replacement order
    Order replacement;
    replacement.symbol   = meta.symbol;
    replacement.side     = meta.side;
    replacement.price    = meta.price;
    replacement.quantity = newQty;

    cancelOrder(orderId);
    uint64_t newId = nextId_;
    addOrder(replacement);
    return newId;
}


//  matchOrders  – core matching engine

std::vector<Trade> OrderBook::matchOrders(const std::string& symbol) {
    std::vector<Trade> trades;
    auto& book = books_[symbol];

    while (!book.bids.empty() && !book.asks.empty()) {

        // Skip cancelled orders at the front of the best bid level
        while (!book.bids.empty()) {
            auto& [bestBidPrice, bidQueue] = *book.bids.begin();
            while (!bidQueue.empty()) {
                auto& front = bidQueue.front();
                auto  idx   = orderIndex_.find(front.id);
                if (idx != orderIndex_.end() && !idx->second.active) {
                    bidQueue.pop();
                } else break;
            }
            if (bidQueue.empty()) book.bids.erase(book.bids.begin());
            else break;
        }

        // Skip cancelled orders at the front of the best ask level
        while (!book.asks.empty()) {
            auto& [bestAskPrice, askQueue] = *book.asks.begin();
            while (!askQueue.empty()) {
                auto& front = askQueue.front();
                auto  idx   = orderIndex_.find(front.id);
                if (idx != orderIndex_.end() && !idx->second.active) {
                    askQueue.pop();
                } else break;
            }
            if (askQueue.empty()) book.asks.erase(book.asks.begin());
            else break;
        }

        if (book.bids.empty() || book.asks.empty()) break;

        auto& [bestBidPrice, bidQueue] = *book.bids.begin();
        auto& [bestAskPrice, askQueue] = *book.asks.begin();

        // No cross – done
        if (bestBidPrice < bestAskPrice) break;

        Order& bid = bidQueue.front();
        Order& ask = askQueue.front();

        // maker-price execution rule:
        // Trade executes at the resting (maker) order's price.
        // The ask arrived first if its timestamp is lower; otherwise bid did.
        Price tradePrice = (ask.timestamp < bid.timestamp) ? ask.price : bid.price;

        int matchedQty = std::min(bid.quantity, ask.quantity);

        Trade t { symbol, tradePrice, matchedQty, bid.id, ask.id };
        trades.push_back(t);
        logTrade(t);

        bid.quantity -= matchedQty;
        ask.quantity -= matchedQty;

        // Update index remaining quantities
        if (orderIndex_.count(bid.id)) orderIndex_[bid.id].remainingQty = bid.quantity;
        if (orderIndex_.count(ask.id)) orderIndex_[ask.id].remainingQty = ask.quantity;

        if (bid.quantity == 0) {
            if (orderIndex_.count(bid.id)) orderIndex_[bid.id].active = false;
            bidQueue.pop();
        }
        if (ask.quantity == 0) {
            if (orderIndex_.count(ask.id)) orderIndex_[ask.id].active = false;
            askQueue.pop();
        }

        if (bidQueue.empty()) book.bids.erase(book.bids.begin());
        if (askQueue.empty()) book.asks.erase(book.asks.begin());
    }

    return trades;
}

//  printBook

void OrderBook::printBook(const std::string& symbol) const {
    auto it = books_.find(symbol);

    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout <<   "║     Order Book  [" << symbol;
    std::cout << std::string(18 - symbol.size(), ' ') << "]  ║\n";
    std::cout <<   "╠══════════════════════════════════════╣\n";

    if (it == books_.end() || (it->second.bids.empty() && it->second.asks.empty())) {
        std::cout << "║         (book is empty)              ║\n";
        std::cout << "╚══════════════════════════════════════╝\n";
        return;
    }

    const auto& book = it->second;

    std::cout << "║  SELL side (asks)                    ║\n";
    std::cout << "║  Price         Qty                   ║\n";
    // Print asks from worst to best (highest to lowest) for visual clarity
    std::vector<std::pair<Price,int>> askLevels;
    for (auto& [price, q] : book.asks) {
        int total = 0;
        std::queue<Order> tmp = q;
        while (!tmp.empty()) { 
            auto idx = orderIndex_.find(tmp.front().id);
            if (idx != orderIndex_.end() && idx->second.active) {
                total += tmp.front().quantity; 
            }
            tmp.pop(); 
        }
        if (total > 0) askLevels.push_back({price, total});
    }
    for (auto rit = askLevels.rbegin(); rit != askLevels.rend(); ++rit) {
        std::cout << "║  " << std::setw(10) << priceToString(rit->first)
                  << "  " << std::setw(6) << rit->second
                  << "                   ║\n";
    }

    std::cout << "║──────────────────────────────────────║\n";
    std::cout << "║  BUY  side (bids)                    ║\n";
    std::cout << "║  Price         Qty                   ║\n";
    for (auto& [price, q] : book.bids) {
        int total = 0;
        std::queue<Order> tmp = q;
        while (!tmp.empty()) { 
            auto idx = orderIndex_.find(tmp.front().id);
            if (idx != orderIndex_.end() && idx->second.active) {
                total += tmp.front().quantity; 
            }
            tmp.pop(); 
        }
        if (total > 0)
            std::cout << "║  " << std::setw(10) << priceToString(price)
                      << "  " << std::setw(6) << total
                      << "                   ║\n";
    }

    std::cout << "╚══════════════════════════════════════╝\n";
}

//  bestBid / bestAsk

Price OrderBook::bestBid(const std::string& symbol) const {
    auto it = books_.find(symbol);
    if (it == books_.end() || it->second.bids.empty()) return 0;
    for (const auto& [price, q] : it->second.bids) {
        std::queue<Order> tmp = q;
        while (!tmp.empty()) {
            auto idx = orderIndex_.find(tmp.front().id);
            if (idx != orderIndex_.end() && idx->second.active) {
                return price;
            }
            tmp.pop();
        }
    }
    return 0;
}

Price OrderBook::bestAsk(const std::string& symbol) const {
    auto it = books_.find(symbol);
    if (it == books_.end() || it->second.asks.empty()) return 0;
    for (const auto& [price, q] : it->second.asks) {
        std::queue<Order> tmp = q;
        while (!tmp.empty()) {
            auto idx = orderIndex_.find(tmp.front().id);
            if (idx != orderIndex_.end() && idx->second.active) {
                return price;
            }
            tmp.pop();
        }
    }
    return 0;
}

//  loadFromFile


void OrderBook::loadFromFile(const std::string& filename,
                             const std::string& symbol,
                             Side side,
                             int defaultQty) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filename);

    std::string line;
    int loaded = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        try {
            Price price = parsePrice(line);
            Order o;
            o.symbol   = symbol;
            o.side     = side;
            o.price    = price;
            o.quantity = defaultQty;
            addOrder(o);
            ++loaded;
        } catch (...) {
            // skip malformed lines
        }
    }
    std::cout << "[INFO] Loaded " << loaded << " " << sideToStr(side)
              << " orders for " << symbol << " from " << filename << "\n";
}

//  logTrade  – writes to the trade log stream

void OrderBook::logTrade(const Trade& t) {
    tradeLog_ << "TRADE | " << t.symbol
              << " | Price: " << priceToString(t.price)
              << " | Qty: " << t.quantity
              << " | BuyOrderId: "  << t.buyOrderId
              << " | SellOrderId: " << t.sellOrderId
              << "\n";
}
