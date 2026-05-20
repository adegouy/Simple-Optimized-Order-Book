// order_book.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

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

// sides d'ordres
enum class Side : uint8_t {
    none=0,
    buy = 1,
    sell = 2
};

enum class ErrorCode : uint8_t {
    no_error = 0,
    max_order_capacity = 1, 
    max_price_capacity = 2, 
    id_already_used = 3, 
    wrong_side = 4
};

// bornes de prix
constexpr Price MAX_PRICE_LEVEL = 10;

// nombre d'ordres max 
constexpr NbOrders MAX_ORDERS = 20;

//###############################################
//#                O R D E R                    #
//###############################################
struct Order {
    Price price_tick = 0;
    Quantity quantity = 0;
    Side side = Side::none;

    //chainage au sein du même Price Level
    Order* prev_in_price_level = nullptr;
    Order* next_in_price_level = nullptr;

    // constructeurs
    Order() = default;
};

//###############################################
//#           P R I C E   L E V E L             #
//###############################################
struct PriceLevel {
    Order* head = nullptr; //First In
    Order* tail = nullptr; //Last In

    //chainer les niveaux de prix pour reconnecter le best bid/ask lors d'un cancel
    PriceLevel* prev = nullptr;
    PriceLevel* next = nullptr;

    Quantity total_quantity = 0; //volume

    // constructeurs
    PriceLevel() = default;
};

//###############################################
//#            O R D E R   B O O K              #
//###############################################
//les ordres sont placés dans un memory pool indexé par id (accès par id = O(1))
struct OrderBook {
    // buy
    PriceLevel* buy_levels_head = nullptr; 
    PriceLevel* buy_levels_tail = nullptr;
    PriceLevel buy_levels[MAX_PRICE_LEVEL+1] = {};

    // sell
    PriceLevel* sell_levels_head = nullptr;
    PriceLevel* sell_levels_tail = nullptr; 
    PriceLevel sell_levels[MAX_PRICE_LEVEL+1] = {};

    // order pool
    Order pool[MAX_ORDERS] = {};
    //NbOrders next_order_id = 0;

    // constructeurs
    OrderBook() = default;  

    // ajoute un ordre O(1) amorti quand chaque niveau de prix est actif, retourne un code d'erreur
    ErrorCode add_order(OrderId _id, Side _side, Quantity _quantity, Price _price_tick) {
        
        //capacité de la pool atteinte donc return 1
        if (_id >= MAX_ORDERS) return ErrorCode::max_order_capacity;

        //prix au delà du prix max donc retourne 2
        if (_price_tick > MAX_PRICE_LEVEL) return ErrorCode::max_price_capacity;

        //id déjà pris
        if (pool[_id].side != Side::none) return ErrorCode::id_already_used;

        //side incorrecte
        if (_side != Side::buy && _side != Side::sell) return ErrorCode::wrong_side;
        
        //mettre l'ordre dans la pool à la main (vs une affectation par move plus lente à cause des checks)
        pool[_id].price_tick = _price_tick;
        pool[_id].quantity = _quantity;
        pool[_id].side = _side;

        //Buy
        if (_side == Side::buy) {

            //mise à jour du price level et du volume  
            update_pricelevel_when_adding_buy_order(_id, _price_tick, _quantity);

            //mise à jour des pointeurs de PriceLevel, peut être en O(n) dans certains cas
            update_best_bid_when_adding_buy_order(_price_tick);
            
        }

        //Sell
        else if (_side == Side::sell) {

            //mise à jour du price level et du volume  
            update_pricelevel_when_adding_sell_order(_id, _price_tick, _quantity);

            //mise à jour des pointeurs de PriceLevel, peut être en O(n) dans certains cas
            update_best_ask_when_adding_sell_order(_price_tick);
        }

        return ErrorCode::no_error;
    }

    // cancel un ordre O(1)
    //Todo

    // best bid O(1)
    Price best_bid() {
        if (buy_levels_tail == nullptr) return 0;
        return static_cast<Price>(buy_levels_tail - buy_levels);
    }

    Price worst_bid() {
        if (buy_levels_head == nullptr) return 0;
        return static_cast<Price>(buy_levels_head - buy_levels);
    }

    // best ask O(1)
    Price best_ask() {
        if (sell_levels_head == nullptr) return 0;
        return static_cast<Price>(sell_levels_head - sell_levels);
    }

    Price worst_ask() {
        if (sell_levels_tail == nullptr) return 0;
        return static_cast<Price>(sell_levels_tail - sell_levels);
    }

    private:        

