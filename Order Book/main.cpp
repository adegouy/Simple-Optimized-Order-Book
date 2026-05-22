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

    OrderBook* orders = new OrderBook();

    UserId client1 = 111;

    orders->add(client1, id_gen.next(), Side::sell, 1, 3);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 6);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 5);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 4);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 2);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 7);
    std::cout << orders->best_ask() << std::endl;

    orders->add(client1, id_gen.next(), Side::sell, 1, 9);
    orders->add(client1, id_gen.next(), Side::sell, 2, 9);
    orders->add(client1, id_gen.next(), Side::sell, 1, 9);

    OrderId test_id = id_gen.next();
    orders->add(client1, test_id, Side::sell, 1, 9);

    print_pricelevel_orders(std::cout, orders->get_sell_price_levels_tail()); std::cout << std::endl;

    std::cout << std::endl; std::cout << std::endl;

    std::cout << *orders << std::endl;

    orders->cancel(test_id);

    std::cout << *orders << std::endl;

}