#include "ModelCompilerUtils.h"

#include <QLocale>
#include <QDebug>

// -----------------------------------------------------------------------------
// Core math helpers
// -----------------------------------------------------------------------------

double ModelCompilerUtils::digestCorpusSize(qint64 corpusSizeInBytes)
{
    if (corpusSizeInBytes < 0) {
        qWarning() << "ModelCompilerUtils: negative corpus size" << corpusSizeInBytes;
        return 0.0;
    }

    return static_cast<double>(corpusSizeInBytes) / DIGEST_FACTOR;
}

// -----------------------------------------------------------------------------
// Presentation helpers
// -----------------------------------------------------------------------------

QString ModelCompilerUtils::formatCompilerOutput(const QString& modelName, qint64 corpusSizeInBytes)
{
    const double result = digestCorpusSize(corpusSizeInBytes);
    const QLocale uiLocale;

    const QString formattedCorpus = uiLocale.toString(corpusSizeInBytes);
    const QString formattedResult = uiLocale.toString(result, 'f', 2);

    const QString report = QString(
        "--- %1 Model Compiler Report ---\n"
        "Input Corpus Size (bytes): %2\n"
        "Digest Factor (PoC): 1 / %3\n"
        "Calculated Result (Resource Unit): %4\n"
        "--------------------------------------\n"
        "NOTE: Result represents a proportional resource unit (e.g., VRAM blocks)."
    ).arg(modelName)
     .arg(formattedCorpus)
     .arg(DIGEST_FACTOR)
     .arg(formattedResult);

    qDebug() << "ModelCompilerUtils:" << modelName
             << "digest" << corpusSizeInBytes
             << "->" << result;

    return report;
}