        //SELL ADD : On met à jour le PriceLevel et son Volume
        inline void update_pricelevel_when_adding_sell_order(OrderId _id, Price _price_tick, Quantity _quantity) {
            
            // s'il n'y a aucun autre ordre à ce prix
            if (sell_levels[_price_tick].head == nullptr) {

                // on met l'ordre dans la liste triée des niveaux de prix en guise de tête et de queue
                sell_levels[_price_tick].head = &pool[_id];
                sell_levels[_price_tick].tail = &pool[_id];

                // pas besoin de mettre à jour les pointeurs de la liste chainée d'ordres au sein du même PriceLevel car il est le seul ordre              
            }

            // il y a d'autres ordres à ce prix     
            else {
                // on ajoute l'ordre à la queue de la liste d'ordres à ce même prix
                Order* old_tail_ptr = sell_levels[_price_tick].tail;
                sell_levels[_price_tick].tail = &pool[_id];

                //on chaine les ordres
                old_tail_ptr->next_in_price_level = &pool[_id];
                pool[_id].prev_in_price_level = old_tail_ptr;
            }

            //mise à jour du volume 
            sell_levels[_price_tick].total_quantity += _quantity;
        }

        //SELL ADD : On met à jour la liste chainée des niveaux de prix pour conserver le Best Ask
        // -> parcourir le sell level pour reconnecter les pointeurs = couteux
        inline void update_best_ask_when_adding_sell_order(Price _price_tick) {

            Price old_best_ask = best_ask();
            Price old_worst_ask = worst_ask();

            // si il n'y a aucun PriceLevel d'actif, le nouveau PriceLevel devient le best ask
            if (sell_levels_tail == nullptr) {
                sell_levels_tail = &sell_levels[_price_tick];
                sell_levels_head = &sell_levels[_price_tick];
                return;
            }

            //si le nouveau prix est == au best ask ou au worst ask alors on ne fait rien
            if (_price_tick == old_best_ask || _price_tick == old_worst_ask) {
                return;
            }

            //si le nouveau prix < best ask alors facile, pas de parcours necessaire
            if (_price_tick < old_best_ask) {

                //on garde le nouveau min des PriceLevels
                PriceLevel* old_head_ptr = sell_levels_head;
                sell_levels_head = &sell_levels[_price_tick];

                //on chaine les price levels (le nouveau max et l'ancien)
                old_head_ptr->prev = sell_levels_head;
                sell_levels_head->next = old_head_ptr;

                return;
            }

            //si le nouveau prix est plus élevé que le worst sell, facile aussi, pas de parcours necessaire
            if (_price_tick > old_worst_ask) {

                //on garde le nouveau max des PriceLevels
                PriceLevel* old_tail_ptr = sell_levels_tail;
                sell_levels_tail = &sell_levels[_price_tick];

                //on chaine les price levels (le nouveau max et l'ancien)
                old_tail_ptr->next = sell_levels_tail;
                sell_levels_tail->prev = old_tail_ptr;

                return;
            }

            // Sinon il faut effectuer un parcours. On part du min et on parcours les PriceLevels actifs jusqu'à encadrer notre valeur. 
            // Puis on reconnecte les pointeurs. 
            // C'est assez couteux mais pour l'instant mais je ne vois pas comment faire mieux  
            // ici la valeur de prix à insérer est forcément dans ]best_ask, worst_ask[     
            PriceLevel* n_minus_1 = sell_levels_head;
            PriceLevel* n_plus_1 = sell_levels_head->next;

            // on boucle, 
            // n_plus_1 ne peut jamais être égal à nullptr et il y a forcément une condition d'arrêt
            // car ici la valeur de prix à insérer est forcément dans ]best_ask, worst_ask[              
            while (static_cast<Price>(n_plus_1 - sell_levels) < _price_tick) {
                n_minus_1 = n_plus_1;
                n_plus_1 = n_plus_1->next;
            }

            //maintenant on veut être sur d'encadrer strictement la valeur
            if ((n_plus_1 - sell_levels) == _price_tick) {
                n_plus_1 = n_plus_1->next;
            }

            // maintenant on reconnecte les pointeurs
            sell_levels[_price_tick].next = n_plus_1;
            sell_levels[_price_tick].prev = n_minus_1;
            n_minus_1->next = &sell_levels[_price_tick];
            n_plus_1->prev = &sell_levels[_price_tick];
        
        }

