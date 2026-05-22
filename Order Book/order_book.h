#pragma once

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>

// alias
using Quantity = uint64_t;
using Price = uint64_t;
using NbOrders = uint64_t;
using OrderId = uint64_t;
using UserId = uint64_t;
using NbTrades = uint64_t;

// sides d'ordres
enum class Side : uint8_t {
    none = 0,
    buy = 1,
    sell = 2
};

enum class ErrorCode : uint8_t {
    no_error = 0,
    max_order_capacity = 1,
    max_price_capacity = 2,
    id_already_used = 3,
    wrong_side = 4,
    id_not_used = 5
};

// bornes de prix
constexpr Price MAX_PRICE_LEVEL = 10;

// nombre d'ordres max 
constexpr NbOrders MAX_ORDERS = 20;

// nombre de trades max
constexpr NbTrades MAX_TRADES = 30;

//###############################################
//#                O R D E R                    #
//###############################################
class Order {
    Price price_tick = 0;
    Quantity quantity = 0;
    Side side = Side::none;
    UserId user_id = 0;

    //chainage au sein du même Price Level
    Order* prev_in_price_level = nullptr;
    Order* next_in_price_level = nullptr;

    // constructeurs
    Order() = default;

    friend class OrderBook;

public:

    // getters
    Price get_price_tick() const;
    Quantity get_quantity() const;
    Side get_side() const;
    const Order* get_prev_in_price_level() const;
    const Order* get_next_in_price_level() const;
    UserId get_user_id() const;

};

//###############################################
//#           P R I C E   L E V E L             #
//###############################################
class PriceLevel {
    Order* head = nullptr; //First In
    Order* tail = nullptr; //Last In

    //chainer les niveaux de prix pour reconnecter le best bid/ask lors d'un cancel
    PriceLevel* prev = nullptr;
    PriceLevel* next = nullptr;

    Quantity total_quantity = 0; //volume

    // constructeurs
    PriceLevel() = default;

    friend class OrderBook;

public:

    // getters

    Quantity get_volume() const;
    const Order* get_head() const;
    const Order* get_tail() const;
    const PriceLevel* get_prev() const;
    const PriceLevel* get_next() const;

};

//###############################################
//#                 T R A D E                   #
//###############################################
class Trade {
    OrderId buy_id = 0;    
    UserId buy_user_id = 0;

    OrderId sell_id = 0;
    UserId sell_user_id = 0;

    Quantity quantity = 0;
    Price price_tick = 0;
};

//###############################################
//#       T R A D E   R E P O S I T O R Y       #
//###############################################
class TradeRepository {
    Trade trades[MAX_TRADES] = {};
};

//###############################################
//#            O R D E R   B O O K              #
//###############################################
//les ordres sont placés dans un memory pool indexé par id (accès par id = O(1))
class OrderBook {
    // buy
    PriceLevel* buy_levels_head = nullptr;
    PriceLevel* buy_levels_tail = nullptr;
    PriceLevel buy_levels[MAX_PRICE_LEVEL + 1] = {};

    // sell
    PriceLevel* sell_levels_head = nullptr;
    PriceLevel* sell_levels_tail = nullptr;
    PriceLevel sell_levels[MAX_PRICE_LEVEL + 1] = {};

    // order pool
    Order pool[MAX_ORDERS] = {};
    //NbOrders next_order_id = 0;

public:

    // constructeurs
    OrderBook() = default;

    // ajoute un ordre O(1) amorti quand chaque niveau de prix est actif, retourne un code d'erreur
    ErrorCode add(UserId _user_id, OrderId _id, Side _side, Quantity _quantity, Price _price_tick);

    // cancel un ordre O(1)
    ErrorCode cancel(OrderId _id);

    // execute TODO -> ajoute dans le TradeRepository les trades correspondants au macthing du best_bid avec 1 ou plusieurs best_ask

    // best bid O(1)
    //Todo retourner une erreur ou autre quand il n'y a pas de best (si vide)
    Price best_bid() const;

    Price worst_bid() const;

    // best ask O(1)
    //Todo retourner une erreur ou autre quand il n'y a pas de best (si vide)
    Price best_ask() const;

    Price worst_ask() const;

    // getters
    Price get_price(OrderId _id) const;

    Quantity get_quantity(OrderId _id) const;

    Side get_side(OrderId _id) const;

    UserId get_user_id(OrderId _id) const;

    const Order* get_order(OrderId _id) const;

    const PriceLevel* get_sell_price_level(Price _price) const;
    const PriceLevel* get_buy_price_level(Price _price) const;

    const PriceLevel* get_sell_price_levels_tail() const;
    const PriceLevel* get_buy_price_levels_tail() const;

    const PriceLevel* get_sell_price_levels_head() const;
    const PriceLevel* get_buy_price_levels_head() const;

private:

    //SELL ADD : On met à jour le PriceLevel et son Volume
    inline void update_pricelevel_when_adding_sell_order(OrderId _id, Price _price_tick, Quantity _quantity);

    //SELL ADD : On met à jour la liste chainée des niveaux de prix pour conserver le Best Ask
    // -> parcourir le sell level pour reconnecter les pointeurs = couteux
    inline void update_best_ask_when_adding_sell_order(Price _price_tick);

    //BUY ADD : On met à jour le PriceLevel et son Volume
    inline void update_pricelevel_when_adding_buy_order(OrderId _id, Price _price_tick, Quantity _quantity);

    //BUY ADD : On met à jour la liste chainée des niveaux de prix pour conserver le Best Bid
    // -> parcourir le buy level pour reconnecter les pointeurs = couteux
    inline void update_best_bid_when_adding_buy_order(Price _price_tick);
};

// Affichage d'un ordre au format "[Side Quantity @ PriceTick EUR]"
std::ostream& operator<<(std::ostream& os, const Order& o);

// Affichage d'un PriceLevel au format "[Volume=vol]"
std::ostream& operator<<(std::ostream& os, const PriceLevel& pl);

// Affiche la liste chaînée des ordres d'un PriceLevel de la tête vers la queue
// Format : HEAD <- [BUY qty @ price EUR] <- [BUY qty @ price EUR] <- TAIL
void print_pricelevel_orders(std::ostream& os, const PriceLevel* pl);

// Affichage de l'OrderBook : 3 colonnes côte à côte : pool | buy levels | sell levels
std::ostream& operator<<(std::ostream& os, const OrderBook& ob);

