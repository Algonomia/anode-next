Plan : QCOM Grid Formatter — Server-Side Rendering

Contexte

Le front/core Angular formate les données QCOM (questionnaires) entièrement côté client (parsers TypeScript, résolution PEV, pivot en mémoire). L'objectif est de reproduire ce rendu dans le remote-grid d'anode-next en déplaçant toute la logique de transformation côté C++/SQL (server-side rendering, mode lecture seule uniquement).

Pipeline complet

[export_data_from_tasks]                     [get_question_metadata]
│ (données plates)                           │ (question_id, type, precision, round, title)
▼                                            │
[qcom_ref_preparer] (remplace datavalue_value       │
par metadatavalue_pev_hv_id pour les Ref)          │
│                                            │
▼                                            │
[pivot] (pivotColumn="question id",                 │
valueColumn="datavalue value",             │
indexColumn="metadatavalue id")             │
│                                            │
▼                                            ▼
[qcom_formatter] ◄──── questions_csv ──── [get_question_metadata]
│                ◄──── language ──── [string_value: "fr"]
▼
[output] → remote-grid

Décisions de design (validées)

1. Pivot sur question_id (pas le titre) → robuste, pas de mismatch i18n. Le formatter renomme les colonnes avec les titres traduits après formatage.
2. Pré-traitement avant pivot pour les Reference : un noeud qcom_ref_preparer remplace datavalue_value par metadatavalue_pev_hv_id pour les questions de type Reference.
3. Numeric : DoubleColumn (nombre arrondi) + metadata unité séparée. Tri numérique possible dans ag-grid.
4. Formatage colonne par colonne (post-pivot) pour être cache-friendly.

 ---
3 projets concernés, 3 agents

 ---
AGENT 1 : Projet aorm (SQL)

Répertoire : /home/julien/Project/back/aorm
Scope : Ajouter 1 fonction SQL

Tâche unique : Créer anode_get_question_metadata

Fichier à modifier : /home/julien/Project/back/aorm/aorm/project/corecppScript/Anode/Form/form.psql (ajouter à la fin)

Fonction à implémenter :
CREATE OR REPLACE FUNCTION anode_get_question_metadata(
p_question_ids INT[],
p_language TEXT DEFAULT 'en'
)
RETURNS TABLE (
"question id"       INT,
"question title"    TEXT,
"question type"     INT,
"precision"         INT,
"round"             INT,
"ref_pev_type"      TEXT
)

Logique :
- Requêter form_question pour les IDs fournis
- Extraire le titre via get_from_json_v3(title, ARRAY[p_language]) (fonction utilitaire existante, utilisée dans les autres fonctions du même fichier — voir anode_identify_questions comme référence)
- Retourner type, precision, round directement depuis form_question
- Pour ref_pev_type : si type = 6 (Reference), déterminer 'hv' ou 'at' à partir de ref_question_pev_type, sinon NULL
- Si p_question_ids est NULL, retourner toutes les questions

Fichiers de référence à lire :
- /home/julien/Project/back/aorm/aorm/project/corecppScript/Anode/Form/form.psql — les fonctions existantes anode_identify_form, anode_identify_section, anode_identify_questions pour le pattern
- /home/julien/Project/back/aorm/aorm/project/corecpp/form/question.py — le modèle Question (champs title, type, precision, round, ref_question_pev_type, tagbf)

Pas d'autre modification dans ce projet.

 ---
AGENT 2 : Projet anode-next (C++)

Répertoire : /home/julien/Project/back/anode-next
Scope : L'essentiel du travail — 3 noeuds C++ + 7 formatters

Fichiers de référence à lire impérativement

