#pragma once

#include "nodes/Types.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace nodes {

/**
 * Execution context passed to node compile functions.
 * Provides access to inputs and allows setting outputs.
 *
 * Usage in onCompile:
 *   void onCompile(NodeContext& ctx) {
 *       auto a = ctx.getInputWorkload("a");
 *       ctx.setOutput("result", a.getInt() * 2);
 *   }
 */
class NodeContext {
public:
    NodeContext() = default;

    // === Input Access (called by node logic) ===

    /**
     * Get raw workload (value + type) for an input
     */
    Workload getInputWorkload(const std::string& name) const;

    /**
     * Check if input exists and is not null
     */
    bool hasInput(const std::string& name) const;

    /**
     * Check if input entry exists (even if null)
     * Used to check if a connection provides a value
     */
    bool hasInputEntry(const std::string& name) const;

    // === Output Setting (called by node logic) ===

    /**
     * Set output with explicit workload
     */
    void setOutput(const std::string& name, const Workload& workload);

    /**
     * Convenience overloads - infer type from value
     */
    void setOutput(const std::string& name, int64_t value);
    void setOutput(const std::string& name, double value);
    void setOutput(const std::string& name, const std::string& value);
    void setOutput(const std::string& name, const char* value);
    void setOutput(const std::string& name, bool value);
    void setOutput(const std::string& name, std::shared_ptr<dataframe::DataFrame> value);

    // === CSV Broadcasting Support ===

    /**
     * Get/set the active CSV for field lookups
     * Automatically set by executor when a CSV input is detected
     */
    std::shared_ptr<dataframe::DataFrame> getActiveCsv() const { return m_activeCsv; }
    void setActiveCsv(std::shared_ptr<dataframe::DataFrame> csv) { m_activeCsv = std::move(csv); }

    /**
     * Get value at specific row with automatic broadcasting
     * - Scalars: return same value for all rows
     * - Fields: lookup in active CSV
     */
    int64_t getIntAtRow(const std::string& inputName, size_t rowIndex) const;
    double getDoubleAtRow(const std::string& inputName, size_t rowIndex) const;
    std::string getStringAtRow(const std::string& inputName, size_t rowIndex) const;

    // === Error Handling ===

    void setError(const std::string& message);
    bool hasError() const { return m_hasError; }
    const std::string& getErrorMessage() const { return m_errorMessage; }

    // === Internal (used by executor) ===

    /**
     * Set an input value (called by executor before compile)
     */
    void setInput(const std::string& name, const Workload& workload);

    /**
     * Get an output value (called by executor after compile)
     */
    Workload getOutput(const std::string& name) const;

    /**
     * Get all outputs
     */
    const std::unordered_map<std::string, Workload>& getOutputs() const { return m_outputs; }

    /**
     * Get all inputs
     */
    const std::unordered_map<std::string, Workload>& getInputs() const { return m_inputs; }

private:
    std::unordered_map<std::string, Workload> m_inputs;
    std::unordered_map<std::string, Workload> m_outputs;
    std::shared_ptr<dataframe::DataFrame> m_activeCsv;
    bool m_hasError = false;
    std::string m_errorMessage;
};

} // namespace nodes
