#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "nodes/Types.hpp"
#include "dataframe/DataFrame.hpp"

namespace nodes {

/**
 * @brief Paramètre de requête dynamique avec préfixe de type
 *
 * Préfixes (comme dans POSTGRESQL_REQUESTS.md):
 * - Minuscule (i, d, s, b) : Valeur scalaire
 * - Majuscule (I, D, S, J) : Tableau de valeurs
 */
struct DynParameter {
    std::string name;   // Ex: "I0", "S1", "i2"
    std::string value;  // Valeur JSON sérialisée
};

/**
 * @brief Constructeur de requêtes PostgreSQL dynamiques
 *
 * Permet de construire des appels de fonctions PostgreSQL avec
 * des paramètres typés, supportant le broadcasting depuis les workloads.
 *
 * Exemple:
 * @code
 * DynRequest req;
 * req.func("my_function")
 *    .addIntArrayParam({10, 20, 30})
 *    .addStringArrayParam({"Planning", "Execution", "Review"});
 *
 * std::string sql = req.buildSQL();
 * // -> "SELECT * FROM my_function(ARRAY[10, 20, 30]::INT[], ARRAY['Planning', 'Execution', 'Review']::TEXT[])"
 * @endcode
 */
class DynRequest {
public:
    DynRequest() = default;

    /**
     * @brief Définit le nom de la fonction PostgreSQL à appeler
     */
    DynRequest& func(const std::string& functionName);

    // ========== Paramètres scalaires ==========

    /**
     * @brief Ajoute un entier scalaire (préfixe: i)
     */
    DynRequest& addIntParam(int64_t value);

    /**
     * @brief Ajoute un double scalaire (préfixe: d)
     */
    DynRequest& addDoubleParam(double value);

    /**
     * @brief Ajoute une chaîne scalaire (préfixe: s)
     */
    DynRequest& addStringParam(const std::string& value);

    /**
     * @brief Ajoute un booléen scalaire (préfixe: b)
     */
    DynRequest& addBoolParam(bool value);

    /**
     * @brief Ajoute NULL (préfixe: n)
     */
    DynRequest& addNullParam();

    // ========== Paramètres tableaux ==========

    /**
     * @brief Ajoute un tableau d'entiers (préfixe: I)
     */
    DynRequest& addIntArrayParam(const std::vector<int64_t>& values);

    /**
     * @brief Ajoute un tableau de doubles (préfixe: D)
     */
    DynRequest& addDoubleArrayParam(const std::vector<double>& values);

    /**
     * @brief Ajoute un tableau de chaînes (préfixe: S)
     */
    DynRequest& addStringArrayParam(const std::vector<std::string>& values);

    /**
     * @brief Ajoute un tableau 2D d'entiers (préfixe: J)
     */
    DynRequest& addIntArray2DParam(const std::vector<std::vector<int64_t>>& values);

    // ========== Paramètres depuis workload (avec broadcasting) ==========

    /**
     * @brief Ajoute un tableau d'entiers depuis un workload
     *
     * Si le workload est un scalaire, la valeur est broadcastée sur toutes les lignes du CSV.
     * Si le workload est un field, les valeurs sont extraites de la colonne correspondante.
     *
     * @param workload Le workload source
     * @param csv Le DataFrame pour la résolution des fields
     * @param nullIfNotDefined Si true, ajoute NULL si le workload est null
     */
    DynRequest& addIntArrayFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    /**
     * @brief Ajoute un tableau de chaînes depuis un workload
     */
    DynRequest& addStringArrayFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    /**
     * @brief Ajoute un tableau de doubles depuis un workload
     */
    DynRequest& addDoubleArrayFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    /**
     * @brief Ajoute un entier scalaire depuis un workload
     */
    DynRequest& addIntFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    /**
     * @brief Ajoute une chaîne scalaire depuis un workload
     */
    DynRequest& addStringFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    /**
     * @brief Ajoute un timestamp scalaire depuis un workload
     *
     * Convertit les strings en timestamps si nécessaire.
     * Pour les fields, vérifie que toutes les valeurs sont identiques.
     * Formats supportés: Unix timestamp, dd/mm/yyyy, dd/mm/yy
     */
    DynRequest& addTimestampFromWorkload(
        const nodes::Workload& workload,
        const std::shared_ptr<dataframe::DataFrame>& csv,
        bool nullIfNotDefined = true);

    // ========== Construction SQL ==========

    /**
     * @brief Construit la requête SQL finale
     * @return SQL au format "SELECT * FROM function_name(param1, param2, ...)"
     */
    std::string buildSQL() const;

    /**
     * @brief Retourne le nom de la fonction
     */
    const std::string& getFunctionName() const { return m_functionName; }

    /**
     * @brief Retourne les paramètres
     */
    const std::vector<DynParameter>& getParameters() const { return m_parameters; }

    /**
     * @brief Réinitialise la requête
     */
    void reset();

private:
    /**
     * @brief Convertit un paramètre en sa représentation SQL
     */
    std::string paramToSQL(const DynParameter& param) const;

    /**
     * @brief Échappe une chaîne pour SQL (apostrophes)
     */
    static std::string escapeString(const std::string& str);

    /**
     * @brief Génère le prochain nom de paramètre avec le préfixe donné
     */
    std::string nextParamName(char prefix);

    std::string m_functionName;
    std::vector<DynParameter> m_parameters;
};

} // namespace nodes