┌──────────────────────────────────────────────┬──────────────────────────────────────────────────────────────────────────────────┐
│                   Fichier                    │                                     Pourquoi                                     │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/DataNodes.cpp      │ Pattern noeud SQL (DynRequest, executeQuery) — copier pour get_question_metadata │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/DataNodes.hpp      │ Déclarations à enrichir                                                          │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/register.cpp       │ Où enregistrer les nouveaux noeuds                                               │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/PerimeterCache.hpp │ API du cache PEV — ajouter accesseurs                                            │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/PerimeterCache.cpp │ Implémenter les accesseurs                                                       │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/algonomia/CMakeLists.txt     │ Ajouter les nouveaux .cpp                                                        │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/nodes/common/AggregateNodes.cpp    │ Comprendre le noeud pivot (entrées/sorties)                                      │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/dataframe/Column.hpp                     │ API IntColumn/DoubleColumn/StringColumn                                          │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/dataframe/DataFrame.hpp                  │ API DataFrame (addColumn, setColumn, getColumn, etc.)                            │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/NodeBuilder.hpp                    │ API fluent pour enregistrer un noeud                                             │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/NodeContext.hpp                    │ API contexte d'exécution (getInputWorkload, setOutput, etc.)                     │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ src/nodes/Types.hpp                          │ NodeType, Workload, PortType                                                     │
├──────────────────────────────────────────────┼──────────────────────────────────────────────────────────────────────────────────┤
│ docs/nodes/IMPLEMENTATION-CHECKLIST.md       │ Checklist officielle pour créer un noeud                                         │
└──────────────────────────────────────────────┴──────────────────────────────────────────────────────────────────────────────────┘

Étape 2.1 : Modifier PerimeterCache (2 méthodes triviales)

Fichier : src/nodes/nodes/algonomia/PerimeterCache.hpp
Ajouter dans la section public :
std::string getHvPevLabel(int pevId) const;
std::string getAtPevLabel(int pevId) const;

Fichier : src/nodes/nodes/algonomia/PerimeterCache.cpp
Implémenter : simple lookup dans m_hvPevLabels / m_atPevLabels, retourner "" si non trouvé.

Étape 2.2 : Créer les formatters (7 fichiers, SRP strict)

Dossier à créer : src/nodes/nodes/algonomia/formatters/

formatters/IQcomFormatter.hpp — Types communs (header-only)

#pragma once
#include "dataframe/Column.hpp"
#include "dataframe/StringPool.hpp"
#include <memory>
#include <string>

namespace nodes::formatters {

enum class QuestionType : int {
TextSmall = 1, TextLarge = 2, Date = 3, Numeric = 4,
QCM = 5, Reference = 6, Multiple = 7, Upload = 8
};

struct QuestionMeta {
int questionType;
int precision;
int roundType;        // 0=none, 1=round, 2=ceil, 3=floor
std::string refPevType; // "hv" ou "at" pour Reference, vide sinon
std::string title;    // titre traduit pour renommer la colonne
};

} // namespace nodes::formatters

formatters/QcomTextFormatter.hpp/.cpp

- Responsabilité : Format texte + extraction i18n
- Input : StringColumn avec valeur brute (texte ou JSON i18n {"fr":"...","en":"..."})
- Output : StringColumn avec texte extrait dans la langue demandée
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, const std::string& language, std::shared_ptr<StringPool> pool)
- Logique : nlohmann::json::parse(), si objet JSON avec clé language → extraire, sinon passthrough. Catch les exceptions de parsing (valeurs non-JSON = passthrough).

formatters/QcomDateFormatter.hpp/.cpp

- Responsabilité : Parse JSON date → dd/MM/yyyy
- Input : StringColumn avec JSON {"values":[{"type":"min","value":tsMs},{"type":"max","value":tsMs}]}
- Output : StringColumn avec date formatée
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, std::shared_ptr<StringPool> pool)
- Logique :
    - Parse JSON, extraire values array
    - Convertir timestamp ms → time_t (diviser par 1000)
    - gmtime_r() + snprintf("%02d/%02d/%04d", day, month, year)
    - Si 2 valeurs (min + max) → "dd/MM/yyyy - dd/MM/yyyy"
    - Si 1 valeur → "dd/MM/yyyy"
    - Si parse échoue → string vide

formatters/QcomNumericFormatter.hpp/.cpp

