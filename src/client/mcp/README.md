# AnodeServer MCP - Guide pour Claude

Ce serveur MCP te permet d'interagir avec AnodeServer, un moteur de graphs de traitement de données. Tu peux créer des graphs visuels, connecter des noeuds, et exécuter des pipelines de transformation de données.

## Concept Global

AnodeServer est un **éditeur de graphs de flux de données** (dataflow graph editor). Les données circulent des noeuds sources (entry points) vers les noeuds de traitement, puis vers les sorties.

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ int_value   │────►│    add      │────►│   result    │
│   (42)      │     │             │     │    (52)     │
└─────────────┘     │             │     └─────────────┘
                    │             │
┌─────────────┐     │             │
│ int_value   │────►│             │
│   (10)      │     └─────────────┘
└─────────────┘
```

### Types de Données

| Type | Description | Exemple |
|------|-------------|---------|
| `int` | Entier | `42` |
| `double` | Nombre décimal | `3.14` |
| `string` | Chaîne de caractères | `"hello"` |
| `bool` | Booléen | `true` |
| `csv` | DataFrame (tableau de données) | Table avec colonnes et lignes |
| `field` | Référence à une colonne | Pointe vers une colonne d'un CSV |

### Broadcasting (Diffusion)

Quand un **scalaire** est utilisé avec un **CSV**, il est automatiquement appliqué à toutes les lignes :

```
csv_source (prix: [100, 200, 300])
          +
     int_value(50)
          =
   résultat: [150, 250, 350]
```

---

## Noeuds Disponibles

### Scalaires (Entry Points)

Ces noeuds n'ont pas d'entrées, ils produisent des valeurs :

| Type | Propriété | Sortie | Usage |
|------|-----------|--------|-------|
| `scalar/int_value` | `_value: int` | `value` | Entier |
| `scalar/double_value` | `_value: double` | `value` | Décimal |
| `scalar/string_value` | `_value: string` | `value` | Texte |
| `scalar/bool_value` | `_value: bool` | `value` | Booléen |
| `scalar/null_value` | - | `value` | Null |

### Source de Données

| Type | Sortie | Description |
|------|--------|-------------|
| `data/csv_source` | `csv` | Retourne un DataFrame de test (4 lignes: id, name, price) |

### Sélection de Colonnes

| Type | Entrées | Sorties | Propriété |
|------|---------|---------|-----------|
| `csv/field` | `csv` | `field`, `csv` | `_column: string` |

Le noeud `field` sélectionne une colonne par son nom. La sortie `field` est une référence à cette colonne, et `csv` passe le DataFrame au noeud suivant.

### Mathématiques

Tous les noeuds math acceptent des scalaires OU des fields (colonnes) :

| Type | Entrées | Sorties | Opération |
|------|---------|---------|-----------|
| `math/add` | `src`, `operand`, `csv`?, `dest`? | `result`, `csv` | Addition |
| `math/subtract` | `src`, `operand`, `csv`?, `dest`? | `result`, `csv` | Soustraction |
| `math/multiply` | `src`, `operand`, `csv`?, `dest`? | `result`, `csv` | Multiplication |
| `math/divide` | `src`, `operand`, `csv`?, `dest`? | `result`, `csv` | Division |
| `math/modulus` | `src`, `operand`, `csv`?, `dest`? | `result`, `csv` | Modulo |

**Entrées détaillées :**
- `src` : Valeur source (int, double, ou field)
- `operand` : Second opérande (int, double, ou field)
- `csv` (optionnel) : Contexte CSV pour les opérations sur colonnes
- `dest` (optionnel) : Nom de la colonne destination

### Agrégation

| Type | Entrées | Sorties | Propriété |
|------|---------|---------|-----------|
| `aggregate/group` | `csv`, `field`, `field_1`...`field_99` | `csv` | `_aggregation` |

**Valeurs de `_aggregation`** : `"sum"`, `"avg"`, `"min"`, `"max"`, `"first"`, `"count"`

---

## Outils MCP Disponibles

### Catalogue

| Outil | Description |
|-------|-------------|
| `list_nodes` | Liste tous les types de noeuds par catégorie |
| `get_node_info` | Détails d'un type (entrées, sorties, propriétés) |

### Gestion des Graphs

| Outil | Description |
|-------|-------------|
| `list_graphs` | Liste les graphs sauvegardés |
| `get_graph` | Charge un graph (remplace le graph courant) |
| `create_graph` | Crée un nouveau graph dans la base |
| `save_graph` | Sauvegarde le graph courant |
| `delete_graph` | Supprime un graph |
| `new_graph` | Vide le graph courant (nouveau graph vierge) |

### Édition du Graph

| Outil | Description |
|-------|-------------|
| `add_node` | Ajoute un noeud au graph |
| `remove_node` | Supprime un noeud (et ses connexions) |
| `connect_nodes` | Connecte deux noeuds (port sortie → port entrée) |
| `disconnect_nodes` | Déconnecte deux noeuds |
| `set_property` | Modifie une propriété d'un noeud |
| `get_node` | Récupère les détails d'un noeud |
| `get_current_graph` | Affiche l'état complet du graph |
| `list_connections` | Liste toutes les connexions |

### Exécution

| Outil | Description |
|-------|-------------|
| `execute_graph` | Exécute le graph et retourne les résultats |
| `query_dataframe` | Requête sur un DataFrame résultat (pagination) |

---

## Workflow Typique

### 1. Créer un graph simple (addition de deux entiers)

```
1. new_graph()                           # Partir d'un graph vierge
2. add_node("scalar/int_value", properties: {_value: 5})  → node_1
3. add_node("scalar/int_value", properties: {_value: 3})  → node_2
4. add_node("math/add")                                   → node_3
5. connect_nodes("node_1", "value", "node_3", "src")
6. connect_nodes("node_2", "value", "node_3", "operand")
7. create_graph(slug: "addition-test", name: "Test Addition")
8. execute_graph()                       # Résultat: 8
```

### 2. Manipuler un CSV

```
1. new_graph()
2. add_node("data/csv_source")                            → node_1
3. add_node("csv/field", properties: {_column: "price"})  → node_2
4. add_node("scalar/int_value", properties: {_value: 10}) → node_3
5. add_node("math/multiply")                              → node_4

