#pragma once

#include <QString>

/**
 * @brief Proof-of-concept utilities for the "model compiler" feature.
 *        Provides lightweight helpers that simulate digest metrics derived
 *        from user-specified corpus sizes.
 */
class ModelCompilerUtils {
public:
    /**
     * @brief Returns corpusSizeInBytes divided by the digest factor.
     * @param corpusSizeInBytes Raw byte size supplied by the caller.
     */
    static double digestCorpusSize(qint64 corpusSizeInBytes);

    /**
     * @brief Produces a formatted report that includes the model name and
     *        digest calculation for display in UI layers.
     */
    static QString formatCompilerOutput(const QString& modelName, qint64 corpusSizeInBytes);

private:
    static constexpr double kDigestFactor = 7.0;
};
