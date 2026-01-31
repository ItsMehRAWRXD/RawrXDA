#pragma once


/**
 * @brief Token-by-token streaming output for inference results
 * 
 * Handles real-time streaming of inference tokens to a console widget.
 * All UI updates are queued to the main thread for thread safety.
 */
class StreamingInference : public void {

public:
    explicit StreamingInference(QPlainTextEdit* target, void* parent = nullptr);

public:
    void startStream(int64_t reqId, const std::string& prompt);
    void pushToken(const std::string& token);        // called from worker
    void finishStream();

private:
    QPlainTextEdit* m_out;
    int64_t          m_reqId{0};
    std::string         m_buffer;
};