        //BUY ADD : On met à jour le PriceLevel et son Volume
        inline void update_pricelevel_when_adding_buy_order(OrderId _id, Price _price_tick, Quantity _quantity) {

            // s'il n'y a aucun autre ordre à ce prix
            if (buy_levels[_price_tick].head == nullptr) {

                // on met l'ordre dans la liste triée des niveaux de prix en guise de tête et de queue
                buy_levels[_price_tick].head = &pool[_id];
                buy_levels[_price_tick].tail = &pool[_id];

                // pas besoin de mettre à jour les pointeurs de la liste chainée d'ordres au sein du même PriceLevel car il est le seul ordre              
            }

            // il y a d'autres ordres à ce prix     
            else {
                // on ajoute l'ordre à la queue de la liste d'ordres à ce même prix
                Order* old_tail_ptr = buy_levels[_price_tick].tail;
                buy_levels[_price_tick].tail = &pool[_id];

                //on chaine les ordres
                old_tail_ptr->next_in_price_level = &pool[_id];
                pool[_id].prev_in_price_level = old_tail_ptr;
            }

            //mise à jour du volume 
            buy_levels[_price_tick].total_quantity += _quantity;
        }


        //BUY ADD : On met à jour la liste chainée des niveaux de prix pour conserver le Best Bid
        // -> parcourir le buy level pour reconnecter les pointeurs = couteux
        inline void update_best_bid_when_adding_buy_order(Price _price_tick) {

            Price old_best_bid = best_bid();   
            Price old_worst_bid = worst_bid();            

            // si il n'y a aucun PriceLevel d'actif, le nouveau PriceLevel devient le best bid
            if (buy_levels_tail == nullptr) {
                buy_levels_tail = &buy_levels[_price_tick];
                buy_levels_head = &buy_levels[_price_tick];
                return;
            }

            //si le nouveau prix est == au best buy ou au worst buy alors on ne fait rien
            if (_price_tick == old_best_bid || _price_tick == old_worst_bid) {
                return;
            }    
            
            //si le nouveau prix > best bid alors facile, pas de parcours necessaire
            if (_price_tick > old_best_bid) {

                //on garde le nouveau max des PriceLevels
                PriceLevel* old_tail_ptr = buy_levels_tail;
                buy_levels_tail = &buy_levels[_price_tick];

                //on chaine les price levels (le nouveau max et l'ancien)
                old_tail_ptr->next = buy_levels_tail;
                buy_levels_tail->prev = old_tail_ptr;

                return;
            }

            //si le nouveau prix est moins élevé que le worst bid, facile aussi, pas de parcours necessaire
            if (_price_tick < old_worst_bid ) {

                //on garde le nouveau min des PriceLevels
                PriceLevel* old_head_ptr = buy_levels_head;
                buy_levels_head = &buy_levels[_price_tick];

                //on chaine les price levels (le nouveau min et l'ancien)
                old_head_ptr->prev = buy_levels_head;
                buy_levels_head->next = old_head_ptr;

                return;
            }

            // Sinon il faut effectuer un parcours. On part du max et on parcours les PriceLevels actifs jusqu'à encadrer notre valeur. 
            // Puis on reconnecte les pointeurs. 
            // C'est assez couteux mais pour l'instant mais je ne vois pas comment faire mieux  
            // ici la valeur de prix à insérer est forcément dans ]worst_bid, best_bid[     
            PriceLevel* n_plus_1 = buy_levels_tail;
            PriceLevel* n_minus_1 = buy_levels_tail->prev;

            //on boucle, 
            // n_minus ne peut jamais être égal à nullptr et il y a forcément une condition d'arrêt
            // car ici la valeur de prix à insérer est forcément dans ]worst_bid, best_bid[     
            while (static_cast<Price>(n_minus_1 - buy_levels) > _price_tick) {
                n_plus_1 = n_minus_1;
                n_minus_1 = n_minus_1->prev;
            }
            
            //maintenant on veut être sur d'encadrer strictement la valeur
            if ((n_minus_1 - buy_levels) == _price_tick) {
                n_minus_1 = n_minus_1->prev;
            }

            // maintenant on reconnecte les pointeurs
            buy_levels[_price_tick].prev = n_minus_1;
            buy_levels[_price_tick].next = n_plus_1;
            n_minus_1->next = &buy_levels[_price_tick];
            n_plus_1->prev = &buy_levels[_price_tick];
        }
};

// Affichage d'un ordre au format "[Side Quantity @ PriceTick EUR]"
inline std::ostream& operator<<(std::ostream& os, const Order& o) {
    if (o.side == Side::none) {
        os << "---";
        return os;
    }

    const char* side_str = "NONE";
    switch (o.side) {
        case Side::buy:  side_str = "BUY";  break;
        case Side::sell: side_str = "SELL"; break;
        default:         side_str = "NONE"; break;
    }

    os << '[' << side_str << ' ' << o.quantity << " @ " << o.price_tick << " EUR]";
    return os;
}

