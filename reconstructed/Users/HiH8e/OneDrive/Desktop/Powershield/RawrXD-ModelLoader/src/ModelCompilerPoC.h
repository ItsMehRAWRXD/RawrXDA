#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

/**
 * @brief Header-only helper implementing the divide-by-seven "model compiler" PoC.
 */
class ModelCompilerPoC {
public:
    /**
     * @brief Divide the supplied corpus size by seven, guarding negative input.
     */
    static double digestCorpusSize(std::int64_t bytes) noexcept
    {
        return bytes < 0 ? 0.0 : static_cast<double>(bytes) / 7.0;
    }

    /**
     * @brief Build a human-readable report string summarising the calculation.
     */
    static std::string formatCompilerOutput(const std::string& modelName,
                                            std::int64_t corpusSizeInBytes)
    {
        const double result = digestCorpusSize(corpusSizeInBytes);

        std::ostringstream out;
        out << "--- " << modelName << " Model Compiler Report ---\n"
            << "Input corpus size (bytes): " << corpusSizeInBytes << '\n'
            << "Digest factor (PoC)      : 1 / 7\n"
            << "Calculated result        : "
            << std::fixed << std::setprecision(2) << result << "\n"
            << "--------------------------------------\n"
            << "NOTE: result is a synthetic resource unit derived from corpus size.\n";
        return out.str();
    }
};
