#include "order_book.h"

class OrderIdGenerator {
    OrderId id = 0;

public:
    OrderIdGenerator() = default;

    OrderId reset() {
        OrderId old_id = id;
        id = 0;
        return old_id;
    }

    OrderId next() {
        id++;
        return id;
    }
};

//###############################################
//#                  M A I N                    #
//###############################################
int main(){
    OrderIdGenerator id_gen;
    TradeRepository* trades = new TradeRepository();
    OrderBook* orders = new OrderBook(trades);

    UserId client1 = 111;
    UserId client2 = 222;
    UserId client3 = 333;
    
    orders->add(client1, id_gen.next(), Side::buy, 100, 10); 
    orders->add(client2, id_gen.next(), Side::sell, 50, 9);
    orders->add(client3, id_gen.next(), Side::sell, 60, 10);

    std::cout << *orders << std::endl << std::endl;

    orders->execute(); 

    std::cout << *orders << std::endl << std::endl;

    std::cout << *trades << std::endl << std::endl;

    return 0;
}