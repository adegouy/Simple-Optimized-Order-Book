#include "order_book.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>

//###############################################
//#                O R D E R                    #
//###############################################

Price Order::get_price_tick() const {
    return price_tick;
}

Quantity Order::get_quantity() const {
    return quantity;
}

Side Order::get_side() const {
    return side;
}

const Order* Order::get_prev_in_price_level() const {
    return prev_in_price_level;
}

const Order* Order::get_next_in_price_level() const {
    return next_in_price_level;
}

UserId Order::get_user_id() const {
    return user_id;
}

//###############################################
//#           P R I C E   L E V E L             #
//###############################################

Quantity PriceLevel::get_volume() const {
    return total_quantity;
}

const Order* PriceLevel::get_head() const {
    return head;
}

const Order* PriceLevel::get_tail() const {
    return tail;
}

const PriceLevel* PriceLevel::get_prev() const {
    return prev;
}

const PriceLevel* PriceLevel::get_next() const {
    return next;
}

//###############################################
//#                 T R A D E                   #
//###############################################



//###############################################
//#       T R A D E   R E P O S I T O R Y       #
//###############################################

ErrorCode TradeRepository::add(OrderId _buy_id, UserId _buy_user_id, OrderId _sell_id, UserId _sell_user_id, Quantity _quantity, Price _price_tick) {

    if (next_id > MAX_TRADES - 1) return ErrorCode::max_trade_capacity;

    trades[next_id].buy_id = _buy_id;
    trades[next_id].buy_user_id = _buy_user_id;
    trades[next_id].sell_id = _sell_id;
    trades[next_id].sell_user_id = _sell_user_id;
    trades[next_id].quantity = _quantity;
    trades[next_id].price_tick = _price_tick;

    next_id++;

    return ErrorCode::no_error;
}

const Trade* TradeRepository::get_trade(TradeId _id) const {
    if (_id > MAX_TRADES - 1) return nullptr;

    return &trades[_id];
}

//###############################################
//#            O R D E R   B O O K              #
//###############################################

OrderBook::OrderBook(TradeRepository* _trades) {
    trades = _trades;
}