- Responsabilité : String → DoubleColumn arrondi
- Input : StringColumn avec nombre brut "1234.56"
- Output : DoubleColumn (changement de type !) avec nombre arrondi
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, int precision, int roundType)
- Logique :
    - std::stod(raw) pour parser
    - Arrondi : factor = pow(10, precision), puis round/ceil/floor(value * factor) / factor selon roundType
    - Cellules vides ou non-parsables → 0.0
- Important : la colonne de sortie est un DoubleColumn (pas StringColumn) pour permettre le tri/agrégation numérique dans ag-grid

formatters/QcomQcmFormatter.hpp/.cpp

- Responsabilité : Parse JSON QCM → labels concaténés
- Input : StringColumn avec JSON {"values":[{"id":N,"label":{"fr":"...","en":"..."},"value":{"fr":"...","en":"..."}}]}
- Output : StringColumn avec labels joints par |
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, const std::string& language, std::shared_ptr<StringPool> pool)
- Logique :
    - Parse JSON, itérer values array
    - Pour chaque entrée : extraire label[language] en priorité, fallback sur value[language]
    - Joindre avec |
    - Si parse échoue → passthrough de la valeur brute

formatters/QcomReferenceFormatter.hpp/.cpp

- Responsabilité : PEV ID → label via PerimeterCache
- Input : StringColumn avec PEV ID en string (ex: "42", grâce au pré-traitement qcom_ref_preparer)
- Output : StringColumn avec label PEV résolu
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, const std::string& refPevType, std::shared_ptr<StringPool> pool)
- Logique :
    - std::stoi(raw) pour extraire l'ID
    - Si refPevType == "hv" → PerimeterCache::instance().getHvPevLabel(id)
    - Si refPevType == "at" → PerimeterCache::instance().getAtPevLabel(id)
    - Si label vide → garder la valeur brute
- Dépendance : #include "PerimeterCache.hpp" (étape 2.1)

formatters/QcomUploadFormatter.hpp/.cpp

- Responsabilité : Passthrough (copie la colonne telle quelle)
- Méthode statique : static IColumnPtr format(IColumnPtr srcCol, std::shared_ptr<StringPool> pool) → return srcCol;

Étape 2.3 : Créer le noeud get_question_metadata

Fichier à modifier : src/nodes/nodes/algonomia/DataNodes.hpp — ajouter void registerGetQuestionMetadataNode();
Fichier à modifier : src/nodes/nodes/algonomia/DataNodes.cpp — ajouter la fonction + l'appeler dans registerDataNodes()

Signature du noeud :
NodeBuilder("get_question_metadata", "data")
.inputOptional("csv", Type::Csv)
.inputOptional("question_ids", {Type::Field, Type::Int})
.inputOptional("language", Type::String)
.output("csv", Type::Csv)
.output("question_id", Type::Field)
.output("question_title", Type::Field)
.output("question_type", Type::Field)
.output("precision", Type::Field)
.output("round", Type::Field)
.output("ref_pev_type", Type::Field)

Logique onCompile : Même pattern que registerExportDataFromTasksNode() — DynRequest + pool.executeQuery().
- req.func("anode_get_question_metadata")
- .addIntArrayFromWorkload(questionIdsWL, csv, true) pour les question_ids
- .addStringFromWorkload(languageWL, csv) pour la langue (ou .addStringParam("en") par défaut)

Étape 2.4 : Créer le noeud qcom_ref_preparer

Fichiers à créer :
- src/nodes/nodes/algonomia/QcomRefPreparerNode.hpp
- src/nodes/nodes/algonomia/QcomRefPreparerNode.cpp

Signature :
NodeBuilder("qcom_ref_preparer", "data")
.input("csv", Type::Csv)              // DataFrame plat de export_data_from_tasks
.input("questions_csv", Type::Csv)    // Metadata question (de get_question_metadata)
.output("csv", Type::Csv)

