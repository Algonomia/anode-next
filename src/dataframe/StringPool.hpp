#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace dataframe {

/**
 * String pooling / Dictionary encoding pour optimiser les comparaisons
 *
 * Au lieu de stocker des strings partout, on stocke des indices (uint32_t)
 * qui pointent vers un dictionnaire global.
 *
 * Avantages:
 * - Comparaisons: O(1) au lieu de O(n)
 * - Hash: O(1) au lieu de O(n)
 * - Mémoire: strings dupliquées stockées une seule fois
 * - Cache friendly: indices contigus en mémoire
 */
class StringPool {
public:
    using StringId = uint32_t;
    static constexpr StringId INVALID_ID = UINT32_MAX;

    StringPool() {
        // Réserver de l'espace pour éviter les reallocations
        m_strings.reserve(1024);
        m_string_to_id.reserve(1024);
    }

    /**
     * Ajoute une string au pool et retourne son ID
     * Si la string existe déjà, retourne l'ID existant
     */
    StringId intern(const std::string& str) {
        // Chercher si la string existe déjà
        auto it = m_string_to_id.find(str);
        if (it != m_string_to_id.end()) {
            return it->second;
        }

        // Ajouter la nouvelle string
        StringId id = static_cast<StringId>(m_strings.size());
        m_strings.push_back(str);
        m_string_to_id[str] = id;

        return id;
    }

    /**
     * Récupère la string à partir de son ID
     */
    const std::string& getString(StringId id) const {
        if (id >= m_strings.size()) {
            static const std::string empty;
            return empty;
        }
        return m_strings[id];
    }

    /**
     * Vérifie si un ID est valide
     */
    bool isValid(StringId id) const {
        return id < m_strings.size();
    }

    /**
     * Retourne le nombre de strings uniques
     */
    size_t size() const {
        return m_strings.size();
    }

    /**
     * Réserve de l'espace pour éviter les reallocations
     */
    void reserve(size_t capacity) {
        m_strings.reserve(capacity);
        m_string_to_id.reserve(capacity);
    }

    /**
     * Vide le pool
     */
    void clear() {
        m_strings.clear();
        m_string_to_id.clear();
    }

    /**
     * Statistiques mémoire
     */
    size_t memoryUsage() const {
        size_t total = 0;
        for (const auto& str : m_strings) {
            total += str.capacity();
        }
        total += m_string_to_id.size() * (sizeof(std::string) + sizeof(StringId));
        return total;
    }

private:
    std::vector<std::string> m_strings;           // ID → String
    std::unordered_map<std::string, StringId> m_string_to_id;  // String → ID
};

} // namespace dataframe