// ajoute un ordre O(1) amorti quand chaque niveau de prix est actif, retourne un code d'erreur
ErrorCode OrderBook::add(UserId _user_id, OrderId _id, Side _side, Quantity _quantity, Price _price_tick) {

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
    pool[_id].user_id = _user_id;

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
ErrorCode OrderBook::cancel(OrderId _id) {

    //id dépasse le nombre max d'orders
    if (_id > MAX_ORDERS - 1) return ErrorCode::max_order_capacity;

    //aucun ordre à cet id
    if (pool[_id].side == Side::none) return ErrorCode::id_not_used;

    Order* order_ptr = &pool[_id];
    Price price = order_ptr->price_tick;
    Order* order_prev_ptr = order_ptr->prev_in_price_level;
    Order* order_next_ptr = order_ptr->next_in_price_level;

    // mettre à jour la file d'attente dans le price level
    if (order_prev_ptr != nullptr) order_prev_ptr->next_in_price_level = order_next_ptr;
    if (order_next_ptr != nullptr) order_next_ptr->prev_in_price_level = order_prev_ptr;

    //Buy
    if (order_ptr->side == Side::buy) {

        // mettre à jour les volumes totaux du Price Level
        buy_levels[price].total_quantity -= order_ptr->quantity;

        // mettre à jour les Best
        if (buy_levels[price].total_quantity <= 0) {

            PriceLevel* price_level_prev_ptr = buy_levels[price].prev;
            PriceLevel* price_level_next_ptr = buy_levels[price].next;

            //mettre à jour les pointeurs
            if (price_level_prev_ptr != nullptr) price_level_prev_ptr->next = price_level_next_ptr;
            if (price_level_next_ptr != nullptr) price_level_next_ptr->prev = price_level_prev_ptr;

            //mettre à jours les tête et queue
            if (price == static_cast<Price>(buy_levels_head - buy_levels)) buy_levels_head = buy_levels[price].next;
            if (price == static_cast<Price>(buy_levels_tail - buy_levels)) buy_levels_tail = buy_levels[price].prev;
        }

    }

    //Sell
    else if (order_ptr->side == Side::sell) {

        // mettre à jour les volumes totaux du Price Level
        sell_levels[price].total_quantity -= order_ptr->quantity;

        // mettre à jour les Best
        if (sell_levels[price].total_quantity <= 0) {

            PriceLevel* price_level_prev_ptr = sell_levels[price].prev;
            PriceLevel* price_level_next_ptr = sell_levels[price].next;

            //mettre à jour les pointeurs
            if (price_level_prev_ptr != nullptr) price_level_prev_ptr->next = price_level_next_ptr;
            if (price_level_next_ptr != nullptr) price_level_next_ptr->prev = price_level_prev_ptr;

            //mettre à jours les tête et queue
            if (price == static_cast<Price>(sell_levels_head - sell_levels)) sell_levels_head = sell_levels[price].next;
            if (price == static_cast<Price>(sell_levels_tail - sell_levels)) sell_levels_tail = sell_levels[price].prev;
        }
    }

    //Supprimer l'ordre
    pool[_id].price_tick = 0;
    pool[_id].quantity = 0;
    pool[_id].side = Side::none;
    pool[_id].user_id = 0;

    return ErrorCode::no_error;
}

// execute TODO -> ajoute dans le TradeRepository les trades correspondants au macthing du best_bid avec 1 ou plusieurs best_ask
//si le trade repos est nullptr alors le trade est quand même executé mais n'est pas loggé
ErrorCode OrderBook::execute() {

    //Récupérer le best bid price
    PriceLevel* best_bid_price = buy_levels_tail;
    if (best_bid_price == nullptr) return ErrorCode::no_best_bid;

    //Récupérer le best bid first order dans la FIFO
    Order* best_bid = best_bid_price->head;
    if (best_bid == nullptr) return ErrorCode::no_best_bid;  

    Quantity bid_remaining_qty = best_bid->quantity;

    //faire
    do{     

        //Récupérer le best ask Price
        PriceLevel* best_ask_price = sell_levels_head;
        if (best_ask_price == nullptr) return ErrorCode::no_best_ask;

        //Récupérer le best ask first order dans la FIFO
        Order* best_ask = best_ask_price->head;
        if (best_ask == nullptr) return ErrorCode::no_best_ask;

        //std::cout << "best bid = " << best_bid->price_tick << std::endl;
        //std::cout << "best ask = " << best_ask->price_tick << std::endl;
     
        // si best bid >= best ask alors il y a un match
        if (best_bid->price_tick >= best_ask->price_tick) {
            //  si bid qty >= ask qty
            if (best_bid->quantity >= best_ask->quantity){
                //si le trade repo existe alors on log le trade (qty = celle du ask, prix du ask)      
                if (trades != nullptr) {
                    ErrorCode err = trades->add(static_cast<OrderId>(best_bid - pool),
                                best_bid->user_id,
                                static_cast<OrderId>(best_ask - pool),
                                best_ask->user_id,
                                best_ask->quantity,

                                best_ask->price_tick);
                    if (err != ErrorCode::no_error) return err;
                }

                Quantity ask_qty = best_ask->quantity;

                //cancel le best ask
                cancel(static_cast<OrderId>(best_ask - pool));

                //soustraire la qty du ask au best bid
                bid_remaining_qty -= ask_qty;                
                best_bid->quantity -= ask_qty;
                best_bid_price->total_quantity -= ask_qty;

                // si qty du bid == 0 on cancel le best bid 
                if (bid_remaining_qty == 0) cancel(static_cast<OrderId>(best_bid - pool));
            
            }
        
            // sinon ça veut dire que bid qty < ask qty
            else{               

                //si le trade repo existe alors on log le trade (qty = celle du bid, prix du ask)
                if (trades != nullptr) {
                    ErrorCode err = trades->add(static_cast<OrderId>(best_bid - pool),
                        best_bid->user_id,
                        static_cast<OrderId>(best_ask - pool),
                        best_ask->user_id,
                        best_bid->quantity,
                        best_ask->price_tick);
                
                    if (err != ErrorCode::no_error) return err;
                }
            
                //cancel le best bid
                //std::cout << "test" << std::endl;
                cancel(static_cast<OrderId>(best_bid - pool));
                
                //Soustraire la qty du bid au best ask
                best_ask->quantity -= bid_remaining_qty;
                best_ask_price->total_quantity -= bid_remaining_qty;                

                //mettre qty bid à 0
                bid_remaining_qty = 0;
            }
            
        }
        // sinon c'est qu'il n'y a plus de match possible
        else return ErrorCode::no_match;

    //on recommence tant que la qty restante du best buy > 0
    } while (bid_remaining_qty > 0);    

    return ErrorCode::no_error;
}

// best bid O(1)
//Todo retourner une erreur ou autre quand il n'y a pas de best (si vide)
Price OrderBook::best_bid() const {
    if (buy_levels_tail == nullptr) return 0;
    return static_cast<Price>(buy_levels_tail - buy_levels);
}

Price OrderBook::worst_bid() const {
    if (buy_levels_head == nullptr) return 0;
    return static_cast<Price>(buy_levels_head - buy_levels);
}

// best ask O(1)
//Todo retourner une erreur ou autre quand il n'y a pas de best (si vide)
Price OrderBook::best_ask() const {
    if (sell_levels_head == nullptr) return 0;
    return static_cast<Price>(sell_levels_head - sell_levels);
}

Price OrderBook::worst_ask() const {
    if (sell_levels_tail == nullptr) return 0;
    return static_cast<Price>(sell_levels_tail - sell_levels);
}

Price OrderBook::get_price(OrderId _id) const {
    if (_id > MAX_ORDERS - 1) return 0;
    return pool[_id].price_tick;
}

Quantity OrderBook::get_quantity(OrderId _id) const {
    if (_id > MAX_ORDERS - 1) return 0;
    return pool[_id].quantity;
}

Side OrderBook::get_side(OrderId _id) const {
    if (_id > MAX_ORDERS - 1) return Side::none;
    return pool[_id].side;
}

UserId OrderBook::get_user_id(OrderId _id) const {
    if (_id > MAX_ORDERS - 1) return 0;
    return pool[_id].user_id;
}

const Order* OrderBook::get_order(OrderId _id) const {
    if (_id > MAX_ORDERS - 1) return nullptr;

    return &pool[_id];
}

const PriceLevel* OrderBook::get_sell_price_level(Price _price) const {
    if (_price > MAX_PRICE_LEVEL) return nullptr;

    return &sell_levels[_price];
}

const PriceLevel* OrderBook::get_buy_price_level(Price _price) const {
    if (_price > MAX_PRICE_LEVEL) return nullptr;

    return &buy_levels[_price];
}

const PriceLevel* OrderBook::get_sell_price_levels_tail() const {
    return sell_levels_tail;
}

const PriceLevel* OrderBook::get_buy_price_levels_tail() const {
    return buy_levels_tail;
}

const PriceLevel* OrderBook::get_sell_price_levels_head() const {
    return sell_levels_head;
}

const PriceLevel* OrderBook::get_buy_price_levels_head() const {
    return buy_levels_head;
}

//SELL ADD : On met à jour le PriceLevel et son Volume
inline void OrderBook::update_pricelevel_when_adding_sell_order(OrderId _id, Price _price_tick, Quantity _quantity) {

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
inline void OrderBook::update_best_ask_when_adding_sell_order(Price _price_tick) {

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
inline void OrderBook::update_pricelevel_when_adding_buy_order(OrderId _id, Price _price_tick, Quantity _quantity) {

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
inline void OrderBook::update_best_bid_when_adding_buy_order(Price _price_tick) {

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
    if (_price_tick < old_worst_bid) {

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

// Affichage d'un ordre au format "[Side Quantity @ PriceTick EUR]"
std::ostream& operator<<(std::ostream& os, const Order& o) {
    if (o.get_side() == Side::none) {
        os << "---";
        return os;
    }

    const char* side_str = "NONE";
    switch (o.get_side()) {
    case Side::buy:  side_str = "BUY";  break;
    case Side::sell: side_str = "SELL"; break;
    default:         side_str = "NONE"; break;
    }

    os << '[' << side_str << ' ' << o.get_quantity() << " @ " << o.get_price_tick() << " EUR]";
    return os;
}

// Affichage d'un PriceLevel au format "[Volume=vol]"
std::ostream& operator<<(std::ostream& os, const PriceLevel& pl) {
    if (pl.get_volume() == 0) {
        os << "---";
        return os;
    }

    os << "[Volume=" << pl.get_volume() << "]";
    return os;
}

// Affiche la liste chaînée des ordres d'un PriceLevel de la tête vers la queue
// Format : HEAD <- [BUY qty @ price EUR] <- [BUY qty @ price EUR] <- TAIL
void print_pricelevel_orders(std::ostream& os, const PriceLevel* pl) {
    if (pl == nullptr) return;
    // n'affiche que si actif et non vide
    if (pl->get_head() == nullptr) return;

    os << "HEAD <- ";
    const Order* cur = pl->get_head();
    while (cur != nullptr) {
        os << *cur; // utilise l'opérateur<< déjà défini pour Order
        if (cur->get_next_in_price_level() != nullptr) os << " <- ";
        cur = cur->get_next_in_price_level();
    }
    os << " <- TAIL";
}

// Affichage de l'OrderBook : 3 colonnes côte à côte : pool | buy levels | sell levels
std::ostream& operator<<(std::ostream& os, const OrderBook& ob) {
    // on fixe la taille des colonnes
    constexpr size_t COL_WIDTH = 22;

    //on calcule le nombre de lignes max
    size_t pool_rows = 1 + MAX_ORDERS; // header + taille de la pool
    size_t price_rows = 1 + (MAX_PRICE_LEVEL + 1); // header + nombre de prix max

    size_t max_rows = std::max(pool_rows, price_rows);

    os << "#################################################################" << std::endl;
    os << "#                      O R D E R   B O O K                      #" << std::endl;
    os << "#################################################################" << std::endl;

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
            ss << idx << ": " << *ob.get_order(idx);
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
                ss << p << ": " << *ob.get_buy_price_level(p);
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
                ss << p << ": " << *ob.get_sell_price_level(p);
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

    // Ligne finale avec Best bid / Best ask alignée sous les colonnes
    os << std::endl;

    std::string best_bid_str = "Best bid = " + std::to_string(ob.best_bid());
    std::string best_ask_str = "Best ask = " + std::to_string(ob.best_ask());

    // imprimer colonne Pool vide, puis Best bid et Best ask
    os << std::left << std::setw(static_cast<int>(COL_WIDTH)) << std::string()
        << std::left << std::setw(static_cast<int>(COL_WIDTH)) << best_bid_str
        << std::left << std::setw(static_cast<int>(COL_WIDTH)) << best_ask_str;

    return os;
}

// Affichage d'un Trade au format "<buy_user_id> bought <qty>lots @ <price_ticks> EUR to <sell_user_id>"
std::ostream& operator<<(std::ostream& os, const Trade& t) {
    os << t.get_buy_user_id() << " bought " << t.get_quantity() << " lots @ " << t.get_price_tick() << " EUR to " << t.get_sell_user_id();
    return os;
}

// Affichage d'un TradeRepository
std::ostream& operator<<(std::ostream& os, const TradeRepository& tr) {
    os << "-- [ TRADE REPOSITORY ] --" << std::endl;
    for (TradeId i = 0; i < MAX_TRADES; ++i) {
        os << i << ": ";
        const Trade* t = tr.get_trade(i);
        if (t == nullptr || t->get_quantity() == 0) os << "---";
        else os << *t;
        if (i + 1 < MAX_TRADES) os << std::endl;
    }
    return os;
}

