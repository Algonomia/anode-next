# Liste des Tests de Benchmark

Document de référence listant tous les tests du benchmark avec leurs noms de clés.

## 📊 Vue d'ensemble

**Total:** 21 tests répartis en 7 catégories

## 🗂️ Tests par Catégorie

### IO (1 test)
| Clé | Description | Input | Output |
|-----|-------------|-------|--------|
| `Load CSV (10,000 rows)` | Chargement CSV de 10k lignes | 0 | 10000 |

### Filter (4 tests)
| Clé | Description | Input | Output typique |
|-----|-------------|-------|----------------|
| `filter_equality_simple` | Filtre simple égalité (Country == 'Norway') | 10000 | ~51 |
| `filter_contains` | Filtre avec contains (City contains 'Lake') | 10000 | ~750 |
| `filter_multi_condition` | Filtre multi-critères (Country + City) | 10000 | ~5 |
| `filter_email_domain` | Filtre domaine email (Email contains '@espinoza') | 10000 | ~6 |

### OrderBy (4 tests)
| Clé | Description | Input | Output |
|-----|-------------|-------|--------|
| `orderby_single_asc` | Tri simple ASC (Country) | 10000 | 10000 |
| `orderby_single_desc` | Tri simple DESC (City) | 10000 | 10000 |
| `orderby_multi_2cols` | Tri 2 colonnes (Country ASC, City ASC) | 10000 | 10000 |
| `orderby_multi_3cols` | Tri 3 colonnes (Country, Last Name, First Name) | 10000 | 10000 |

### GroupBy (4 tests)
| Clé | Description | Input | Output typique |
|-----|-------------|-------|----------------|
| `groupby_count_country` | Groupe par pays avec count | 10000 | ~243 |
| `groupby_count_city` | Groupe par ville avec count | 10000 | ~8910 |
| `groupby_multi_country_city` | Groupe par pays+ville avec count | 10000 | ~9995 |
| `groupby_count_company` | Groupe par compagnie avec count | 10000 | ~9215 |

### Select (2 tests)
| Clé | Description | Input | Output |
|-----|-------------|-------|--------|
| `select_3cols` | Sélection 3 colonnes (First Name, Last Name, Email) | 10000 | 10000 |
| `select_6cols` | Sélection 6 colonnes | 10000 | 10000 |

### Chain (3 tests)
| Clé | Description | Input | Output typique |
|-----|-------------|-------|----------------|
| `chain_filter_orderby` | Filter (Norway) + OrderBy (City) | 10000 | ~51 |
| `chain_filter_orderby_select` | Filter + OrderBy + Select | 10000 | ~51 |
| `chain_filter_groupby_orderby` | Filter (Lake) + GroupBy + OrderBy | 10000 | ~236 |

### Export (3 tests)
| Clé | Description | Input | Output |
|-----|-------------|-------|--------|
| `export_json_sample` | Export échantillon JSON (Norway) | 51 | 0 |
| `export_csv_full` | Export CSV complet | 10000 | 0 |
| `export_csv_filtered` | Export CSV filtré (Norway) | 51 | 0 |

## 📈 Performances Typiques (10k lignes)

### Par Catégorie
```
IO:      ~259ms  (1 opération)
Filter:  ~9ms    (4 opérations, moyenne: 2ms)
OrderBy: ~246ms  (4 opérations, moyenne: 61ms)
GroupBy: ~132ms  (4 opérations, moyenne: 33ms)
Select:  <1ms    (2 opérations, moyenne: 0ms)
Chain:   ~9ms    (3 opérations, moyenne: 3ms)
Export:  ~43ms   (3 opérations, moyenne: 14ms)
```

### Top 5 Opérations les Plus Rapides
1. `select_3cols` : <1ms
2. `select_6cols` : <1ms
3. `filter_multi_condition` : ~1ms
4. `filter_email_domain` : ~1ms
5. `chain_filter_orderby_select` : ~1ms

### Top 5 Opérations les Plus Lentes
1. `Load CSV (10,000 rows)` : ~259ms
2. `orderby_multi_3cols` : ~65ms
3. `orderby_multi_2cols` : ~64ms
4. `orderby_single_desc` : ~63ms
5. `orderby_single_asc` : ~54ms

## 🔍 Utilisation pour Comparaison

### Exemple de requête jq pour analyser

```bash
# Afficher tous les tests d'une catégorie
jq '.results[] | select(.category == "Filter")' benchmark_report.json

# Trouver les tests > 50ms
jq '.results[] | select(.duration_ms > 50)' benchmark_report.json

# Comparer deux versions
jq -s '.[0].results as $v1 | .[1].results as $v2 |
  $v1 | map(. as $r1 |
    ($v2[] | select(.operation == $r1.operation)) as $r2 |
    {operation: $r1.operation, v1: $r1.duration_ms, v2: $r2.duration_ms, diff: ($r2.duration_ms - $r1.duration_ms)}
  )' v1.0.0.json v1.1.0.json
```

## 🎯 Convention de Nommage

Les noms de clés suivent le pattern : `{categorie}_{operation}_{details}`

Exemples :
- `filter_equality_simple` → categorie=filter, operation=equality, details=simple
- `orderby_multi_2cols` → categorie=orderby, operation=multi, details=2cols
- `groupby_count_country` → categorie=groupby, operation=count, details=country

## 📝 Ajouter un Nouveau Test

Pour ajouter un nouveau test dans `benchmark.cpp` :

```cpp
{
    BenchmarkTimer timer(
        "mon_nouveau_test",        // Nom de clé unique
        &reporter,                  // Reporter
        "MaCategorie",             // Catégorie
        datasetRows                // Input rows
    );

    auto result = monOperation();
    timer.setOutputRows(result->rowCount());  // Output rows
}
```

## 🔄 Historique des Modifications

- **v1.0.0** (2025-11-18) : 21 tests initiaux
  - IO: 1 test
  - Filter: 4 tests
  - OrderBy: 4 tests
  - GroupBy: 4 tests
  - Select: 2 tests
  - Chain: 3 tests
  - Export: 3 tests