
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <string>
using namespace std;

// Structure to represent an order
struct Order {
    string symbol;
    string side;  // BUY or SELL
    double price;
    int quantity;
};

class OrderBook {
public:
    void addOrder(const Order& order) {
        orders[order.symbol][order.side].push_back(order);
        sortOrders(order.symbol);   // maintain priority
        matchOrders(order.symbol);
    }

    void printOrderBook(const string& symbol) {
        cout << "\n====== Order Book (" << symbol << ") ======\n";

        cout << "\nBUY Orders:\n";
        for (auto &o : orders[symbol]["BUY"]) {
            cout << "Price: " << o.price 
                 << " | Qty: " << o.quantity << endl;
        }

        cout << "\nSELL Orders:\n";
        for (auto &o : orders[symbol]["SELL"]) {
            cout << "Price: " << o.price 
                 << " | Qty: " << o.quantity << endl;
        }

        cout << "=====================================\n";
    }

private:
    map<string, map<string, vector<Order>>> orders;

    // Sort orders: BUY high→low, SELL low→high
    void sortOrders(const string& symbol) {
        auto &buy = orders[symbol]["BUY"];
        auto &sell = orders[symbol]["SELL"];

        sort(buy.begin(), buy.end(), [](Order &a, Order &b) {
            return a.price > b.price;   // highest buy first
        });

        sort(sell.begin(), sell.end(), [](Order &a, Order &b) {
            return a.price < b.price;   // lowest sell first
        });
    }

    // Matching logic
    void matchOrders(const string& symbol) {
        auto &buy = orders[symbol]["BUY"];
        auto &sell = orders[symbol]["SELL"];

        while (!buy.empty() && !sell.empty()) {
            Order &b = buy.front();
            Order &s = sell.front();

            if (b.price >= s.price) {
                int matchedQty = min(b.quantity, s.quantity);

                cout << "Matched Trade -> "
                     << matchedQty << " units at price "
                     << s.price << endl;

                b.quantity -= matchedQty;
                s.quantity -= matchedQty;

                if (b.quantity == 0) buy.erase(buy.begin());
                if (s.quantity == 0) sell.erase(sell.begin());
            } else {
                break; // no match possible
            }
        }
    }
};

int main() {
    OrderBook ob;

    string ticker = "AAPL";

    // Sample orders
    ob.addOrder({ticker, "BUY", 100, 10});
    ob.addOrder({ticker, "SELL", 95, 5});
    ob.addOrder({ticker, "SELL", 105, 7});
    ob.addOrder({ticker, "BUY", 102, 6});
    ob.addOrder({ticker, "BUY", 98, 8});
    ob.addOrder({ticker, "SELL", 99, 4});

    // Print remaining order book
    ob.printOrderBook(ticker);

    return 0;
}