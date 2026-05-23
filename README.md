# Préambule
Ce projet implémente un Order Book ainsi qu'un Matching Engine simples de ma conception. Il est optimisé pour la rapidité d'execution.

# Descritpion 
Un carnet d'ordres (Order Book) stocke un ensemble d'ordres qui sont décrits par un id unique, un side (Buy ou Sell), un prix en Ticks, une quantité et un userid. Ce carnet d'ordre est pensé pour être utilisé côté marché.

# Cahier des charges
Notre carnet d'ordres doit respecter les contraintes suivantes : 
- Accès à un ordre par son id en O(1)
- Accès au Best Bid en O(1)
- Accès au Best Ask en O(1)
- Ajout d'un ordre en O(1) amorti (possible ?)
- Cancel d'un ordre en O(1)
- Execute via un matching Engine en O(1)

# Résultats à date
- Accès au Best Bid en O(1) -> DONE
- Accès au Best Ask en O(1) -> DONE
- Ajout d'un ordre en O(1) amorti -> je dois encore calculer la complexité mais elle est < à O(N) pour sûr. Dans le meilleur cas, on est en O(1) et dans certains cas il faut faire un petit parcours d'un sous ensemble de N que je dois quantifier.
- Cancel d'un ordre en O(1) -> DONE
- Execute en O(1) -> DONE

# Roadmap
Prochaines étapes : 
- testing approfondi de Add et Cancel avec des jeux de test
- quantifier les complexités de Add et Cancel
- OrderBook::get_volume(Price, Side);

# Analyse des complexités
## Complexité algorithmique

TODO

## Taille en mémoire vive

TODO
