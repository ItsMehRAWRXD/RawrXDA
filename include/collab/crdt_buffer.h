#ifndef CRDT_BUFFER_H
#define CRDT_BUFFER_H

// C++20, no Qt. CRDT text buffer; callbacks replace signals.

#include <string>
#include <functional>

class CRDTBuffer
{
public:
    using TextChangedFn    = std::function<void(const std::string& newText)>;
    using OperationGeneratedFn = std::function<void(const std::string& operation)>;

    CRDTBuffer() = default;

    void setOnTextChanged(TextChangedFn f)         { m_onTextChanged = std::move(f); }
    void setOnOperationGenerated(OperationGeneratedFn f) { m_onOperationGenerated = std::move(f); }

    void applyRemoteOperation(const std::string& operation);
    std::string getText() const { return m_text; }
    void insertText(int position, const std::string& text);
    void deleteText(int position, int length);

private:
    std::string m_text;
    TextChangedFn    m_onTextChanged;
    OperationGeneratedFn m_onOperationGenerated;
};

#endif // CRDT_BUFFER_H