// Affichage d'un PriceLevel au format "[Volume=vol]"
inline std::ostream& operator<<(std::ostream& os, const PriceLevel& pl) {
    if (pl.total_quantity == 0) {
        os << "---";
        return os;
    }

    os << "[Volume=" << pl.total_quantity << "]";
    return os;
}

// Affiche la liste chaînée des ordres d'un PriceLevel de la tête vers la queue
// Format : HEAD <- [BUY qty @ price EUR] <- [BUY qty @ price EUR] <- TAIL
inline void print_pricelevel_orders(std::ostream& os, const PriceLevel* pl) {
    if (pl == nullptr) return;
    // n'affiche que si actif et non vide
    if (pl->head == nullptr) return;

    os << "HEAD <- ";
    Order* cur = pl->head;
    while (cur != nullptr) {
        os << *cur; // utilise l'opérateur<< déjà défini pour Order
        if (cur->next_in_price_level != nullptr) os << " <- ";
        cur = cur->next_in_price_level;
    }
    os << " <- TAIL";
}

// Affichage de l'OrderBook : 3 colonnes côte à côte : pool | buy levels | sell levels
inline std::ostream& operator<<(std::ostream& os, const OrderBook& ob) {
    // on fixe la taille des colonnes
    constexpr size_t COL_WIDTH = 22;

    //on calcule le nombre de lignes max
    size_t pool_rows = 1 + MAX_ORDERS; // header + taille de la pool
    size_t price_rows = 1 + (MAX_PRICE_LEVEL + 1); // header + nombre de prix max

    size_t max_rows = std::max(pool_rows, price_rows);

    //fonction pour tronquer
    auto truncate = [](const std::string& s) {
        constexpr size_t W = COL_WIDTH;
        if (s.size() <= W) return s;
        return s.substr(0, W);
    };

    for (size_t row = 0; row < max_rows; ++row) {
        std::string a, b, c;

        // Colonne de Pool
        if (row == 0) {
            a = "-- [ ORDER POOL ] --";
        } 
        
        else {
            NbOrders idx = static_cast<NbOrders>(row - 1);
            std::ostringstream ss;
            ss << idx << ": " << ob.pool[idx];
            a = ss.str();
        }

        // Colonne des Buy
        if (row == 0) {
            b = "-- [ BUY LEVELS ] --";
        } 
        
        else {
            Price p = static_cast<Price>(row - 1);
            if (p <= MAX_PRICE_LEVEL) {
                std::ostringstream ss;
                ss << p << ": " << ob.buy_levels[p];
                b = ss.str();
            }
        }

        // Colonne des Sell
        if (row == 0) {
            c = "-- [ SELL LEVELS ] --";
        } 
        
        else {
            Price p = static_cast<Price>(row - 1);
            if (p <= MAX_PRICE_LEVEL) {
                std::ostringstream ss;
                ss << p << ": " << ob.sell_levels[p];
                c = ss.str();
            }
        }

        // on tronque et on aligne
        std::string ta = truncate(a);
        std::string tb = truncate(b);
        std::string tc = truncate(c);

        os << std::left << std::setw(static_cast<int>(COL_WIDTH)) << ta
           << std::left << std::setw(static_cast<int>(COL_WIDTH)) << tb
           << std::left << std::setw(static_cast<int>(COL_WIDTH)) << tc;

        if (row + 1 < max_rows) os << std::endl;
    }

    return os;
}

class OrderIdGenerator {
    OrderId id = 0;

public :
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
int main()
{

    OrderIdGenerator id_gen;

    OrderBook* orders = new OrderBook();

    orders->add_order(id_gen.next(), Side::sell, 1, 3);
    std::cout << orders->best_ask() << std::endl;

    orders->add_order(id_gen.next(), Side::sell, 1, 6);
    std::cout << orders->best_ask() << std::endl;

    orders->add_order(id_gen.next(), Side::sell, 1, 5);
    std::cout << orders->best_ask() << std::endl;

    orders->add_order(id_gen.next(), Side::sell, 1, 4);
    std::cout << orders->best_ask() << std::endl;

    orders->add_order(id_gen.next(), Side::sell, 1, 2);
    std::cout << orders->best_ask() << std::endl;

    orders->add_order(id_gen.next(), Side::sell, 1, 7);
    std::cout << orders->best_ask() << std::endl; 

    orders->add_order(id_gen.next(), Side::sell, 1, 9);
    orders->add_order(id_gen.next(), Side::sell, 2, 9);
    orders->add_order(id_gen.next(), Side::sell, 1, 9);

    print_pricelevel_orders(std::cout, orders->sell_levels_tail); std::cout << std::endl;

    std::cout << std::endl; std::cout << std::endl;

    std::cout << *orders << std::endl;

}


