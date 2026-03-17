#ifndef SPECULATIVE_DECODER_H
#define SPECULATIVE_DECODER_H

#include <QObject>
#include <QString>
#include <QList>

class SpeculativeDecoder : public QObject
{
    Q_OBJECT

public:
    explicit SpeculativeDecoder(QObject *parent = nullptr);
    ~SpeculativeDecoder();

    // Set draft model (smaller, faster model for speculation)
    void setDraftModel(const QString &modelPath);
    
    // Set target model (larger, more accurate model for verification)
    void setTargetModel(const QString &modelPath);
    
    // Generate tokens using speculative decoding
    // Returns verified tokens from target model
    QList<int> generateTokens(const QString &prompt, int maxTokens);

signals:
    void tokensGenerated(const QList<int> &tokens);
    void acceptanceRateChanged(float rate);

private:
    QList<int> generateDraftTokens(const QString &prompt, int maxTokens);
    QList<int> verifyTokens(const QString &prompt, const QList<int> &draftTokens);

    QString m_draftModelPath;
    QString m_targetModelPath;
    bool m_gpuAccelerated;
    bool m_draftModelLoaded;
    bool m_targetModelLoaded;
};

#endif // SPECULATIVE_DECODER_H
