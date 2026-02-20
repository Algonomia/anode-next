# Benchmark System - Documentation

Système de benchmarking pour DataFrame avec rapports JSON comparables entre versions.

## 📋 Vue d'ensemble

Le système de benchmark comprend :
- **BenchmarkReporter** : Classe pour enregistrer et sauvegarder les résultats
- **benchmark** : Programme de test complet sur 10k lignes
- **compare_benchmarks** : Outil de comparaison entre versions

## 🚀 Utilisation Rapide

### Exécuter un benchmark

```bash
cd build
./benchmark
```

Cela génère :
- Affichage console avec temps d'exécution
- `../examples/benchmark_report.json` - Rapport détaillé

### Format du rapport JSON

```json
{
  "version": "1.0.0",
  "timestamp": "2025-11-18 15:53:40",
  "dataset": {
    "rows": 10000,
    "columns": 12,
    "filename": "customers-10000.csv"
  },
  "system": {
    "os": "Linux",
    "compiler": "g++ 13.3.0"
  },
  "results": [
    {
      "category": "IO",
      "operation": "Load CSV (10,000 rows)",
      "duration_ms": 256,
      "input_rows": 0,
      "output_rows": 10000,
      "details": ""
    }
  ],
  "statistics": {
    "total_duration_ms": 256,
    "total_operations": 1,
    "by_category": {
      "IO": {
        "total_ms": 256,
        "average_ms": 256,
        "min_ms": 256,
        "max_ms": 256,
        "count": 1
      }
    }
  }
}
```

## 📊 Comparer les Benchmarks

### 1. Sauvegarder la baseline

```bash
# Version 1.0.0
cd build
./benchmark
cp ../examples/benchmark_report.json ../examples/benchmark_v1.0.0.json
```

### 2. Faire des modifications au code

```cpp
// Optimisez votre code...
// Par exemple, améliorer l'algorithme de tri
```

### 3. Re-benchmarker

```bash
# Version 1.1.0
# Modifier la version dans benchmark.cpp : BenchmarkReporter reporter("1.1.0");
make benchmark
./benchmark
cp ../examples/benchmark_report.json ../examples/benchmark_v1.1.0.json
```

### 4. Comparer les résultats

```bash
./compare_benchmarks \
    ../examples/benchmark_v1.0.0.json \
    ../examples/benchmark_v1.1.0.json \
    ../examples/comparison.json
```

**Sortie exemple :**
```
Comparing benchmarks...
  Baseline: ../examples/benchmark_v1.0.0.json
  Current:  ../examples/benchmark_v1.1.0.json

=== Benchmark Comparison ===
Baseline: 1.0.0 (2025-11-18 15:53:40)
Current:  1.1.0 (2025-11-18 16:10:25)

Results:
  Improved:  12
  Regressed: 2
  Stable:    8

Comparison saved to: ../examples/comparison.json
```

### Format du rapport de comparaison

```json
{
  "baseline": {
    "file": "benchmark_v1.0.0.json",
    "version": "1.0.0",
    "timestamp": "2025-11-18 15:53:40"
  },
  "current": {
    "file": "benchmark_v1.1.0.json",
    "version": "1.1.0",
    "timestamp": "2025-11-18 16:10:25"
  },
  "summary": {
    "total_operations": 22,
    "improved": 12,
    "regressed": 2,
    "stable": 8
  },
  "comparisons": [
    {
      "category": "Filter",
      "operation": "Filter: Country == 'Norway'",
      "baseline_ms": 5,
      "current_ms": 3,
      "diff_ms": -2,
      "percent_change": -40.0,
      "status": "improved"
    },
    {
      "category": "OrderBy",
      "operation": "OrderBy: Country ASC",
      "baseline_ms": 50,
      "current_ms": 55,
      "diff_ms": 5,
      "percent_change": 10.0,
      "status": "regressed"
    }
  ]
}
```

## 🔧 Intégration dans votre code

### Utilisation basique

```cpp
#include "BenchmarkReporter.hpp"

int main() {
    BenchmarkReporter reporter("1.0.0");
    reporter.setSystemInfo("Linux", "g++ 13.3.0");
    reporter.setDatasetInfo(10000, 12, "customers.csv");

    // Mesurer une opération
    {
        auto start = std::chrono::high_resolution_clock::now();

        // Votre code ici
        auto df = DataFrameIO::readCSV("data.csv");

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        reporter.addResult(
            "IO",                     // Catégorie
            "Load CSV",               // Opération
            duration,                 // Durée en ms
            0,                        // Lignes en entrée
            df->rowCount()            // Lignes en sortie
        );
    }

    // Sauvegarder le rapport
    reporter.saveToFile("benchmark.json");
    reporter.printSummary();

    return 0;
}
```

