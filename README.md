# Préambule
Ce projet implémente un Order Book ainsi qu'un Matching Engine simples de ma conception. Il est optimisé pour la rapidité d'execution.

# Descritpion 
Un carnet d'ordres (Order Book) stocke un ensemble d'ordres qui sont décrits par un id unique, un side (Buy ou Sell), un prix en Ticks, une quantité et un userid. Ce carnet d'ordre est pensé pour être utilisé côté marché.

# Cahier des charges
Notre carnet d'ordres doit respecter les contraintes suivantes : 
- Accès à un ordre par son id en O(1) -> DONE
- Accès au Best Bid en O(1) -> DONE
- Accès au Best Ask en O(1) -> DONE
- Ajout d'un ordre en O(1) amorti -> je dois encore calculer la complexité mais elle est < à O(N) pour sûr. Dans le meilleur cas, on est en O(1) et dans certains cas il faut faire un petit parcours d'un sous ensemble de N que je dois quantifier.
- Cancel d'un ordre en O(1) -> DONE
- Execute via un matching Engine en O(1) -> DONE
- Accès au volume total par niveau de prix en O(1) -> DONE

# Matching Engine
TODO

# Architecture
La particularité de cette architecture est que nous indexons les ordres dans 2 tableaux triés par prix séparés (1 pour les Buy et 1 pour les Sell). Celà nous permet de savoir de manière instantannée quel est le meilleur Bid et le meilleur Ask. 
Afin de ne pas avoir à faire d'allocations dynamiques dans le Hot Path, nous reservons donc d'avance deux grands tableaux contigus qui stockent les niveaux de prix actifs Buy et Sell. Chaque case de ces tableaux décrivent donc un niveau de prix.

Dans chaque niveau de prix, nous allons donc ranger les ordres, de manière chainée par priorité d'ordre d'arrivée. Ainsi, si j'ai un ordre **BUY 20 @ 10 EUR** puis **BUY 10 @ 10 EUR**, mon Price Level à 10 € ressemblera à ça :

[BUY PriceLevel 10: Volume = 30] -> [20 @ 10 EUR] <-> [10 @ 10 EUR]

Afin de pouvoir accéder au best bid et best ask en O(1) nous devons également chainer les PriceLevel entre eux. En effet, tous les niveaux de prix ne sont pas utilisés forcément, il peut donc y avoir des trous dans le tableau des niveaux de prix. L'OrderBook garde donc en permanance l'index du premier niveau de prix actif ainsi que le dernier, et chaque prix actif du tableau est chainé à son prochain prix actif. 

En ce qui concerne les ordres eux même, ils sont stockés dans une pool allouée à l'avance et indexés par Id d'ordre pour garantir un accès O(1). Chaque niveau de prix contient donc un pointeur vers la pool, qui mène donc à l'ordre de ce même niveau de prix qui est arrivé en premier. Puis cet ordre contient un pointeur sur l'ordre du même prix arrivé en second etc.

Ce schéma garanti que toutes les opérations puissent être faites en O(1).

En revanche, il existe un exception : l'ajout. En effet, dans certains cas, lors d"un ajout, il nous faut effectuer un parcours d'un sous ensemble de N. Je détaille le calcul de la complexité plus bas.

# Roadmap
Prochaines étapes : 
- testing approfondi de Add et Cancel avec des jeux de test
- quantifier la complexité de Add
- OrderBook::get_volume(Price, Side);

# Analyse des complexités
Les opérations d'accès par id, d'accès aux Bests, de cancel et d'execution sont en O(1).

## Complexité algorithmique du ADD
En revanche, comme vu plus haut, la question se pose pour l'ajout d'un ordre.

En temps moyen / amorti, l'ajout est en O(1). En revanche, il existe certains cas où l'ajout se fait en O(P), P étant le nombre maximal de niveaux de prix.

- insertion dans la pool : O(1)
- chaînage de l'ordre dans son PriceLevel (ajout en queue) : O(1)
- mise à jour des volumes : O(1)
- mettre à jour le chainage des PriceLevels entre eux. Pire cas : O(P) où P = nombre de niveaux de prix (MAX_PRICE_LEVEL + 1), en moyenne O(1).

Quand on insère un nouvel PriceLevel il faut mettre à jour la liste chaînée des niveaux de prix (et mettre à jour les Best). Dans le cas précis ou un nouveau niveau de prix non actif doit devenir actif, et que ce dernier se retrouve borné par d'autres prix actifs, on doit donc parcourir les price levels actifs pour reconnecter les pointeurs — coût proportionnel au nombre de niveaux actifs parcourus. Si on traite P comme une variable, c’est O(P).

Si le nouveau niveau de prix pas encore actif est < au plus petit ou > au plus grand, alors O(1). Si l'ordre qui arrive tombe sur un niveau de prix déjà actif, ce qui arrive souvent, alors O(1).

## Taille en mémoire vive

**Pour l'OrderBook:**
Ordre = 48 octets
PriceLevel = 40 octets
OrderBook (sans les tableaux contenant les ordres et les niveaux de prix) = 40 octets

Si l'on considère : 
- N le nombre maximal d'ordres
- P le nombre maximal de niveaux de prix en ticks

La taille en RAM de notre architecture est alors de **48X + 80P + 40 octets**.

Soit par exemple pour **1 milliard d'ordres** par jours et **1 million de niveaux de prix**, il nous faudrait alors : 48 080 000 040 octets soit **48 Go de RAM**.