Logique onCompile :
1. Lire questions_csv → construire std::unordered_set<int> des question_ids de type 6 (Reference) + std::unordered_map<int, std::string> question_id → ref_pev_type ("hv"/"at")
2. Lire les colonnes du CSV d'entrée : "question id" (IntColumn), "datavalue value" (StringColumn), "metadatavalue pev_hv_id" (IntColumn), "metadatavalue pev_at_id" (IntColumn)
3. Créer un nouveau DataFrame, copier toutes les colonnes sauf "datavalue value"
4. Créer une nouvelle StringColumn("datavalue value") :
- Pour chaque ligne : si question_id est dans le Set → prendre metadatavalue_pev_hv_id (ou pev_at_id) et convertir en string via std::to_string()
- Sinon → copier la valeur originale de "datavalue value"
5. Ajouter la colonne et sortir le CSV

Étape 2.5 : Créer le noeud qcom_formatter (noeud principal)

Fichiers à créer :
- src/nodes/nodes/algonomia/QcomFormatterNode.hpp
- src/nodes/nodes/algonomia/QcomFormatterNode.cpp

Signature :
NodeBuilder("qcom_formatter", "data")
.input("csv", Type::Csv)              // DataFrame pivoté (colonnes = question_ids en string)
.input("questions_csv", Type::Csv)    // Metadata question
.inputOptional("language", Type::String)  // Langue pour i18n (défaut: "en")
.output("csv", Type::Csv)             // DataFrame formaté, colonnes renommées en titres

