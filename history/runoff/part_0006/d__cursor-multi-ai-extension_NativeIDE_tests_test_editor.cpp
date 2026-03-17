#include <gtest/gtest.h>
#include "../src/text_buffer.cpp"
#include "../src/syntax_highlighter.cpp"

class TextBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<TextBuffer>();
    }
    
    std::unique_ptr<TextBuffer> buffer;
};

TEST_F(TextBufferTest, BasicOperations) {
    // Test setting and getting text
    buffer->SetText(L"Hello World");
    EXPECT_EQ(buffer->GetLineCount(), 1);
    EXPECT_EQ(buffer->GetLine(0), L"Hello World");
    
    // Test line operations
    buffer->InsertLine(1, L"Second line");
    EXPECT_EQ(buffer->GetLineCount(), 2);
    EXPECT_EQ(buffer->GetLine(1), L"Second line");
}

TEST_F(TextBufferTest, UndoRedo) {
    buffer->SetText(L"Initial text");
    
    // Insert some text
    buffer->InsertText(0, 12, L" and more");
    EXPECT_EQ(buffer->GetLine(0), L"Initial text and more");
    
    // Undo the insertion
    EXPECT_TRUE(buffer->Undo());
    EXPECT_EQ(buffer->GetLine(0), L"Initial text");
    
    // Redo the insertion
    EXPECT_TRUE(buffer->Redo());
    EXPECT_EQ(buffer->GetLine(0), L"Initial text and more");
}

TEST_F(TextBufferTest, SearchOperations) {
    buffer->SetText(L"The quick brown fox jumps over the lazy dog");
    
    auto results = buffer->FindAll(L"the", false, false);
    EXPECT_EQ(results.size(), 2);  // "The" and "the"
    
    results = buffer->FindAll(L"the", true, false);
    EXPECT_EQ(results.size(), 1);  // Only "the" (case sensitive)
}

class SyntaxHighlighterTest : public ::testing::Test {
protected:
    void SetUp() override {
        highlighter = std::make_unique<SyntaxHighlighter>();
    }
    
    std::unique_ptr<SyntaxHighlighter> highlighter;
};

TEST_F(SyntaxHighlighterTest, LanguageDetection) {
    highlighter->SetLanguage("cpp");
    EXPECT_EQ(highlighter->GetLanguage(), "cpp");
    
    auto supportedLanguages = highlighter->GetSupportedLanguages();
    EXPECT_GT(supportedLanguages.size(), 0);
}

TEST_F(SyntaxHighlighterTest, CppHighlighting) {
    highlighter->SetLanguage("cpp");
    
    std::wstring code = L"int main() { return 0; }";
    auto tokens = highlighter->HighlightLine(code);
    
    EXPECT_GT(tokens.size(), 0);
    
    // Should have keywords, operators, etc.
    bool hasKeyword = false;
    for (const auto& token : tokens) {
        if (token.type == SyntaxToken::Keyword) {
            hasKeyword = true;
            break;
        }
    }
    EXPECT_TRUE(hasKeyword);
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}