6. connect_nodes("node_1", "csv", "node_2", "csv")        # CSV → field
7. connect_nodes("node_2", "field", "node_4", "src")      # Colonne price
8. connect_nodes("node_2", "csv", "node_4", "csv")        # Contexte CSV
9. connect_nodes("node_3", "value", "node_4", "operand")  # Multiplicateur

10. create_graph(slug: "prix-x10", name: "Prix multiplié par 10")
11. execute_graph()
12. query_dataframe(node_id: "node_4", port: "csv")       # Voir le résultat
```

### 3. Charger et modifier un graph existant

```
1. list_graphs()                         # Voir les graphs disponibles
2. get_graph(slug: "mon-graph")          # Charger
3. add_node("scalar/int_value", properties: {_value: 100})
4. connect_nodes(...)                    # Modifier
5. save_graph()                          # Sauvegarder les changements
6. execute_graph()                       # Tester
```

---

## Structure des Données

### Noeud (AnodeNode)

```json
{
  "id": "node_1",
  "type": "scalar/int_value",
  "properties": {
    "_value": { "value": 42, "type": "int" }
  },
  "position": [100, 200]
}
```

### Connexion (AnodeConnection)

```json
{
  "from": "node_1",
  "fromPort": "value",
  "to": "node_2",
  "toPort": "src"
}
```

### Graph Complet (AnodeGraph)

```json
{
  "nodes": [...],
  "connections": [...]
}
```

---

## Conventions de Nommage

### Ports Communs

| Port | Direction | Types | Usage |
|------|-----------|-------|-------|
| `value` | Sortie | scalaires | Valeur produite par les noeuds scalaires |
| `csv` | Entrée/Sortie | csv | DataFrame passé entre noeuds |
| `field` | Sortie | field | Référence à une colonne |
| `src` | Entrée | int, double, field | Valeur source pour opérations |
| `operand` | Entrée | int, double, field | Second opérande |
| `result` | Sortie | double | Résultat scalaire |
| `output` | Sortie | variable | Sortie générique |

### Propriétés

- Les propriétés commencent par `_` (ex: `_value`, `_column`, `_aggregation`)
- Utilise `set_property(node_id, "_value", 42)` pour modifier

---

## Bonnes Pratiques

1. **Toujours commencer par `list_nodes`** pour connaître les noeuds disponibles
2. **Utiliser `get_node_info`** pour comprendre les entrées/sorties d'un noeud
3. **Créer le graph pas à pas** : noeuds d'abord, connexions ensuite
4. **Sauvegarder avant d'exécuter** : `create_graph` ou `save_graph`
5. **Utiliser `get_current_graph`** pour vérifier l'état avant exécution
6. **Pour les CSV** : utiliser `query_dataframe` pour voir les données

---

## Exemples de Graphs

### Addition Simple
```
int_value(5) ──────► add ──► result: 8
                      ▲
