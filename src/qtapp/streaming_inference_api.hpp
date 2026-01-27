#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <functional>

/**
 * @brief Streaming inference API with token-by-token callbacks
 * 
 * Features:
 * - Real-time token streaming
 * - Progress callbacks
 * - Backpressure handling
 * - Cancellation support
 * - Partial result delivery
 */
class StreamingInferenceAPI : public QObject {
    Q_OBJECT

public:
    using TokenCallback = std::function<void(const QString& token, int position)>;
    using ProgressCallback = std::function<void(int tokensGenerated, int totalTokens)>;
    using CompletionCallback = std::function<void(const QString& fullResult)>;
    using ErrorCallback = std::function<void(const QString& error)>;

    explicit StreamingInferenceAPI(QObject* parent = nullptr);
    ~StreamingInferenceAPI();

    /**
     * @brief Start streaming inference
     * @param modelPath Path to GGUF model
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     * @param temperature Sampling temperature
     * @return Stream ID for tracking
     */
    qint64 startStream(const QString& modelPath, const QString& prompt,
                       int maxTokens = 256, float temperature = 0.7f);

    /**
     * @brief Cancel an active stream
     */
    bool cancelStream(qint64 streamId);

    /**
     * @brief Check if stream is active
     */
    bool isStreamActive(qint64 streamId) const;

    /**
     * @brief Set token callback (called for each generated token)
     */
    void setTokenCallback(TokenCallback callback);

    /**
     * @brief Set progress callback (called periodically)
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set completion callback (called when stream finishes)
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Set error callback
     */
    void setErrorCallback(ErrorCallback callback);

signals:
    void tokenGenerated(qint64 streamId, const QString& token, int position);
    void progressUpdated(qint64 streamId, int tokensGenerated, int totalTokens);
    void streamCompleted(qint64 streamId, const QString& fullResult);
    void streamFailed(qint64 streamId, const QString& error);
    void streamCancelled(qint64 streamId);

public slots:
    void onTokenReady(qint64 streamId, const QString& token);
    void onStreamProgress(qint64 streamId, int current, int total);
    void onStreamComplete(qint64 streamId, const QString& result);
    void onStreamError(qint64 streamId, const QString& error);

private:
    struct StreamState {
        qint64 id;
        QString modelPath;
        QString prompt;
        QString partialResult;
        int tokensGenerated = 0;
        int maxTokens;
        bool active = true;
    };

    QHash<qint64, StreamState> m_activeStreams;
    qint64 m_nextStreamId = 1;
    
    TokenCallback m_tokenCallback;
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    ErrorCallback m_errorCallback;
};