Logique onCompile :
1. Lire questions_csv → construire std::unordered_map<std::string, QuestionMeta> keyed par std::to_string(question_id) (= nom de colonne dans le DataFrame pivoté)
2. Lire language (défaut "en")
3. Créer le DataFrame de sortie avec le même StringPool
4. Pour chaque colonne du DataFrame pivoté :
- Lookup dans le map par nom de colonne
- Pas trouvé (ex: "metadatavalue id", colonnes d'index) → result->addColumn(col) (copie par shared_ptr, zero-copy)
- Trouvé → dispatcher au formatter selon meta.questionType :
    - 1, 2 → QcomTextFormatter::format(col, language, pool)
    - 3 → QcomDateFormatter::format(col, pool)
    - 4 → QcomNumericFormatter::format(col, meta.precision, meta.roundType)
    - 5 → QcomQcmFormatter::format(col, language, pool)
    - 6 → QcomReferenceFormatter::format(col, meta.refPevType, pool)
    - 8 → QcomUploadFormatter::format(col, pool)
    - default → passthrough
- Renommer la colonne de sortie avec meta.title (le titre traduit de la question)
5. Sortir le DataFrame

Pour le renommage : chaque formatter retourne un IColumnPtr. Il faut soit que le formatter accepte un paramètre destName, soit qu'on crée la colonne avec le bon nom. Le plus propre : chaque formatter prend un const std::string& destName en paramètre et nomme la colonne de sortie avec ce nom.

Étape 2.6 : Registration + CMake

Fichier : src/nodes/nodes/algonomia/register.cpp
Ajouter :
#include "QcomFormatterNode.hpp"
#include "QcomRefPreparerNode.hpp"

// Dans registerNodes() :
nodes::registerQcomFormatterNode();
nodes::registerQcomRefPreparerNode();

Fichier : src/nodes/nodes/algonomia/CMakeLists.txt
Ajouter à add_library(algonomia_nodes OBJECT ...) :
QcomFormatterNode.cpp
QcomRefPreparerNode.cpp
formatters/QcomTextFormatter.cpp
formatters/QcomDateFormatter.cpp
formatters/QcomNumericFormatter.cpp
formatters/QcomQcmFormatter.cpp
formatters/QcomReferenceFormatter.cpp
formatters/QcomUploadFormatter.cpp

 ---
AGENT 3 : Projet front/core (Angular)

Répertoire : /home/julien/Project/front/core
Scope : Rien à modifier. Ce projet sert uniquement de référence pour comprendre les formats JSON et la logique de formatage. Aucun changement requis.

 ---
Récapitulatif par projet

┌──────────────────────┬─────────┬───────────────────┬──────────────────────┐
│        Projet        │  Agent  │ Nb fichiers créés │ Nb fichiers modifiés │
├──────────────────────┼─────────┼───────────────────┼──────────────────────┤
│ aorm (SQL)           │ Agent 1 │ 0                 │ 1 (form.psql)        │
├──────────────────────┼─────────┼───────────────────┼──────────────────────┤
│ anode-next (C++)     │ Agent 2 │ 16                │ 5                    │
├──────────────────────┼─────────┼───────────────────┼──────────────────────┤
│ front/core (Angular) │ —       │ 0                 │ 0                    │
└──────────────────────┴─────────┴───────────────────┴──────────────────────┘

Détail fichiers anode-next

┌──────────┬───────────────────────────────────────┬──────────────────────────────────────────┐
│  Action  │                Fichier                │              Responsabilité              │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/IQcomFormatter.hpp         │ Types communs (QuestionMeta, enum)       │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomTextFormatter.hpp      │ Header text formatter                    │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomTextFormatter.cpp      │ Extraction i18n de texte                 │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomDateFormatter.hpp      │ Header date formatter                    │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomDateFormatter.cpp      │ Parse JSON date → dd/MM/yyyy             │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomNumericFormatter.hpp   │ Header numeric formatter                 │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomNumericFormatter.cpp   │ String → DoubleColumn arrondi            │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomQcmFormatter.hpp       │ Header QCM formatter                     │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomQcmFormatter.cpp       │ Parse JSON QCM → labels                  │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomReferenceFormatter.hpp │ Header reference formatter               │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomReferenceFormatter.cpp │ PEV ID → label via cache                 │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomUploadFormatter.hpp    │ Header upload formatter                  │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ formatters/QcomUploadFormatter.cpp    │ Passthrough                              │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ QcomFormatterNode.hpp                 │ Header noeud principal                   │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ QcomFormatterNode.cpp                 │ Dispatch par colonne + renommage         │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ QcomRefPreparerNode.hpp               │ Header pré-traitement Ref                │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ CREER    │ QcomRefPreparerNode.cpp               │ Remplacement dv_value par pev_id         │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ DataNodes.hpp                         │ Déclarer registerGetQuestionMetadataNode │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ DataNodes.cpp                         │ Implémenter le noeud SQL                 │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ PerimeterCache.hpp                    │ Ajouter getHvPevLabel/getAtPevLabel      │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ PerimeterCache.cpp                    │ Implémenter les 2 accesseurs             │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ register.cpp                          │ Enregistrer les 2 nouveaux noeuds        │
├──────────┼───────────────────────────────────────┼──────────────────────────────────────────┤
│ MODIFIER │ CMakeLists.txt                        │ Ajouter les 8 nouveaux .cpp              │
└──────────┴───────────────────────────────────────┴──────────────────────────────────────────┘

 ---
Ordre d'implémentation (dans chaque agent)

Agent 1 (aorm) — peut démarrer immédiatement

1. Lire les fonctions existantes dans form.psql pour comprendre le pattern
2. Implémenter anode_get_question_metadata

Agent 2 (anode-next) — peut démarrer en parallèle de l'agent 1

1. PerimeterCache : ajouter les 2 accesseurs (pré-requis pour ReferenceFormatter)
2. formatters/IQcomFormatter.hpp (pré-requis pour tous les formatters et les noeuds)
3. Les 6 formatters (parallélisable, indépendants entre eux)
4. get_question_metadata node (dans DataNodes.cpp)
5. QcomRefPreparerNode
6. QcomFormatterNode
7. register.cpp + CMakeLists.txt
8. Build : cd build && cmake .. && make -j

 ---
Vérification

1. Build : cd /home/julien/Project/back/anode-next/build && cmake .. && make -j — compilation sans erreur
2. SQL : Déployer la fonction sur la BDD, tester : SELECT * FROM anode_get_question_metadata(ARRAY[1,2,3], 'fr')
3. Test unitaire (optionnel mais recommandé) : Créer QcomFormatterTest.cpp avec Catch2 :
- Construire un DataFrame mock avec des valeurs brutes de chaque type
- Appeler chaque formatter individuellement, vérifier les valeurs de sortie
4. Test d'intégration : Dans l'éditeur de graphe, créer le pipeline complet et exécuter
5. Test remote-grid : Vérifier l'affichage dans l'ag-grid du remote viewer