int_value(3) ────────┘
```

### Prix avec TVA (20%)
```
csv_source ──► field("price") ──► multiply ──► csv avec prix TTC
                                      ▲
              double_value(1.2) ─────┘
```

### Chaînage d'Opérations Mathématiques sur CSV

Quand tu chaînes plusieurs opérations math (ex: multiplier puis additionner), voici les règles importantes :

**Règle 1 : Le port `csv` transporte le DataFrame transformé**
- Chaque nœud math a une entrée `csv` et une sortie `csv`
- Pour chaîner, connecte `sortie csv` → `entrée csv` du nœud suivant

**Règle 2 : Le port `src` reçoit toujours le `field` (colonne)**
- Le `src` indique QUELLE colonne transformer
- Il faut connecter le même `field` à chaque nœud math de la chaîne
- NE PAS utiliser la sortie `result` (c'est un scalaire, pas applicable aux DataFrames)

**Règle 3 : La sortie `result` est un scalaire**
- `result` retourne une valeur unique (double), utile pour des calculs scalaires
- Pour les opérations sur DataFrame, utilise la sortie `csv`

**Exemple : (prix × 5) + 50**
```
csv_source ─────────────────────────────────► field("price")
                                                   │
              ┌────────────────────────────────────┤ (field)
              │                                    │
              │                     ┌──────────────┘
              ▼                     ▼
         multiply ◄──────────── field
              │ csv                 │
              │                     │
              ▼                     ▼
            add ◄───────────── field (même connexion!)
              │
              ▼
         csv résultat
```

**Connexions détaillées :**
```
1. csv_source.csv      → multiply.csv      # DataFrame source
2. field.field         → multiply.src      # Colonne "price" à multiplier
3. int_value(5).value  → multiply.operand  # Multiplicateur

4. multiply.csv        → add.csv           # DataFrame après multiplication
5. field.field         → add.src           # Même colonne "price" pour l'addition
6. int_value(50).value → add.operand       # Valeur à ajouter
```

**⚠️ Erreur courante** : Connecter `multiply.result` → `add.src`
- `result` est un double scalaire, pas une référence de colonne
- Le nœud `add` ne saura pas sur quelle colonne opérer

### Grouper par Catégorie
```
csv_source ──────────────────► group ──► csv groupé (somme par catégorie)
                                  ▲
field("category") ───────────────┘
```

---

## Résolution de Problèmes

### "Node not found"
→ Vérifie l'ID du noeud avec `get_current_graph`

### "Connection already exists"
→ Utilise `disconnect_nodes` avant de reconnecter

### "Input port already connected"
→ Les entrées n'acceptent qu'une seule connexion. Déconnecte d'abord.

### Résultat CSV vide
→ Vérifie que le contexte `csv` est bien connecté aux noeuds math

### Erreur d'exécution
→ Vérifie que tous les ports requis sont connectés avec `get_node_info`