### Utilisation avec ScopedBenchmark (RAII)

```cpp
#include "BenchmarkReporter.hpp"

int main() {
    BenchmarkReporter reporter("1.0.0");

    // Mesure automatique avec RAII
    {
        ScopedBenchmark bench(reporter, "Filter", "Country filter", 10000);
        auto result = df->filter(filterJson);
        bench.setOutputRows(result->rowCount());
    }
    // Le temps est automatiquement enregistré à la sortie du scope

    reporter.saveToFile("benchmark.json");
    return 0;
}
```

## 📈 Catégories de Benchmark

Les catégories recommandées :

| Catégorie | Description | Exemples |
|-----------|-------------|----------|
| **IO** | Lecture/écriture de fichiers | CSV, JSON, Excel |
| **Filter** | Opérations de filtrage | Égalité, contains, comparaisons |
| **OrderBy** | Opérations de tri | Simple, multi-colonnes |
| **GroupBy** | Agrégations et groupements | count, sum, avg, min, max |
| **Select** | Projection de colonnes | Sélection partielle |
| **Chain** | Opérations chaînées | filter + orderBy + select |
| **Transform** | Transformations de données | map, apply, join |

## 📝 Bonnes pratiques

### 1. Versionnage

Utilisez le semantic versioning pour vos benchmarks :
```cpp
BenchmarkReporter reporter("1.2.3");  // MAJOR.MINOR.PATCH
```

### 2. Nommage des fichiers

```bash
benchmark_v1.0.0_[date].json     # Version + date
benchmark_baseline.json          # Baseline de référence
benchmark_current.json           # Version en cours de développement
```

### 3. Organisation

```
benchmarks/
├── baseline/
│   ├── v1.0.0.json
│   └── v1.1.0.json
├── current/
│   └── latest.json
└── comparisons/
    ├── v1.0.0_vs_v1.1.0.json
    └── v1.1.0_vs_v1.2.0.json
```

### 4. CI/CD Integration

```bash
#!/bin/bash
# Script de CI pour vérifier les régressions

./benchmark
./compare_benchmarks baseline.json benchmark_report.json comparison.json

# Vérifier les régressions
REGRESSED=$(jq '.summary.regressed' comparison.json)

if [ "$REGRESSED" -gt 5 ]; then
    echo "Warning: $REGRESSED operations regressed!"
    exit 1
fi
```

## 🎯 Interprétation des résultats

### Status des comparaisons

- **improved** : Performance améliorée de >5%
- **regressed** : Performance dégradée de >5%
- **stable** : Variation entre -5% et +5%

### Seuils de tolérance

Vous pouvez modifier les seuils dans `BenchmarkReporter.cpp` :

```cpp
// Ligne ~180 dans BenchmarkReporter.cpp
if (percent_change < -5.0) {
    status = "improved";
} else if (percent_change > 5.0) {
    status = "regressed";
} else {
    status = "stable";
}
```

## 🔍 Analyse avancée

### Extraire les opérations les plus lentes

```bash
# Top 10 opérations les plus lentes
jq '.results | sort_by(.duration_ms) | reverse | .[0:10] | .[] | "\(.operation): \(.duration_ms)ms"' benchmark_report.json
```

### Statistiques par catégorie

```bash
# Afficher les stats par catégorie
jq '.statistics.by_category' benchmark_report.json
```

### Filtrer les régressions

```bash
# Afficher seulement les régressions
jq '.comparisons[] | select(.status == "regressed")' comparison.json
```

## 🚀 Workflow recommandé

1. **Baseline** : Créer un benchmark de référence avant optimisation
2. **Optimiser** : Modifier le code
3. **Re-benchmark** : Mesurer les nouvelles performances
4. **Comparer** : Analyser les différences
5. **Itérer** : Répéter jusqu'à satisfaction
6. **Commit** : Sauvegarder baseline + code optimisé

## 📚 Exemples complets

Voir :
- `examples/benchmark.cpp` - Benchmark complet sur 10k lignes
- `examples/compare_benchmarks.cpp` - Outil de comparaison
- `examples/benchmark_report.json` - Exemple de rapport

## 🛠️ Troubleshooting

### Le benchmark est trop lent

- Réduire le nombre d'opérations
- Utiliser un dataset plus petit pour les tests rapides
- Compiler en mode Release : `cmake -DCMAKE_BUILD_TYPE=Release ..`

### Les résultats varient beaucoup

- Fermer les applications gourmandes
- Exécuter plusieurs fois et faire une moyenne
- Utiliser `nice -n -20 ./benchmark` pour priorité haute

### Erreur "Cannot open file"

- Vérifier les chemins relatifs (../examples/)
- Exécuter depuis le répertoire `build/`