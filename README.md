# Order Book Engine

A C++ implementation of a **price-time priority limit order book** with a continuous matching engine — the core data structure powering every modern exchange.

---

## Features

- **Price-time (FIFO) priority matching** — orders at the same price level are matched in arrival order
- **Efficient data structure** — `std::map<price, std::queue<Order>>` gives O(log N) insert and O(1) best-price access, versus the O(n log n) re-sort-on-insert approach
- **Cancel & modify support** — lazy-deletion cancel, modify re-inserts at new time priority
- **Multi-symbol** — a single `OrderBook` instance handles independent books per ticker
- **File I/O** — loads buy/sell orders from `.txt` files, writes matched trades to `order_output.txt`
- **Test suite** — 9 test scenarios with 30 assertions covering matching, partial fills, price priority, cancel, modify, input validation, and symbol isolation

---

## Data Structure Design

```
OrderBook
├── books_["AAPL"]
│   ├── bids: map<price, queue<Order>, greater<>>   ← best bid at begin()
│   └── asks: map<price, queue<Order>>              ← best ask at begin()
└── orderIndex_: map<id, OrderMeta>                 ← O(log N) cancel/modify
```

**Why `map<price, queue<Order>>`?**

| Operation      | Naïve (sorted vector) | This engine         |
|----------------|-----------------------|---------------------|
| Insert order   | O(n log n)            | **O(log N)**        |
| Best bid/ask   | O(1)                  | **O(1)**            |
| Match / erase  | O(n)                  | **O(log N)**        |
| Cancel         | O(n) scan             | **O(log N)** via index |

---

## Build

**Requirements:** C++17, g++ or clang++, CMake ≥ 3.14 (optional)

### With CMake
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### With g++ directly
```bash
# Main binary
g++ -std=c++17 -O2 -Isrc src/OrderBook.cpp src/main.cpp -o order_book_engine

# Test binary
g++ -std=c++17 -O2 -Isrc src/OrderBook.cpp Tests/test_orderbook.cpp -o run_tests
```

---

## Usage

```bash
# Run with built-in demo orders
./order_book_engine

# Run with your own order files (one price per line)
./order_book_engine data/buy_orders.txt data/sell_orders.txt
```

### Input format (`buy_orders.txt` / `sell_orders.txt`)
One price per line (floating point):
```
150.25
149.80
151.00
```

### Output
Matched trades are written to **`order_output.txt`**:
```
TRADE | AAPL | Price: 150.0000 | Qty: 5 | BuyOrderId: 1 | SellOrderId: 5
TRADE | AAPL | Price: 149.0000 | Qty: 1 | BuyOrderId: 3 | SellOrderId: 6
```

---

## Running Tests

```bash
./run_tests
```

Expected output:
```
Results: 30 passed, 0 failed
```

Tests cover: basic match, partial fill, no-cross, price priority, cancel, modify, multi-symbol independence.

---

## Matching Rules

1. A trade occurs when `bestBid >= bestAsk`
2. Trade executes at the **maker's price** (the resting order's price)
3. Partial fills leave the remaining quantity in the book
4. Cancelled orders are skipped lazily at match time (no O(n) scan)

---

## Project Structure

```
order_book_engine/
├── src/
│   ├── OrderBook.h      # Class interface, Order/Trade structs, Side enum
│   ├── OrderBook.cpp    # Matching engine implementation
│   └── main.cpp         # Entry point, file I/O, demo
├── Tests/
│   └── test_orderbook.cpp
├── data/
│   ├── buy_orders.txt
│   └── sell_orders.txt
├── CMakeLists.txt
└── README.md
```

---

## Known Limitations

- Prices are currently stored as `double`, which can introduce floating-point precision issues.
- Thread safety is not implemented; concurrent access requires external locking.
- Market orders are not yet supported.

---

## Possible Extensions

- **Market orders** — skip price check, match immediately at any available price
- **Stop orders** — trigger when market price crosses a threshold
- **Order book depth** — expose bid/ask depth at N levels
- **Benchmarking** — throughput test (orders/sec) with large synthetic datasets
- **Persistent logging** — structured trade log (CSV / binary)
