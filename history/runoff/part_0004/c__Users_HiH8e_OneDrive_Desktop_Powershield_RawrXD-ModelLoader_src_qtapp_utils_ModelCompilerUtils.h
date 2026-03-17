#pragma once

#include <QString>

/**
 * @brief Utility helper for the "model compiler" proof of concept.
 *
 * The functions hosted here simulate the resource estimation math that will
 * later live inside the Model School pipeline. For now, it satisfies the
 * requirement of letting a user name their compiler and see corpus_size / 7.
 */
class ModelCompilerUtils {
public:
    /**
     * @brief Divide a corpus size by the PoC digest factor (7.0).
     * @param corpusSizeInBytes Raw corpus size in bytes (non-negative).
     * @return Result of corpusSizeInBytes / 7.0. Negative inputs yield 0.0.
     */
    static double digestCorpusSize(qint64 corpusSizeInBytes);

    /**
     * @brief Produce a formatted report for UI display.
     * @param modelName Arbitrary compiler/model name selected by the user.
     * @param corpusSizeInBytes Raw corpus size in bytes.
     * @return Multi-line QString describing the digest calculation.
     */
    static QString formatCompilerOutput(const QString& modelName, qint64 corpusSizeInBytes);

private:
    static constexpr double DIGEST_FACTOR = 7.0;
};
