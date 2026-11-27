#pragma once

#include "common/shared_array.hpp"
#include <cstddef>
#include <string>

namespace nicxlive::core::serde {
struct InLoadDiagnostics {
    std::size_t recoveredComponents = 0;
    std::size_t repairedWeldingMappings = 0;
    common::SharedArray<std::string> warnings;
};

// Unqualified D module globals are thread-local; each loader thread owns its diagnostics.
inline thread_local InLoadDiagnostics inLastLoadDiagnostics;

inline void inRecordWeldingRecovery(const std::string& context, std::size_t count) {
    inLastLoadDiagnostics.repairedWeldingMappings += count;
    if (inLastLoadDiagnostics.warnings.size() < 32)
        inLastLoadDiagnostics.warnings.push_back(context + ": removed " + std::to_string(count) +
            " inconsistent Welding mappings");
}

inline void inRecordLoadRecovery(const std::string& context, std::size_t count) {
    inLastLoadDiagnostics.recoveredComponents += count;
    if (inLastLoadDiagnostics.warnings.size() < 32)
        inLastLoadDiagnostics.warnings.push_back(context + ": reconstructing " + std::to_string(count) +
            " nonfinite deformation components from finite mesh samples");
}
} // namespace nicxlive::core::serde
